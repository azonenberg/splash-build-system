include_directories(${CMAKE_BINARY_DIR}/protobufs)

add_executable(splash
	main.cpp
)

target_link_libraries(splash
	rt splashcore log xptools pthread)

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splash analysis)

# Installation config
install(TARGETS splash
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib)
