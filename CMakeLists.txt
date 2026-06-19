cmake_minimum_required(VERSION 3.20)
project(PiiScanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg integration (set VCPKG_ROOT before cmake configure)
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# -------------------------
# Dependencies
# -------------------------
find_package(unofficial-libxlsxwriter CONFIG REQUIRED)

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
# Link libraries
# -------------------------
target_link_libraries(PiiScanner PRIVATE
    unofficial::libxlsxwriter::libxlsxwriter
    # Windows system libraries
    pathcch
    shlwapi
    ole32
    oleaut32
    query          # IFilter/ISearch
    windowsapp     # WinRT (Windows.Media.Ocr)
)

# -------------------------
# Windows-specific settings
# -------------------------
if(MSVC)
    target_compile_options(PiiScanner PRIVATE
        /W4
        /wd4100   # unreferenced formal parameter (common in stubs)
        /wd4505   # unreferenced function
        /utf-8    # source/exec charset UTF-8
    )
    # Enable C++/WinRT
    target_compile_options(PiiScanner PRIVATE /await)
    # Define Windows version for WinRT
    target_compile_definitions(PiiScanner PRIVATE
        _WIN32_WINNT=0x0A00    # Windows 10
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    # Subsystem: console
    set_target_properties(PiiScanner PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
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

message(STATUS "PiiScanner configured. Build with: cmake --build . --config Release")
