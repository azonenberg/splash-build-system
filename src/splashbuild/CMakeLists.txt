include_directories(${CMAKE_BINARY_DIR}/protobufs)

add_executable(splashbuild
	FindCompilers.cpp

	main.cpp
)

target_link_libraries(splashbuild
	rt splashcore log xptools)

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splashbuild analysis)

# Installation config
install(TARGETS splashbuild
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib)
