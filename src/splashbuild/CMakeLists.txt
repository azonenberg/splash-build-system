add_executable(splashbuild
	FindCompilers.cpp

	main.cpp
)

target_link_libraries(splashbuild
	rt splashcore log xptools)
