cmake_minimum_required(VERSION 3.16)

# set project name and language
project(HPC_Analytics_Engine LANGUAGES CXX)

# ensure C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# default to release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# compiler flags
if (MSVC)
  add_compile_options(/O2 /DNDEBUG /permissive- /W4)
else()
  # clang++ or g++ on non windows machines  
  add_compile_options(-O3 -march=native -DNDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion)
endif()

# include headers under src/
include_directories(src)

# executable - analytics engine
add_executable(hpcae/src/hpcae/main.cpp)

# executable - log generator
add_executable(loggen/tools/generate.cpp)

# multi-threading using pthreads on non windows machine
if (NOT MSVC)
  target_link_libraries(hpcae PRIVATE pthread)
  target_link_libraries(loggen PRIVATE pthread)
endif()
