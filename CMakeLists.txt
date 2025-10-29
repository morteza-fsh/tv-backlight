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

# FlatBuffers support (REQUIRED - try different package name variants)
find_package(flatbuffers QUIET)
if(NOT flatbuffers_FOUND)
    find_package(Flatbuffers QUIET)
endif()

set(FLATBUFFERS_FOUND ${flatbuffers_FOUND})
if(NOT FLATBUFFERS_FOUND AND Flatbuffers_FOUND)
    set(FLATBUFFERS_FOUND ${Flatbuffers_FOUND})
endif()

if(NOT FLATBUFFERS_FOUND)
    message(FATAL_ERROR 
        "FlatBuffers not found! This is a required dependency.\n"
        "Please install FlatBuffers:\n"
        "  macOS: brew install flatbuffers\n"
        "  Linux: sudo apt install flatbuffers-compiler libflatbuffers-dev\n"
    )
endif()

message(STATUS "FlatBuffers package found")

# Find flatc compiler (REQUIRED)
find_program(FLATC_EXECUTABLE NAMES flatc)
if(NOT FLATC_EXECUTABLE)
    message(FATAL_ERROR 
        "flatc compiler not found! This is required to build the project.\n"
        "Please install FlatBuffers compiler:\n"
        "  macOS: brew install flatbuffers\n"
        "  Linux: sudo apt install flatbuffers-compiler\n"
    )
endif()

message(STATUS "flatc compiler found: ${FLATC_EXECUTABLE}")

# Define schema files
set(FLATBUFFER_SCHEMAS
    ${CMAKE_SOURCE_DIR}/schemas/hyperion_request.fbs
    ${CMAKE_SOURCE_DIR}/schemas/hyperion_reply.fbs
)

# Define output directory for generated headers
set(FLATBUFFER_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/include/flatbuffer)
file(MAKE_DIRECTORY ${FLATBUFFER_OUTPUT_DIR})

# Generate headers immediately during CMake configuration
foreach(SCHEMA ${FLATBUFFER_SCHEMAS})
    get_filename_component(SCHEMA_NAME ${SCHEMA} NAME_WE)
    set(GENERATED_HEADER ${FLATBUFFER_OUTPUT_DIR}/${SCHEMA_NAME}_generated.h)
    
    # Check if header needs to be regenerated
    if(NOT EXISTS ${GENERATED_HEADER} OR ${SCHEMA} IS_NEWER_THAN ${GENERATED_HEADER})
        message(STATUS "Generating FlatBuffer header: ${SCHEMA_NAME}_generated.h")
        execute_process(
            COMMAND ${FLATC_EXECUTABLE} --cpp -o ${FLATBUFFER_OUTPUT_DIR} ${SCHEMA}
            RESULT_VARIABLE FLATC_RESULT
            OUTPUT_VARIABLE FLATC_OUTPUT
            ERROR_VARIABLE FLATC_ERROR
        )
        if(NOT FLATC_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to generate ${SCHEMA_NAME}_generated.h: ${FLATC_ERROR}")
        endif()
        message(STATUS "Successfully generated ${SCHEMA_NAME}_generated.h")
    else()
        message(STATUS "FlatBuffer header ${SCHEMA_NAME}_generated.h is up to date")
    endif()
endforeach()

# Create a custom target for flatbuffer generation (for rebuilds)
add_custom_target(generate_flatbuffers
    COMMAND ${CMAKE_COMMAND} -E echo "FlatBuffer headers already generated during configuration"
    COMMENT "FlatBuffer headers are generated during CMake configuration"
)

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

# Make sure flatbuffer headers are generated before building the app
add_dependencies(app generate_flatbuffers)

# Link OpenCV
target_link_libraries(app ${OpenCV_LIBS})
target_include_directories(app PRIVATE ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(app PRIVATE ENABLE_OPENCV)

# Link JSON library
target_link_libraries(app nlohmann_json::nlohmann_json)

# Link FlatBuffers (required)
# Try to link with the correct library name variant
if(TARGET flatbuffers::flatbuffers)
    target_link_libraries(app flatbuffers::flatbuffers)
elseif(TARGET Flatbuffers::flatbuffers)
    target_link_libraries(app Flatbuffers::flatbuffers)
else()
    # Fallback: try to find the library manually
    find_library(FLATBUFFERS_LIBRARY NAMES flatbuffers)
    if(FLATBUFFERS_LIBRARY)
        target_link_libraries(app ${FLATBUFFERS_LIBRARY})
    else()
        message(FATAL_ERROR "FlatBuffers library not found for linking!")
    endif()
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
