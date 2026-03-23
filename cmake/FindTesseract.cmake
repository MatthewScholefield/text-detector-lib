# FindTesseract.cmake
# Find Tesseract OCR library and headers

find_path(Tesseract_INCLUDE_DIR
    NAMES tesseract/baseapi.h
    PATHS
        /usr/include/tesseract
        /usr/local/include/tesseract
        /opt/include/tesseract
        /usr/include
        /usr/local/include
    DOC "Tesseract include directory"
)

find_library(Tesseract_LIBRARY
    NAMES tesseract
    PATHS
        /usr/lib64
        /usr/lib
        /usr/local/lib64
        /usr/local/lib
        /opt/lib64
        /opt/lib
    DOC "Tesseract library"
)

if(Tesseract_INCLUDE_DIR AND Tesseract_LIBRARY)
    set(Tesseract_FOUND TRUE)
    set(Tesseract_INCLUDE_DIRS ${Tesseract_INCLUDE_DIR})
    set(Tesseract_LIBRARIES ${Tesseract_LIBRARY})

    # Get version
    if(EXISTS "${Tesseract_INCLUDE_DIR}/tesseract/version.h")
        file(READ "${Tesseract_INCLUDE_DIR}/tesseract/version.h" _version_h)
        string(REGEX MATCH "define TESSERACT_VERSION_STR \"([0-9.]+)\"" _version_match "${_version_h}")
        if(CMAKE_MATCH_1)
            set(Tesseract_VERSION ${CMAKE_MATCH_1})
        endif()
    endif()

    if(NOT Tesseract_FIND_QUIETLY)
        message(STATUS "Found Tesseract: ${Tesseract_LIBRARY} (version ${Tesseract_VERSION})")
    endif()
else()
    set(Tesseract_FOUND FALSE)
    if(Tesseract_FIND_REQUIRED)
        message(FATAL_ERROR
            "Tesseract not found. Please install the development package:\n"
            "  Fedora/RHEL: sudo dnf install tesseract-devel leptonica-devel\n"
            "  Debian/Ubuntu: sudo apt-get install libtesseract-dev liblept-dev\n"
            "  Arch: sudo pacman -S tesseract leptonica")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tesseract
    REQUIRED_VARS Tesseract_LIBRARY Tesseract_INCLUDE_DIR
    VERSION_VAR Tesseract_VERSION
)

mark_as_advanced(Tesseract_INCLUDE_DIR Tesseract_LIBRARY)
