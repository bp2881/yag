cmake_minimum_required(VERSION 3.16)
project(yag LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(yag
    cli/main.cpp
    core/repo.cpp
    core/commit.cpp
    core/branch.cpp
    core/sync.cpp
    core/staging.cpp
    utils/file_utils.cpp
    utils/hash.cpp
)

target_include_directories(yag PRIVATE ${CMAKE_SOURCE_DIR})
