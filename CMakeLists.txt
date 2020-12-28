cmake_minimum_required(VERSION 3.5)
project(cmt)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-g -lpthread -O3")
#set release
#set(CMAKE_BUILD_TYPE Release)

#marco definitions
#add_definitions("-DNOSUB")
#add_definitions("-DMATCH_NOSUB")
#add_definitions("-DLS_HEURISTIC")

set(SOURCE_FILES unittest.c ./cmt/cmt.c ./cmt/common.c ./cmt/nfheap.c ./cmt/vector.c)
add_executable(cmt ${SOURCE_FILES})