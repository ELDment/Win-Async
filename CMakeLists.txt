cmake_minimum_required(VERSION 3.10)

project(CppCoroutine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/await:strict /utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

add_library(coroutine STATIC
    src/coroutine.cpp
    src/scheduler.cpp
    src/exception_handler.cpp
)

target_include_directories(coroutine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(coroutine PRIVATE $<$<CONFIG:Debug>:DEBUG_COROUTINE>)

add_executable(benchmark
    test/benchmark.cpp
)

target_link_libraries(benchmark PRIVATE coroutine)