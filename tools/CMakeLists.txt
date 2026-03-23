# CLI tool executable
add_executable(text-detector-cli text-detector-cli.c)

# Link against the library
target_link_libraries(text-detector-cli
    PRIVATE
        textdetector
)

# Include directories
target_include_directories(text-detector-cli
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Installation
install(TARGETS text-detector-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
