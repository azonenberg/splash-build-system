add_executable(splashdev
	FileEventHandlers.cpp

	main.cpp
)

target_link_libraries(splashdev
	rt splashcore log xptools) 
