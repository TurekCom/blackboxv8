cmake_minimum_required(VERSION 3.22.1)
project(blackbox_android_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BLACKBOX_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

add_library(sam_c_core STATIC
    ${BLACKBOX_ROOT}/third_party/SAM/src/sam.c
    ${BLACKBOX_ROOT}/third_party/SAM/src/render.c
    ${BLACKBOX_ROOT}/cpp_core/src/sam_stub.c
)

target_include_directories(sam_c_core
    PUBLIC
        ${BLACKBOX_ROOT}/third_party/SAM/src
)

add_library(blackbox_core_cpp STATIC
    ${BLACKBOX_ROOT}/cpp_core/src/prosody.cpp
    ${BLACKBOX_ROOT}/cpp_core/src/sam_like.cpp
    ${BLACKBOX_ROOT}/cpp_core/src/text.cpp
)

target_include_directories(blackbox_core_cpp
    PUBLIC
        ${BLACKBOX_ROOT}/cpp_core/include
)

target_link_libraries(blackbox_core_cpp PUBLIC sam_c_core)

add_library(blackbox_android_native SHARED
    blackbox_android_jni.cpp
)

target_include_directories(blackbox_android_native
    PRIVATE
        ${BLACKBOX_ROOT}/cpp_core/include
)

target_link_libraries(blackbox_android_native
    PRIVATE
        android
        log
        blackbox_core_cpp
)
