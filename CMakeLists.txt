cmake_minimum_required(VERSION 2.8)
project(splash)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)
set( LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)

set(EXTRA_WARNINGS "-Wcast-align -Winit-self -Wmissing-declarations")
set(EXTRA_WARNINGS "${EXTRA_WARNINGS} -Wswitch -Wwrite-strings ")
set( CMAKE_CXX_FLAGS "-Wall -Wextra ${EXTRA_WARNINGS} -std=c++11 -fPIC -shared-libgcc -ggdb -O2")
set( CMAKE_LINK_FLAGS "-ggdb")


#Differentiate between crypto++ and cryptopp libraries on different distros
if(EXISTS "/usr/include/crypto++")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCRYPTOPP_DIR_PLUS")
elseif(EXISTS "/usr/include/cryptopp")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCRYPTOPP_DIR_P")
else()
	message(FATAL_ERROR "crypto++/cryptopp include directory not found.")
endif()

#Search for Google protocol buffers
find_package(Protobuf 3 REQUIRED)

#TODO: Find libyaml-cpp

include_directories(${PROTOBUF_INCLUDE_DIRS})

#Custom target to run static analysis
add_custom_target(
	analysis
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND cppcheck . --enable=style,performance,portability,missingInclude --force -I ${CMAKE_BINARY_DIR}/protobufs --suppress=missingIncludeSystem -q --check-config)

add_subdirectory(src)

#General package config
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SPLASH Build System")
set(CPACK_PACKAGE_VENDOR "Andrew Zonenberg")
set(CPACK_PACKAGE_CONTACT "azonenberg@drawersteak.com")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "1")	# TODO: set this based on git sha or something?
#set(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")

# List of package types to make
set(CPACK_GENERATOR "DEB")

#Debian specific configuration (TODO set bsed on host or something?)
set(CPACK_DEBIAN_PACKAGE_NAME splash)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)

# TODO: Debian package dependencies

#TODO: Windows config
#if(WIN32 AND NOT UNIX)
#  set(CPACK_PACKAGE_ICON "${CMake_SOURCE_DIR}/Utilities/Release\\\\InstallIcon.bmp")
#  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\MyExecutable.exe")
#  set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY} My Famous Project")
#  set(CPACK_NSIS_HELP_LINK "http:\\\\\\\\www.my-project-home-page.org")
#  set(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\www.my-personal-home-page.com")
#  set(CPACK_NSIS_CONTACT "me@my-personal-home-page.com")
#  set(CPACK_NSIS_MODIFY_PATH ON)
#else(WIN32 AND NOT UNIX)
#  set(CPACK_STRIP_FILES "bin/MyExecutable")
#  set(CPACK_SOURCE_STRIP_FILES "")
#endif(WIN32 AND NOT UNIX)
#set(CPACK_PACKAGE_EXECUTABLES "MyExecutable" "My Executable")

# Pull in the packaging tool itself
include(CPack)
