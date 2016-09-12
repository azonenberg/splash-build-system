include_directories(${CMAKE_BINARY_DIR}/protobufs)

add_executable(splashctl
	BuildClientThread.cpp
	ClientThread.cpp
	DevClientThread.cpp

	main.cpp
)

target_link_libraries(splashctl
	rt splashcore log xptools pthread)

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splashctl analysis)

# Installation config
install(TARGETS splashctl
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib)
