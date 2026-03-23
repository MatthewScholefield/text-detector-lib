# FindLeptonica.cmake
# Find Leptonica library and headers

find_path(Leptonica_INCLUDE_DIR
    NAMES leptonica/allheaders.h
    PATHS
        /usr/include/leptonica
        /usr/local/include/leptonica
        /opt/include/leptonica
        /usr/include
        /usr/local/include
    DOC "Leptonica include directory"
)

find_library(Leptonica_LIBRARY
    NAMES lept
    PATHS
        /usr/lib64
        /usr/lib
        /usr/local/lib64
        /usr/local/lib
        /opt/lib64
        /opt/lib
    DOC "Leptonica library"
)

if(Leptonica_INCLUDE_DIR AND Leptonica_LIBRARY)
    set(Leptonica_FOUND TRUE)
    set(Leptonica_INCLUDE_DIRS ${Leptonica_INCLUDE_DIR})
    set(Leptonica_LIBRARIES ${Leptonica_LIBRARY})

    if(NOT Leptonica_FIND_QUIETLY)
        message(STATUS "Found Leptonica: ${Leptonica_LIBRARY}")
    endif()
else()
    set(Leptonica_FOUND FALSE)
    if(Leptonica_FIND_REQUIRED)
        message(FATAL_ERROR
            "Leptonica not found. Please install the development package:\n"
            "  Fedora/RHEL: sudo dnf install leptonica-devel\n"
            "  Debian/Ubuntu: sudo apt-get install liblept-dev\n"
            "  Arch: sudo pacman -S leptonica")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Leptonica
    REQUIRED_VARS Leptonica_LIBRARY Leptonica_INCLUDE_DIR
)

mark_as_advanced(Leptonica_INCLUDE_DIR Leptonica_LIBRARY)
