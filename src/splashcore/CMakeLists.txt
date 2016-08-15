ADD_LIBRARY(splashcore SHARED
	splashcore.cpp
)

target_link_libraries(splashcore
	cryptopp rt)
