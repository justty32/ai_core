# llme — cllm 多 endpoint dispatcher（薄轉發器）。純 C++17、零外部依賴。
# 產物 _exec 直接落在本資料夾（folder-as-callable：資料夾自帶可執行體）。
cmake_minimum_required(VERSION 3.16)
project(llme CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(_exec main.cpp)
set_target_properties(_exec PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# 免 CMake 的一行建置（等價）：
#   c++ -std=c++17 -O2 main.cpp -o _exec
