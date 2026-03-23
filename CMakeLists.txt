cmake_minimum_required(VERSION 3.15)
project(textdetector VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Model dimensions (configurable)
set(TEXT_DETECTOR_INPUT_WIDTH 256 CACHE STRING "Model input width")
set(TEXT_DETECTOR_INPUT_HEIGHT 256 CACHE STRING "Model input height")

# Validate dimensions
if(NOT TEXT_DETECTOR_INPUT_WIDTH MATCHES "^[0-9]+$" OR NOT TEXT_DETECTOR_INPUT_HEIGHT MATCHES "^[0-9]+$")
    message(FATAL_ERROR "TEXT_DETECTOR_INPUT_WIDTH and TEXT_DETECTOR_INPUT_HEIGHT must be positive integers")
endif()

# Check if divisible by 4
math(EXPR WIDTH_MOD "${TEXT_DETECTOR_INPUT_WIDTH} % 4")
math(EXPR HEIGHT_MOD "${TEXT_DETECTOR_INPUT_HEIGHT} % 4")
if(NOT WIDTH_MOD EQUAL 0 OR NOT HEIGHT_MOD EQUAL 0)
    message(FATAL_ERROR "TEXT_DETECTOR_INPUT_WIDTH and TEXT_DETECTOR_INPUT_HEIGHT must be multiples of 4")
endif()

# Configure model config header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/text_detector_model_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/text_detector_model_config.h
    @ONLY
)

# Options
option(BUILD_TOOLS "Build command-line tools" ON)
option(BUILD_TESTS "Build tests" OFF)
option(INSTALL_CMAKEDIR "Install CMake config files for find_package()" ON)

# Find dependencies
find_package(PNG REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(Tesseract REQUIRED tesseract)
pkg_check_modules(Leptonica REQUIRED lept)

# Include directories
include_directories(
    ${PNG_INCLUDE_DIRS}
    ${Tesseract_INCLUDE_DIRS}
    ${Leptonica_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated config header
)

# Main library
add_subdirectory(src)

# Tools
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Installation
include(GNUInstallDirs)

# Install library
install(TARGETS textdetector
    EXPORT textdetectorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install public headers
install(FILES
    text_detector.h
    ${CMAKE_CURRENT_BINARY_DIR}/text_detector_model_config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install CMake config files
if(INSTALL_CMAKEDIR)
    include(CMakePackageConfigHelpers)

    # Create config file
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/textdetector-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/textdetector-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/textdetector
    )

    # Create version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/textdetector-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/textdetector-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/textdetector-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/textdetector
    )

    # Install the targets
    install(
        EXPORT textdetectorTargets
        FILE textdetector-targets.cmake
        NAMESPACE textdetector::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/textdetector
    )
endif()

# Print summary
message(STATUS "")
message(STATUS "Text Detector Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Input dimensions: ${TEXT_DETECTOR_INPUT_WIDTH}x${TEXT_DETECTOR_INPUT_HEIGHT}")
message(STATUS "  Build tools: ${BUILD_TOOLS}")
message(STATUS "  Install CMake: ${INSTALL_CMAKEDIR}")
message(STATUS "")
