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


#################################### Targets ####################################
if(MSVC)
  # not making any difference on Windows
  # and helps Visual Studio to parse it correctly as a target
  set(LibraryType SHARED)
else()
  set(LibraryType MODULE)
endif(MSVC)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/index_html.cpp"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/Source/index.html
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/index_html.cpp
        -DSYMBOL=indexHtml
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake
    VERBATIM)
add_library(MonitoringEngine ${LibraryType}
  Source/MonitoringEngine.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/index_html.cpp)

set_property(TARGET MonitoringEngine PROPERTY CXX_STANDARD 23)
if(MSVC)
  target_compile_options(MonitoringEngine PUBLIC "/Zc:__cplusplus")
  target_compile_options(MonitoringEngine PUBLIC "/EHsc")
endif(MSVC)
target_include_directories(MonitoringEngine SYSTEM PUBLIC ${MonitoringEngine_BINARY_DIR}/deps/include)
add_dependencies(MonitoringEngine BOSS)

set_target_properties(MonitoringEngine PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
install(TARGETS MonitoringEngine LIBRARY DESTINATION lib)
