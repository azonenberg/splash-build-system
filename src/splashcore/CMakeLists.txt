set(PROTOBUF_DIR ${CMAKE_BINARY_DIR}/protobufs)

set(PROTO_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SplashNet.proto
)

add_custom_command(
	OUTPUT ${PROTOBUF_DIR}/splashcore/SplashNet.pb.cc ${PROTOBUF_DIR}/splashcore/SplashNet.pb.h
	COMMAND mkdir -p ${PROTOBUF_DIR}
	COMMAND protoc --proto_path=${CMAKE_SOURCE_DIR}/src --cpp_out=${PROTOBUF_DIR} ${PROTO_FILES}
	COMMENT "Building protocol buffer"
	DEPENDS ${PROTO_FILES}
)

include_directories(${PROTOBUF_DIR})

add_library(splashcore SHARED

	Cache.cpp
	NodeManager.cpp
	WorkingCopy.cpp

	${PROTOBUF_DIR}/splashcore/SplashNet.pb.cc

	BuildConfiguration.cpp
	BuildFlag.cpp
	BuildSettings.cpp
	ToolchainSettings.cpp

	DependencyCache.cpp

	CPPToolchain.cpp
	CToolchain.cpp
	FPGAToolchain.cpp
	GNUCToolchain.cpp
	GNUCPPToolchain.cpp
	GNULinkerToolchain.cpp
	GNUToolchain.cpp
	LinkerToolchain.cpp
	RemoteToolchain.cpp
	Toolchain.cpp
	XilinxISEToolchain.cpp
	XilinxVivadoToolchain.cpp
	YosysToolchain.cpp

	BuildGraph.cpp
	BuildGraphNode.cpp
	ConstantTableNode.cpp
	CPPExecutableNode.cpp
	CPPSharedLibraryNode.cpp
	CPPObjectNode.cpp
	FormalVerificationNode.cpp
	FPGABitstreamNode.cpp
	HDLNetlistNode.cpp
	SourceFileNode.cpp
	SystemLibraryNode.cpp

	BuildJob.cpp
	DependencyScanJob.cpp
	Job.cpp
	Scheduler.cpp

	ClientSettings.cpp

	splashcore.cpp
)

# disable warnings for generated protobuf code
set_source_files_properties(${PROTOBUF_DIR}/splashcore/SplashNet.pb.cc PROPERTIES COMPILE_FLAGS -w)

target_link_libraries(splashcore
	cryptopp rt log yaml-cpp xptools ${PROTOBUF_LIBRARIES})

#force static analysis to rerun
#TODO: automate this for every target?
add_dependencies(splashcore analysis)

# Installation config
install(TARGETS splashcore
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib)
