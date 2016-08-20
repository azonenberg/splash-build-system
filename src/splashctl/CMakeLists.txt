add_executable(splashctl
	main.cpp
)

target_link_libraries(splashctl
	rt splashcore log xptools)
