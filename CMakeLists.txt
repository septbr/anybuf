cmake_minimum_required(VERSION 3.18)
project(anybuf)

set(CMAKE_CXX_STANDARD 17)

if (WIN32)
	add_definitions("-DWIN32")
endif()

include_directories("./include")

file(GLOB_RECURSE src_file "./src/*.cpp")
add_executable(${PROJECT_NAME} ${src_file})
