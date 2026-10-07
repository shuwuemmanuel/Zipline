cmake_minimum_required(VERSION 3.16)
project(Zipline VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# Vendored LZMA SDK (LZMA2 + AES + SHA-256), compiled natively for the engine.
# ---------------------------------------------------------------------------
set(LZMA_DIR ${CMAKE_SOURCE_DIR}/third_party/lzma)
set(LZMA_SOURCES
    ${LZMA_DIR}/LzmaDec.c ${LZMA_DIR}/Lzma2Dec.c
    ${LZMA_DIR}/LzmaEnc.c ${LZMA_DIR}/Lzma2Enc.c
    ${LZMA_DIR}/LzFind.c ${LZMA_DIR}/LzFindMt.c ${LZMA_DIR}/LzFindOpt.c
    ${LZMA_DIR}/Threads.c ${LZMA_DIR}/MtCoder.c ${LZMA_DIR}/MtDec.c
    ${LZMA_DIR}/7zStream.c ${LZMA_DIR}/Alloc.c ${LZMA_DIR}/CpuArch.c
    ${LZMA_DIR}/7zCrc.c ${LZMA_DIR}/7zCrcOpt.c
    ${LZMA_DIR}/Sha256.c ${LZMA_DIR}/Sha256Opt.c
    ${LZMA_DIR}/Aes.c ${LZMA_DIR}/AesOpt.c ${LZMA_DIR}/SwapBytes.c)

add_library(lzma_sdk STATIC ${LZMA_SOURCES})
target_include_directories(lzma_sdk PUBLIC ${LZMA_DIR})
find_package(Threads REQUIRED)
target_link_libraries(lzma_sdk PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Engine (archive manager + 7z writer/reader + SFX builder + shared codec)
# ---------------------------------------------------------------------------
add_library(zipline_engine STATIC
    core/archive_manager.cpp
    core/sevenzip_writer.c
    core/sevenzip_reader.cpp
    core/sfx_builder.c
    sfx/sfx_codec_dec.c
    sfx/sfx_codec_enc.c
    sfx/stub_payload.c
    sfx/stub_settings.c)
target_include_directories(zipline_engine PUBLIC
    ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/sfx)
target_link_libraries(zipline_engine PUBLIC lzma_sdk)

# libarchive
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBARCHIVE libarchive)
endif()
if(LIBARCHIVE_FOUND)
    target_include_directories(zipline_engine PUBLIC ${LIBARCHIVE_INCLUDE_DIRS})
    target_link_libraries(zipline_engine PUBLIC ${LIBARCHIVE_LIBRARIES})
    target_link_directories(zipline_engine PUBLIC ${LIBARCHIVE_LIBRARY_DIRS})
else()
    find_path(ARCHIVE_INCLUDE archive.h)
    find_library(ARCHIVE_LIB archive)
    if(NOT ARCHIVE_LIB)
        message(FATAL_ERROR "libarchive not found. Install libarchive-dev / libarchive.")
    endif()
    target_include_directories(zipline_engine PUBLIC ${ARCHIVE_INCLUDE})
    target_link_libraries(zipline_engine PUBLIC ${ARCHIVE_LIB})
endif()

# ---------------------------------------------------------------------------
# Self-extracting stub: cross-compiled to a Windows PE with MinGW when present.
# The resulting sfx_stub.exe is embedded next to the app at runtime.
# ---------------------------------------------------------------------------
find_program(MINGW_CC NAMES x86_64-w64-mingw32-gcc-posix x86_64-w64-mingw32-gcc)
find_program(MINGW_WINDRES NAMES x86_64-w64-mingw32-windres)

if(MINGW_CC AND MINGW_WINDRES)
    set(STUB_OUT ${CMAKE_BINARY_DIR}/sfx_stub.exe)
    set(STUB_RES ${CMAKE_BINARY_DIR}/stub_res.o)
    set(STUB_DEC
        ${LZMA_DIR}/LzmaDec.c ${LZMA_DIR}/Lzma2Dec.c ${LZMA_DIR}/Alloc.c
        ${LZMA_DIR}/CpuArch.c ${LZMA_DIR}/7zCrc.c ${LZMA_DIR}/7zCrcOpt.c
        ${LZMA_DIR}/Sha256.c ${LZMA_DIR}/Sha256Opt.c ${LZMA_DIR}/Aes.c
        ${LZMA_DIR}/AesOpt.c ${LZMA_DIR}/SwapBytes.c)
    add_custom_command(
        OUTPUT ${STUB_OUT}
        COMMAND ${MINGW_WINDRES} -I${CMAKE_SOURCE_DIR}/sfx
                ${CMAKE_SOURCE_DIR}/sfx/stub.rc -O coff -o ${STUB_RES}
        COMMAND ${MINGW_CC} -O2 -DSFX_DECODE_ONLY -D_7ZIP_ST
                -I${CMAKE_SOURCE_DIR}/sfx -I${LZMA_DIR}
                ${CMAKE_SOURCE_DIR}/sfx/stub_main.c
                ${CMAKE_SOURCE_DIR}/sfx/stub_payload.c
                ${CMAKE_SOURCE_DIR}/sfx/stub_settings.c
                ${CMAKE_SOURCE_DIR}/sfx/stub_extract.c
                ${CMAKE_SOURCE_DIR}/sfx/sfx_codec_dec.c
                ${STUB_DEC} ${STUB_RES}
                -o ${STUB_OUT}
                -lole32 -luuid -lshell32 -lshlwapi -lcomctl32 -ladvapi32
                -lgdi32 -luser32 -lcomdlg32 -mwindows -static -static-libgcc
        DEPENDS sfx/stub_main.c sfx/stub_payload.c sfx/stub_settings.c
                sfx/stub_extract.c sfx/sfx_codec_dec.c sfx/stub.rc
                sfx/stub_icon.ico sfx/stub.manifest
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/sfx
        COMMENT "Building Windows SFX stub with MinGW")
    add_custom_target(sfx_stub ALL DEPENDS ${STUB_OUT})
else()
    message(WARNING
        "MinGW cross-compiler not found; the SFX stub will not be built. "
        "EXE (self-extracting) creation needs sfx_stub.exe (set ZIPLINE_SFX_STUB). "
        "Install mingw-w64 to enable it.")
endif()

# ---------------------------------------------------------------------------
# Qt GUI application
# ---------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
find_package(Qt6 QUIET COMPONENTS Widgets)

if(Qt6_FOUND)
    add_executable(zipline
        main.cpp
        ui/main_window.cpp
        ui/toolbar.cpp
        ui/menu_bar.cpp
        ui/file_list.cpp
        ui/archive_settings.cpp
        ui/exe_settings_dialog.cpp
        ui/collapsible_widget.cpp)
    target_include_directories(zipline PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(zipline PRIVATE zipline_engine Qt6::Widgets)
    if(TARGET sfx_stub)
        add_dependencies(zipline sfx_stub)
    endif()
else()
    message(WARNING "Qt6 Widgets not found; the GUI app will not be built. "
                    "Install qt6-base-dev. The engine and tests still build.")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(engine_tests tests/engine_smoke.cpp)
target_link_libraries(engine_tests PRIVATE zipline_engine)
target_include_directories(engine_tests PRIVATE ${CMAKE_SOURCE_DIR})
if(TARGET sfx_stub)
    add_dependencies(engine_tests sfx_stub)
endif()
add_test(NAME engine_roundtrip
         COMMAND engine_tests ${CMAKE_SOURCE_DIR}/test_files ${CMAKE_BINARY_DIR}/test_work)
set_tests_properties(engine_roundtrip PROPERTIES
    ENVIRONMENT "ZIPLINE_SFX_STUB=${CMAKE_BINARY_DIR}/sfx_stub.exe")

# Wine integration test for the self-extracting installer (skips if no Wine/stub).
find_program(WINE_EXE NAMES wine)
if(TARGET zipline AND TARGET sfx_stub AND WINE_EXE)
    add_test(NAME sfx_wine
             COMMAND ${CMAKE_COMMAND} -E env
                     bash ${CMAKE_SOURCE_DIR}/tests/sfx_wine_test.sh
                     $<TARGET_FILE:zipline>
                     ${CMAKE_BINARY_DIR}/sfx_stub.exe
                     ${CMAKE_SOURCE_DIR}/test_files
                     ${CMAKE_BINARY_DIR}/wine_work
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(sfx_wine PROPERTIES TIMEOUT 240)
endif()
