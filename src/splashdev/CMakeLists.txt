include_directories(${CMAKE_BINARY_DIR}/protobufs)

add_executable(splashdev
	ChangeNotifier.cpp
	FileEventHandlers.cpp

	main.cpp
)

target_link_libraries(splashdev
	rt splashcore log xptools) 

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splashdev analysis)

# Installation config
install(TARGETS splashdev
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib)
