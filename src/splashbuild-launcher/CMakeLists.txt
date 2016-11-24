include_directories(${CMAKE_BINARY_DIR}/protobufs)

add_executable(splashbuild-launcher
	main.cpp
)

target_link_libraries(splashbuild-launcher
	rt splashcore log xptools)

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splashbuild-launcher analysis)

# Installation config
install(TARGETS splashbuild-launcher
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib)
