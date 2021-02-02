cmake_minimum_required(VERSION 3.13)
project(da_etcs)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_executable(da_etcs
               src/instance.cpp
               include/instance.h
               src/graph.cpp
               include/graph.h
               src/edge.cpp
               include/edge.h
               src/train.cpp
               include/train.h
               apps/main.cpp)
list (APPEND CMAKE_MODULE_PATH
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Z3 REQUIRED)
if(Z3_FOUND AND NOT TARGET z3::z3lib)
	add_library(z3::z3lib IMPORTED INTERFACE)
	set_property(TARGET z3::z3lib PROPERTY
	             INTERFACE_INCLUDE_DIRECTORIES ${Z3_CXX_INCLUDE_DIRS})
	set_property(TARGET z3::z3lib PROPERTY
	             INTERFACE_LINK_LIBRARIES ${Z3_LIBRARIES})
endif()

set_property(TARGET da_etcs PROPERTY CXX_STANDARD 17)
set_property(TARGET da_etcs PROPERTY CXX_EXTENSIONS OFF)
#target_compile_options(da_etcs PRIVATE -DNDEBUG -g -mtune=native -march=native -pipe -flto -O2)
if(MSVC)
	target_compile_options(da_etcs PUBLIC "/GR" "/O2" "/GL")
else()
        target_compile_options(da_etcs PUBLIC -Wall -g -Og -Wextra -Wpedantic -pedantic -O2 -mtune=native -march=native -g0)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
	                           $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include/>)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(${PROJECT_NAME} z3::z3lib)                              
#target_include_directories(da_etcs PUBLIC /usr/local/include src include)
#target_link_directories(da_etcs PUBLIC /usr/local/lib)
#target_link_libraries(da_etcs Z3)
