cmake_minimum_required(VERSION 4.3)
project(MonitoringEngine)

############################## Custom build modes ###############################

set(CMAKE_CXX_FLAGS_SANITIZE "-fno-omit-frame-pointer -fsanitize=address,signed-integer-overflow,null,alignment,bounds,function,return,vla-bound -O0 -g"
  CACHE STRING
  "Flags used by the C++ compiler during Sanitize builds."
  FORCE)
set(CMAKE_C_FLAGS_SANITIZE "-fno-omit-frame-pointer -fsanitize=address,signed-integer-overflow,null,alignment,bounds,function,return,vla-bound -O0 -g"
  CACHE STRING
  "Flags used by the C compiler during Sanitize builds."
  FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SANITIZE
  ${CMAKE_EXE_LINKER_FLAGS_DEBUG} CACHE STRING
  "Flags used for linking binaries during Sanitize builds."
  FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_SANITIZE
  ${CMAKE_SHARED_LINKER_FLAGS_DEBUG} CACHE STRING
  "Flags used by the shared libraries linker during Sanitize builds."
  FORCE)
mark_as_advanced(
  CMAKE_CXX_FLAGS_SANITIZE		  CMAKE_EXE_LINKER_FLAGS_SANITIZE
  CMAKE_C_FLAGS_SANITIZE		  CMAKE_SHARED_LINKER_FLAGS_SANITIZE
  )

set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
  "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Sanitize."
  FORCE)

############################### External Projects ###############################

include(ExternalProject)
set(BOSS_SOURCE_BRANCH mcp-tool CACHE STRING "if boss core is built from source, which branch should be fetched")
set(BOSS_SOURCE_REPOSITORY https://github.com/symbol-store/BOSS.git CACHE STRING "url for the boss repository")
ExternalProject_Add(BOSS
  GIT_REPOSITORY ${BOSS_SOURCE_REPOSITORY}
  DOWNLOAD_DIR /Users/hlgr/.cmake-downloads/${CMAKE_PROJECT_NAME}
  GIT_TAG ${BOSS_SOURCE_BRANCH}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${MonitoringEngine_BINARY_DIR}/deps $<IF:$<CONFIG:>,,-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}> -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
  )

# Nested emcmake build of the browser BOSS (Source/Wasm). Its boss.js/boss.wasm/
# boss.data are embedded into libMonitoringEngine (below); BUILD_BYPRODUCTS lets
# the embed steps depend on them. emcc is required.
find_program(EMCMAKE_PROGRAM emcmake REQUIRED)
ExternalProject_Get_Property(BOSS SOURCE_DIR BINARY_DIR)
set(BOSS_WASM_SRC_DIR    "${SOURCE_DIR}")
set(BOSS_WASM_NATIVE_DIR "${BINARY_DIR}")
set(BOSS_WASM_BUILD_DIR  "${CMAKE_CURRENT_BINARY_DIR}/Wasm")
ExternalProject_Add(BOSSWasm
  DEPENDS BOSS
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source/Wasm
  BINARY_DIR ${BOSS_WASM_BUILD_DIR}
  CONFIGURE_COMMAND ${EMCMAKE_PROGRAM} ${CMAKE_COMMAND}
      -S <SOURCE_DIR> -B <BINARY_DIR>
      -DCMAKE_BUILD_TYPE=Release
      -DBOSS_SOURCE_DIR=${BOSS_WASM_SRC_DIR}
      -DBOSS_NATIVE_BUILD_DIR=${BOSS_WASM_NATIVE_DIR}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
  BUILD_ALWAYS TRUE
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS
      ${BOSS_WASM_BUILD_DIR}/boss.js
      ${BOSS_WASM_BUILD_DIR}/boss.wasm
      ${BOSS_WASM_BUILD_DIR}/boss.data)


#################################### Targets ####################################
if(MSVC)
  # not making any difference on Windows
  # and helps Visual Studio to parse it correctly as a target
  set(LibraryType SHARED)
else()
  set(LibraryType MODULE)
endif(MSVC)

# Embed a file into a .cpp compiled into the library: text (.html/.js/...) as a
# string, anything else as a byte array + _len. With COMPRESS, gzip-compress it
# (byte array + compressed _len + _rawlen). Returns the .cpp path in _outvar.
function(embed_file _symbol _input _outvar)
  cmake_parse_arguments(PARSE_ARGV 3 A "COMPRESS" "" "")
  set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_symbol}.cpp")
  if(A_COMPRESS)
    set(_mode -DCOMPRESS=ON)
  elseif(NOT "${_input}" MATCHES "\\.(html|js|css|txt)$")
    set(_mode -DBINARY=ON)
  else()
    set(_mode "")
  endif()
  add_custom_command(
    OUTPUT "${_out}"
    COMMAND ${CMAKE_COMMAND} -DINPUT=${_input} -DOUTPUT=${_out} -DSYMBOL=${_symbol}
            ${_mode} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake
    DEPENDS ${_input} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake
    VERBATIM)
  set(${_outvar} "${_out}" PARENT_SCOPE)
endfunction()

embed_file(indexHtml "${CMAKE_CURRENT_SOURCE_DIR}/Source/index.html" INDEX_HTML_CPP)
# boss.* are BOSSWasm's BUILD_BYPRODUCTS; add_dependencies(... BOSSWasm) below
# orders the whole library after that build.
embed_file(bossJs   "${BOSS_WASM_BUILD_DIR}/boss.js"   BOSS_JS_CPP   COMPRESS)
embed_file(bossWasm "${BOSS_WASM_BUILD_DIR}/boss.wasm" BOSS_WASM_CPP COMPRESS)
embed_file(bossData "${BOSS_WASM_BUILD_DIR}/boss.data" BOSS_DATA_CPP COMPRESS)

find_package(ZLIB REQUIRED)  # decompress the embedded artifacts at serve time
add_library(MonitoringEngine ${LibraryType}
  Source/MonitoringEngine.cpp
  ${INDEX_HTML_CPP}
  ${BOSS_JS_CPP}
  ${BOSS_WASM_CPP}
  ${BOSS_DATA_CPP})

set_property(TARGET MonitoringEngine PROPERTY CXX_STANDARD 23)
if(MSVC)
  target_compile_options(MonitoringEngine PUBLIC "/Zc:__cplusplus")
  target_compile_options(MonitoringEngine PUBLIC "/EHsc")
endif(MSVC)
target_include_directories(MonitoringEngine SYSTEM PUBLIC ${MonitoringEngine_BINARY_DIR}/deps/include)
add_dependencies(MonitoringEngine BOSS BOSSWasm)
target_link_libraries(MonitoringEngine PRIVATE ZLIB::ZLIB)

set_target_properties(MonitoringEngine PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
install(TARGETS MonitoringEngine LIBRARY DESTINATION lib)
