cmake_minimum_required(VERSION 3.20)
project(PiiScanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NOTE: No vcpkg / no external packages required.
# xlsx_writer.h is a zero-dependency header-only xlsx writer (ZIP STORED + XML).

# -------------------------
# Source files
# -------------------------
set(SOURCES
    src/main.cpp
    src/everything_scanner.cpp
    src/text_extractor.cpp
    src/pii_detector.cpp
    src/reporter.cpp
)

add_executable(PiiScanner ${SOURCES})

# -------------------------
# Include directories
# -------------------------
target_include_directories(PiiScanner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -------------------------
# Link libraries  (Windows SDK only, no external packages)
# -------------------------
target_link_libraries(PiiScanner PRIVATE
    pathcch
    shlwapi
    ole32
    oleaut32
    query          # IFilter/ISearch
    windowsapp     # WinRT (Windows.Media.Ocr)
    shell32
)

# -------------------------
# Windows-specific settings
# -------------------------
if(MSVC)
    target_compile_options(PiiScanner PRIVATE
        /W3
        /wd4100   # unreferenced formal parameter
        /wd4505   # unreferenced local function
        /utf-8    # source/exec charset UTF-8
        # /await removed: use /std:c++20 for native coroutine support
    )
    target_compile_definitions(PiiScanner PRIVATE
        _WIN32_WINNT=0x0A00    # Windows 10+
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    # Static CRT – no MSVCRT.dll dependency
    set_property(TARGET PiiScanner PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # Subsystem: console
    set_target_properties(PiiScanner PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# -------------------------
# Post-build: copy Everything64.dll
# -------------------------
add_custom_command(TARGET PiiScanner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/Everything64.dll"
        "$<TARGET_FILE_DIR:PiiScanner>/Everything64.dll"
    COMMENT "Copying Everything64.dll to output directory"
)

message(STATUS "PiiScanner: zero external dependencies. Build with: cmake --build . --config Release")
message(STATUS "  Or use build.bat for a direct cl.exe build without CMake.")
