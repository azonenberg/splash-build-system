ADD_LIBRARY(splashcore SHARED
	CPPToolchain.cpp
	CToolchain.cpp
	Toolchain.cpp

	splashcore.cpp
)

target_link_libraries(splashcore
	cryptopp rt log)
