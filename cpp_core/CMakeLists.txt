cmake_minimum_required(VERSION 3.20)
project(BlackBoxCoreCpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sam_c_core STATIC
  ../third_party/SAM/src/sam.c
  ../third_party/SAM/src/render.c
  src/sam_stub.c
)

target_include_directories(sam_c_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SAM/src
)

if(MSVC)
  target_compile_definitions(sam_c_core PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(sam_c_core PRIVATE /W3)
else()
  target_compile_options(sam_c_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_library(blackbox_core_cpp STATIC
  src/prosody.cpp
  src/sam_like.cpp
  src/text.cpp
)

target_include_directories(blackbox_core_cpp
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(blackbox_core_cpp PRIVATE /W4 /EHsc /utf-8)
else()
  target_compile_options(blackbox_core_cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(blackbox_core_cpp PUBLIC sam_c_core)

add_library(blackbox_nvda_native SHARED
  src/nvda_api.cpp
)

target_include_directories(blackbox_nvda_native
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(blackbox_nvda_native PRIVATE /W4 /EHsc /utf-8)
else()
  target_compile_options(blackbox_nvda_native PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(blackbox_nvda_native PRIVATE blackbox_core_cpp)
