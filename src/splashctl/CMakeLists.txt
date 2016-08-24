add_executable(splashctl
	BuildClientThread.cpp
	ClientThread.cpp

	main.cpp
)

target_link_libraries(splashctl
	rt splashcore log xptools pthread)
