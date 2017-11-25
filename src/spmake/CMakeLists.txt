include_directories(${CMAKE_BINARY_DIR}/protobufs)

add_executable(spmake
	main.cpp
)

target_link_libraries(spmake
	rt splashcore log xptools pthread)

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(spmake analysis)

# Installation config
install(TARGETS spmake
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib)
