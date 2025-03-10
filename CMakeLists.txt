cmake_minimum_required(VERSION 3.10)
project(libdrm-samples)

# CMAKE VAR SETUP
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake ${CMAKE_MODULE_PATH})

# Find CMAKE package
find_package(Libdrm REQUIRED)

# CMAKE C & CXX FLAGS SETUP

# compile definitions

# PROJECT VARS SETUP
set(PROJECT_BIN     "bin")
set(PROJECT_LIB     "lib")
set(PROJECT_ARCHIVE "lib/static")
set(PROJECT_INSTALL ${CMAKE_INSTALL_PREFIX})
set(PROJECT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
list(APPEND PROJECT_INCLUDE_DIR "${CMAKE_PREFIX_PATH}/include")

add_executable(DrmApp drmapp_sample.cpp)
target_include_directories(DrmApp   PUBLIC  $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
                                            $<BUILD_INTERFACE:${Libdrm_INCLUDE_DIRS}>
                                            )
target_link_libraries(DrmApp PUBLIC ${Libdrm_LIBRARIES})
