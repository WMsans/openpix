cmake_minimum_required(VERSION 3.20)
project(openpix VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# --- Dependencies ---
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui DBus Concurrent)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

# onnxruntime - try pkg-config first, then find_library fallback
pkg_check_modules(ONNXRUNTIME QUIET libonnxruntime)
if(NOT ONNXRUNTIME_FOUND)
    find_library(ONNXRUNTIME_LIBRARIES NAMES onnxruntime)
    find_path(ONNXRUNTIME_INCLUDE_DIRS NAMES onnxruntime_cxx_api.h
              PATH_SUFFIXES onnxruntime/core/session onnxruntime)
    if(NOT ONNXRUNTIME_LIBRARIES)
        message(FATAL_ERROR "onnxruntime not found")
    endif()
endif()

# Create onnxruntime include wrapper for RapidOcrOnnx compatibility
# RapidOcr expects: onnxruntime/core/session/onnxruntime_cxx_api.h
# System provides: /usr/include/onnxruntime/onnxruntime_cxx_api.h
execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/onnxruntime/core/session
)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${ONNXRUNTIME_INCLUDE_DIRS}/onnxruntime_cxx_api.h
        ${CMAKE_BINARY_DIR}/onnxruntime/core/session/onnxruntime_cxx_api.h
)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${ONNXRUNTIME_INCLUDE_DIRS}/onnxruntime_c_api.h
        ${CMAKE_BINARY_DIR}/onnxruntime/core/session/onnxruntime_c_api.h
)

# --- Wayland protocol code generation ---
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(WLR_SCREENCOPY_XML ${CMAKE_SOURCE_DIR}/protocols/wlr-screencopy-unstable-v1.xml)
set(WLR_SCREENCOPY_C ${CMAKE_BINARY_DIR}/wlr-screencopy-unstable-v1-protocol.c)
set(WLR_SCREENCOPY_H ${CMAKE_BINARY_DIR}/wlr-screencopy-unstable-v1-client-protocol.h)

add_custom_command(
    OUTPUT ${WLR_SCREENCOPY_H}
    COMMAND ${WAYLAND_SCANNER} client-header ${WLR_SCREENCOPY_XML} ${WLR_SCREENCOPY_H}
    DEPENDS ${WLR_SCREENCOPY_XML}
    COMMENT "Generating wlr-screencopy client header"
)
add_custom_command(
    OUTPUT ${WLR_SCREENCOPY_C}
    COMMAND ${WAYLAND_SCANNER} private-code ${WLR_SCREENCOPY_XML} ${WLR_SCREENCOPY_C}
    DEPENDS ${WLR_SCREENCOPY_XML}
    COMMENT "Generating wlr-screencopy protocol code"
)

# --- RapidOcrOnnx (built from source) ---
include(FetchContent)
FetchContent_Declare(
    rapidocr
    GIT_REPOSITORY https://github.com/RapidAI/RapidOcrOnnx.git
    GIT_TAG main
    EXCLUDE_FROM_ALL
)
# Only fetch, don't configure (their CMakeLists.txt has issues with system onnxruntime)
FetchContent_GetProperties(rapidocr)
if(NOT rapidocr_POPULATED)
    FetchContent_Populate(rapidocr)
endif()

# RapidOcrOnnx sources (excluding main.cpp and JNI/CAPI files)
set(RAPIDOCR_SOURCES
    ${rapidocr_SOURCE_DIR}/src/AngleNet.cpp
    ${rapidocr_SOURCE_DIR}/src/clipper.cpp
    ${rapidocr_SOURCE_DIR}/src/CrnnNet.cpp
    ${rapidocr_SOURCE_DIR}/src/DbNet.cpp
    ${rapidocr_SOURCE_DIR}/src/OcrLite.cpp
    ${rapidocr_SOURCE_DIR}/src/OcrLiteImpl.cpp
    ${rapidocr_SOURCE_DIR}/src/OcrResultUtils.cpp
    ${rapidocr_SOURCE_DIR}/src/OcrUtils.cpp
)

# --- Main executable ---
add_executable(openpix
    src/main.cpp
    src/CaptureManager.cpp
    src/OverlayWidget.cpp
    src/Toolbar.cpp
    src/AnnotationManager.cpp
    src/OcrEngine.cpp
    ${WLR_SCREENCOPY_C}
    ${WLR_SCREENCOPY_H}
    ${RAPIDOCR_SOURCES}
    # TODO: Add as implemented
    # src/ScrollCapture.cpp
    # src/Stitcher.cpp
)

target_include_directories(openpix PRIVATE
    ${CMAKE_BINARY_DIR}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${rapidocr_SOURCE_DIR}/include
    ${rapidocr_SOURCE_DIR}/src
)

target_link_libraries(openpix PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::DBus
    Qt6::Concurrent
    ${OpenCV_LIBS}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${ONNXRUNTIME_LIBRARIES}
)

# --- Install ---
install(TARGETS openpix RUNTIME DESTINATION bin)
install(DIRECTORY resources/models DESTINATION share/openpix)
