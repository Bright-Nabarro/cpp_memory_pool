cmake_minimum_required(VERSION 3.21)
project(memory_pool CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG_MODE ON)
option(ENABLE_TEST ON)

add_compile_options(
	-Wall
	-Wextra
	-Wuninitialized
)

if (DEBUG_MODE)
	set(CMAKE_BUILD_TYPE Debug)
else()
	set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(src)

if (ENABLE_TEST)
	enable_testing()
	add_subdirectory(test)
endif()
