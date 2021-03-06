
project(vesper-util)
cmake_minimum_required(VERSION 2.8)

# add API header files of this module
set(API_HEADERS
    ${PROJECT_SOURCE_DIR}/vsp_api.h
    ${PROJECT_SOURCE_DIR}/vsp_error.h
)

# add header files of this module
set(HEADERS
    ${PROJECT_SOURCE_DIR}/vsp_util.h
    ${PROJECT_SOURCE_DIR}/vsp_time.h
    ${PROJECT_SOURCE_DIR}/vsp_random.h
)

# add source files of this module
set(SOURCES
    ${PROJECT_SOURCE_DIR}/vsp_error.c
    ${PROJECT_SOURCE_DIR}/vsp_time.c
    ${PROJECT_SOURCE_DIR}/vsp_random.c
)

# add object files to be compiled
add_library(vesper-util-objects OBJECT ${API_HEADERS} ${HEADERS} ${SOURCES})

# combine object files to static and shared library
add_library(vesper-util-static STATIC $<TARGET_OBJECTS:vesper-util-objects>)
add_library(vesper-util SHARED $<TARGET_OBJECTS:vesper-util-objects>)

# add external libraries linked to shared module library
target_link_libraries(vesper-util nanomsg)

# add rule to install public header file
install(FILES ${API_HEADERS} DESTINATION include)
