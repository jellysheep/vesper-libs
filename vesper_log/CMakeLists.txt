
project(vesper-log)
cmake_minimum_required(VERSION 2.8)

# add header files of this module
set(HEADERS
    ${PROJECT_SOURCE_DIR}/Logging.hpp
    ${PROJECT_SOURCE_DIR}/Vout.hpp
    ${PROJECT_SOURCE_DIR}/LoggingType.hpp
)

# add source files of this module
set(SOURCES
    ${PROJECT_SOURCE_DIR}/Logging.cpp
    ${PROJECT_SOURCE_DIR}/Vout.cpp
)

# add object files to be compiled
add_library(vesper-log-objects OBJECT ${HEADERS} ${SOURCES})

# combine object files to static and shared library
add_library(vesper-log-static STATIC $<TARGET_OBJECTS:vesper-log-objects>)
add_library(vesper-log SHARED $<TARGET_OBJECTS:vesper-log-objects>)

# add external libraries linked to shared module library
target_link_libraries(vesper-log)
