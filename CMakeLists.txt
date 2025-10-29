cmake_minimum_required(VERSION 3.16)

project(cross_cpp_demo VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure proper compiler detection
if(NOT CMAKE_CXX_COMPILER_ID)
    enable_language(CXX)
endif()

# Platform-specific settings
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    # Detect current architecture and use it
    execute_process(COMMAND uname -m OUTPUT_VARIABLE CURRENT_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(CURRENT_ARCH STREQUAL "arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64")
    endif()
    # Use the latest available SDK
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE MACOS_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(MACOS_SDK_PATH)
        set(CMAKE_OSX_SYSROOT ${MACOS_SDK_PATH})
        # Add C++ standard library include path
        include_directories(${MACOS_SDK_PATH}/usr/include/c++/v1)
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux/Raspberry Pi settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# OpenCV support (required)
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV found: ${OpenCV_VERSION}")
add_definitions(-DENABLE_OPENCV)

# Note: Camera uses rpicam-vid pipe - no additional libraries needed!
# Just need rpicam-apps installed on Raspberry Pi: sudo apt install rpicam-apps

# JSON library (nlohmann/json - header only)
# Try to find system package first, fallback to FetchContent
find_package(nlohmann_json 3.11.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# OpenMP support (optional, for parallel processing)
if(APPLE)
    # On macOS, help CMake find libomp installed via Homebrew
    set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
    set(OpenMP_C_LIB_NAMES "omp")
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/Cellar/libomp/21.1.4/lib/libomp.dylib")
    # Also check /usr/local for Intel Macs
    if(NOT EXISTS "/opt/homebrew/Cellar/libomp")
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I/usr/local/opt/libomp/include")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I/usr/local/opt/libomp/include")
        set(OpenMP_omp_LIBRARY "/usr/local/opt/libomp/lib/libomp.dylib")
    endif()
endif()

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
    # Temporarily disable OpenMP due to linking issues on macOS
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    message(STATUS "OpenMP linking disabled due to macOS compatibility issues")
else()
    message(STATUS "OpenMP not found, parallel processing will be disabled")
endif()

# Include directories
include_directories(include)

# FlatBuffers support
find_package(Flatbuffers REQUIRED)
if(Flatbuffers_FOUND)
    message(STATUS "FlatBuffers found: ${Flatbuffers_VERSION}")
    add_definitions(-DENABLE_FLATBUFFERS)
else()
    message(STATUS "FlatBuffers not found, falling back to simple UDP protocol")
endif()

# Collect all source files
set(SOURCES
    src/main.cpp
    src/core/Config.cpp
    src/core/ImageFrameSource.cpp
    src/core/CameraFrameSource.cpp
    src/core/LEDController.cpp
    src/processing/BezierCurve.cpp
    src/processing/CoonsPatching.cpp
    src/processing/ColorExtractor.cpp
    src/communication/LEDLayout.cpp
    src/communication/HyperHDRClient.cpp
)

# Add executables
add_executable(app ${SOURCES})

# Link OpenCV
target_link_libraries(app ${OpenCV_LIBS})
target_include_directories(app PRIVATE ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(app PRIVATE ENABLE_OPENCV)

# Link JSON library
target_link_libraries(app nlohmann_json::nlohmann_json)

# Link FlatBuffers if found
if(Flatbuffers_FOUND)
    target_link_libraries(app flatbuffers::flatbuffers)
endif()

# Set output directory
set_target_properties(app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Print configuration summary
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
