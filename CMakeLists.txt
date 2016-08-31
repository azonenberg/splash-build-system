cmake_minimum_required(VERSION 3.0)
project(splash)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)
set( LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)

set(EXTRA_WARNINGS "-Wcast-align -Winit-self -Wmissing-declarations -Wshadow")
set(EXTRA_WARNINGS "${EXTRA_WARNINGS} -Wswitch -Wwrite-strings ")
set( CMAKE_CXX_FLAGS "-Wall -Wextra ${EXTRA_WARNINGS} -std=c++11 -fPIC -shared-libgcc -ggdb")
set( CMAKE_LINK_FLAGS "-ggdb")

#Search for Google protocol buffers
find_package(Protobuf REQUIRED)

include_directories(${PROTOBUF_INCLUDE_DIRS})

#Custom target to run static analysis
add_custom_target(
	analysis
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND cppcheck . --enable=style,performance,portability,missingInclude --force --suppress=missingIncludeSystem -q)

add_subdirectory(src)
