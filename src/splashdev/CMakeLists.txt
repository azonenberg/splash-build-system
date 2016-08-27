add_executable(splashdev
	FileEventHandlers.cpp

	main.cpp
)

target_link_libraries(splashdev
	rt splashcore log xptools) 

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splashdev analysis)
