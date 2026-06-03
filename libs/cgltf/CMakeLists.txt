cmake_minimum_required(VERSION 3.30.0)
cmake_policy(SET CMP0069 NEW)
project(cgltf)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(cgltf)
target_sources(cgltf PRIVATE ${SRCS})
target_include_directories(cgltf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")