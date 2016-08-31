ADD_LIBRARY(splashcore SHARED
	
	Cache.cpp

	CPPToolchain.cpp
	CToolchain.cpp
	FPGAToolchain.cpp
	GNUCToolchain.cpp
	GNUCPPToolchain.cpp
	GNULinkerToolchain.cpp
	LinkerToolchain.cpp
	RemoteToolchain.cpp
	Toolchain.cpp
	XilinxISEToolchain.cpp
	XilinxVivadoToolchain.cpp

	BuildGraph.cpp
	BuildGraphNode.cpp

	splashcore.cpp
)

target_link_libraries(splashcore
	cryptopp rt log yaml-cpp)

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splashcore analysis)
