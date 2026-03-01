cmake_minimum_required(VERSION 3.20)
project(BlackBoxSapi5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(BlackBoxSapi5 SHARED
  engine/blackbox_sapi5.cpp
  engine/blackbox_sapi5.def
)

target_compile_definitions(BlackBoxSapi5 PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

target_compile_options(BlackBoxSapi5 PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>
)

target_link_libraries(BlackBoxSapi5 PRIVATE ole32 sapi uuid)

set_target_properties(BlackBoxSapi5 PROPERTIES
  OUTPUT_NAME "BlackBoxSapi5"
)
