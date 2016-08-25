ADD_LIBRARY(splashcore SHARED
	
	CPPToolchain.cpp
	CToolchain.cpp
	FPGAToolchain.cpp
	GNUCToolchain.cpp
	GNUCPPToolchain.cpp
	Toolchain.cpp
	XilinxISEToolchain.cpp
	XilinxVivadoToolchain.cpp

	splashcore.cpp
)

target_link_libraries(splashcore
	cryptopp rt log)
