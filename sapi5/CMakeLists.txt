cmake_minimum_required(VERSION 3.20)
project(BlackBoxSapi5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(..\\cpp_core ${CMAKE_CURRENT_BINARY_DIR}/cpp_core_build)

set(SPEECHSDK51_CANDIDATES
  "${CMAKE_CURRENT_SOURCE_DIR}/../tools/SpeechSDK51_admin/program files/Microsoft Speech SDK 5.1"
  "$ENV{SPEECHSDK51_ROOT}"
  "C:/Program Files/Microsoft Speech SDK 5.1"
  "C:/Program Files (x86)/Microsoft Speech SDK 5.1"
)

set(SPEECHSDK51_ROOT "")
foreach(candidate IN LISTS SPEECHSDK51_CANDIDATES)
  if(candidate AND EXISTS "${candidate}/Include/sapi.h")
    set(SPEECHSDK51_ROOT "${candidate}")
    break()
  endif()
endforeach()

if(NOT SPEECHSDK51_ROOT)
  message(FATAL_ERROR "Nie znaleziono Microsoft Speech SDK 5.1. Ustaw SPEECHSDK51_ROOT albo rozpakuj SDK do tools/SpeechSDK51_admin.")
endif()

add_library(BlackBoxSapi5 SHARED
  engine/blackbox_sapi5.cpp
  engine/blackbox_sapi5.def
)

add_executable(BlackBoxSapi5Config WIN32
  config/blackbox_sapi5_config.cpp
)

target_compile_definitions(BlackBoxSapi5 PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(BlackBoxSapi5Config PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(BlackBoxSapi5 PRIVATE
  "${SPEECHSDK51_ROOT}/Include"
)

target_compile_options(BlackBoxSapi5 PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>
)
target_compile_options(BlackBoxSapi5Config PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>
)

target_link_libraries(BlackBoxSapi5 PRIVATE blackbox_core_cpp ole32 uuid)
target_link_libraries(BlackBoxSapi5Config PRIVATE comctl32)

set_target_properties(BlackBoxSapi5 PROPERTIES
  OUTPUT_NAME "BlackBoxSapi5"
)
set_target_properties(BlackBoxSapi5Config PROPERTIES
  OUTPUT_NAME "BlackBoxSapi5Config"
)
