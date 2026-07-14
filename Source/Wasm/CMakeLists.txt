cmake_minimum_required(VERSION 3.24)
project(BOSSWasm C CXX)

# Browser (WASM) build of BOSS: chibi-scheme + BOSS core + an Embind entry point,
# linked to boss.js/boss.wasm/boss.data. Launched by the parent CMakeLists via
# emcmake as a nested build; it reuses the native BOSS build's artifacts.
if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this with emcmake (the Emscripten toolchain).")
endif()

# Provided by the parent build (the native BOSS ExternalProject's source
# and build dirs).
set(BOSS_SOURCE_DIR "" CACHE PATH
  "BOSS source tree (Source/BOSS.cpp, Source/ExpressionParser.hpp, Source/Shims/...)")
set(BOSS_NATIVE_BUILD_DIR "" CACHE PATH
  "Native BOSS build dir providing chibi headers, sources and generated FFI bindings")
if(NOT BOSS_SOURCE_DIR OR NOT BOSS_NATIVE_BUILD_DIR)
  message(FATAL_ERROR "BOSS_SOURCE_DIR and BOSS_NATIVE_BUILD_DIR must be set for the WASM build")
endif()

# Emscripten dynamic linking rejects non-PIC objects (wasm-ld can't relocate
# against runtime symbols like __cxa_throw).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CHIBI_SRC_DIR "${BOSS_NATIVE_BUILD_DIR}/chibi-scheme-prefix/src/chibi-scheme")
set(CHIBI_INCLUDE_DIR "${CHIBI_SRC_DIR}/include")
set(CHIBI_MODULE_DIR "${CHIBI_SRC_DIR}/lib")
set(GENERATED_FFI_BINDINGS "${BOSS_NATIVE_BUILD_DIR}/Source/Shims/chibi-bindings.c")
set(HOST_CHIBI "${CHIBI_SRC_DIR}/chibi-scheme")

foreach(_required IN ITEMS
    "${CHIBI_SRC_DIR}/sexp.c" "${CHIBI_INCLUDE_DIR}/chibi/eval.h"
    "${GENERATED_FFI_BINDINGS}" "${CHIBI_MODULE_DIR}/init-7.scm" "${HOST_CHIBI}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR
      "Missing reusable native-build artifact: ${_required}\n"
      "Build the native MonitoringEngine target first (it builds BOSS).")
  endif()
endforeach()

# WASM has no dlopen, so chibi's C-backed modules (hash tables, ast, the C prims
# behind srfi-166/show, ...) must be statically registered in a generated clibs.c
# that eval.c #includes under SEXP_USE_STATIC_LIBS; chibi-genstatic (run by the
# host chibi-scheme) emits it, excluding modules needing syscalls Emscripten lacks.
#
# One line on purpose: add_custom_command collapses newlines within a COMMAND
# argument, so a multi-line body would be joined and break; the bracket argument
# keeps it verbatim (unescaped sed backslash and ${VAR:-} shell defaults).
set(CHIBI_CLIBS_C "${CMAKE_CURRENT_BINARY_DIR}/clibs.c")
set(CHIBI_GENSTATIC_SH [==[set -eu; cd "$1/lib"; find . -name '*.sld' | sed 's|^\./||' | grep -vE '^chibi/(process|pty|signal|net|stty|system|win32)' | env DYLD_LIBRARY_PATH=".:${DYLD_LIBRARY_PATH:-}" LD_LIBRARY_PATH=".:${LD_LIBRARY_PATH:-}" CHIBI_IGNORE_SYSTEM_PATH=1 CHIBI_MODULE_PATH=. "$2" -q "$1/tools/chibi-genstatic" --features chibi,emscripten,wasm32,little-endian > "$3"]==])
add_custom_command(
  OUTPUT "${CHIBI_CLIBS_C}"
  COMMAND sh -c "${CHIBI_GENSTATIC_SH}" gen-clibs
          "${CHIBI_SRC_DIR}" "${HOST_CHIBI}" "${CHIBI_CLIBS_C}"
  VERBATIM)

set(CHIBI_CORE_SOURCES gc.c sexp.c bignum.c gc_heap.c opcodes.c vm.c eval.c simplify.c)
list(TRANSFORM CHIBI_CORE_SOURCES PREPEND "${CHIBI_SRC_DIR}/")
add_library(chibi_wasm STATIC ${CHIBI_CORE_SOURCES})
# BEFORE so our install.h shadows the chibi source tree's host-generated one.
target_include_directories(chibi_wasm BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/chibi-config")
target_include_directories(chibi_wasm PUBLIC "${CHIBI_INCLUDE_DIR}")
# PRIVATE: else chibi's source root leaks to the boss target, whose libc++
# <version> include would resolve to chibi's source file named "version".
target_include_directories(chibi_wasm PRIVATE "${CHIBI_SRC_DIR}")
target_include_directories(chibi_wasm PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(chibi_wasm PUBLIC
  SEXP_USE_DL=0 SEXP_USE_STATIC_LIBS=1 SEXP_USE_STATIC_LIBS_NO_INCLUDE=0
  SEXP_USE_STRICT_TOPLEVEL_BINDINGS=1 SEXP_USE_ALIGNED_BYTECODE=1)
target_compile_options(chibi_wasm PRIVATE -O2 -Wno-implicit-function-declaration -Wno-int-conversion)

# eval.c does #include "clibs.c"; make the generator run first.
set_source_files_properties("${CHIBI_SRC_DIR}/eval.c" PROPERTIES OBJECT_DEPENDS "${CHIBI_CLIBS_C}")

add_executable(boss
  "${BOSS_SOURCE_DIR}/Source/BOSS.cpp"
  "${BOSS_SOURCE_DIR}/Source/Serialization.cpp"
  "${BOSS_SOURCE_DIR}/Source/Shims/BossPrettyPrint.cpp"
  "${GENERATED_FFI_BINDINGS}"
  "${CMAKE_CURRENT_SOURCE_DIR}/wasm-stubs.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/BOSSWasm.cpp")
set_target_properties(boss PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  OUTPUT_NAME "boss" SUFFIX ".js")
target_include_directories(boss PRIVATE "${BOSS_SOURCE_DIR}/Source" "${CHIBI_INCLUDE_DIR}")
set_source_files_properties("${GENERATED_FFI_BINDINGS}" PROPERTIES
  INCLUDE_DIRECTORIES "${BOSS_SOURCE_DIR}/Source")
target_link_libraries(boss PRIVATE chibi_wasm)

set(BOSS_WASM_ENGINE_LIBS "" CACHE STRING
  "Semicolon-separated paths to prebuilt engine SIDE_MODULE .so/.wasm files")

target_link_options(boss PRIVATE
  "-sMAIN_MODULE=1"
  "-sINITIAL_MEMORY=268435456"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=createBOSS"
  "-sEXPORTED_RUNTIME_METHODS=['FS','ENV','callMain']"
  "-sENVIRONMENT=web,worker,node"
  "-sEXIT_RUNTIME=0"
  "-lembind"
  "--preload-file" "${CHIBI_MODULE_DIR}@/chibi"
  "-fexceptions")
target_compile_options(boss PRIVATE -fexceptions)
target_link_options(boss PRIVATE ${BOSS_WASM_ENGINE_LIBS})
