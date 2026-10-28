# Stockfish 19 für Android. Die C++-Quellen kommen unverändert aus dem iOS-Package
# (../src); hier liegen nur die JNI-Brücke und der
# Android-Architektur-Header. Flags wie in Package.swift.
cmake_minimum_required(VERSION 3.22.1)
project(chesswow_stockfish CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT STOCKFISH_DIR)
    get_filename_component(STOCKFISH_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE)
endif()
if(NOT EXISTS "${STOCKFISH_DIR}/bridge.cpp")
    message(FATAL_ERROR "Stockfish sources not found at ${STOCKFISH_DIR}")
endif()

file(GLOB STOCKFISH_SOURCES CONFIGURE_DEPENDS
    "${STOCKFISH_DIR}/*.cpp"
    "${STOCKFISH_DIR}/nnue/*.cpp"
    "${STOCKFISH_DIR}/nnue/features/*.cpp"
    "${STOCKFISH_DIR}/syzygy/*.cpp")
# main.cpp wird durch bridge.cpp ersetzt.
list(FILTER STOCKFISH_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(chesswow_stockfish SHARED ${STOCKFISH_SOURCES} jni_bridge.cpp)

target_include_directories(chesswow_stockfish PRIVATE "${STOCKFISH_DIR}" "${STOCKFISH_DIR}/include")

target_compile_definitions(chesswow_stockfish PRIVATE NDEBUG NNUE_EMBEDDING_OFF USE_PTHREADS)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(chesswow_stockfish PRIVATE IS_64BIT)
endif()

# -O3 nach den Build-Typ-Flags, damit auch Debug-Builds eine schnelle Engine haben.
target_compile_options(chesswow_stockfish PRIVATE
    -O3 -funroll-loops -fno-exceptions
    -fvisibility=hidden -fvisibility-inlines-hidden
    -ffunction-sections -fdata-sections
    -Wno-everything
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/android_arch.h")

if(CHESSWOW_DOTPROD AND ANDROID_ABI STREQUAL "arm64-v8a")
    target_compile_options(chesswow_stockfish PRIVATE -march=armv8.2-a+dotprod)
endif()

target_link_options(chesswow_stockfish PRIVATE
    "-Wl,-z,max-page-size=16384"
    "-Wl,--gc-sections"
    "-Wl,--exclude-libs,ALL")

target_link_libraries(chesswow_stockfish PRIVATE log)
