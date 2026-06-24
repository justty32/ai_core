cmake_minimum_required(VERSION 3.20)
project(core_handy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ac_helper 是 header-only，這裡用 INTERFACE library 表達其性質
add_library(ac_helper INTERFACE)
target_include_directories(ac_helper INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE ac_helper)
