# Library sources
set(LIB_SOURCES
    ../text_detector.c
    image_io.c
    ocr.c
    bounding_boxes.c
)

# Post-process model.c to use configurable dimensions
set(MODEL_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/models/model.c)
set(MODEL_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/model_postprocessed.c)

add_custom_command(
    OUTPUT ${MODEL_OUTPUT}
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/../tools/postprocess_model.py
        ${MODEL_INPUT}
        ${MODEL_OUTPUT}
        --width ${TEXT_DETECTOR_INPUT_WIDTH}
        --height ${TEXT_DETECTOR_INPUT_HEIGHT}
    DEPENDS ${MODEL_INPUT} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/postprocess_model.py
    COMMENT "Post-processing model to use configurable dimensions"
)

# Add the post-processed model to sources
list(APPEND LIB_SOURCES ${MODEL_OUTPUT})

# Private headers
set(LIB_PRIVATE_HEADERS
    bounding_boxes.h
)

# Create the library
add_library(textdetector STATIC ${LIB_SOURCES})

# Set library version
set_target_properties(textdetector PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Link libraries
target_link_libraries(textdetector
    PUBLIC
        ${PNG_LIBRARIES}
        ${Tesseract_LIBRARIES}
        ${Leptonica_LIBRARIES}
        m
)

# Include directories for library targets
target_include_directories(textdetector
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>  # For generated config header
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${PNG_INCLUDE_DIRS}
        ${Tesseract_INCLUDE_DIRS}
        ${Leptonica_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}  # For internal headers
)

# Compiler flags
target_compile_options(textdetector PRIVATE
    -O3
    -ffast-math
)

# Alias for namespaced target
add_library(textdetector::textdetector ALIAS textdetector)
