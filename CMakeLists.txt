cmake_minimum_required(VERSION 3.30.0)
cmake_policy(SET CMP0069 NEW)
project(DeferredTest)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    add_compile_options(/arch:AVX2)
  elseif(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}")

# Compiling shaders
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "GLSLC not found! Make sure to download the vulkan sdk and set the VULKAN_SDK environment variable!")
endif()

function(CompileShader shader output_dir OUT_LIST)
    get_filename_component(FILE_NAME ${shader} NAME)
    set(SPIRV_NAME "${output_dir}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_NAME}
        COMMAND ${GLSLC_EXECUTABLE} ${flags} ${shader} -o ${SPIRV_NAME}
        DEPENDS ${shader}
        BYPRODUCTS ${SPIRV_NAME}
        VERBATIM
    )

    set(${OUT_LIST} ${${OUT_LIST}} ${SPIRV_NAME} PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE VERT_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.vert")
file(GLOB_RECURSE FRAG_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.frag")

set(SPIRV_SHADERS "")
foreach(SHADER ${VERT_SRCS})
    CompileShader(${SHADER} ${CMAKE_BINARY_DIR}/assets/shaders SPIRV_SHADERS)
endforeach()

foreach(SHADER ${FRAG_SRCS})
    CompileShader(${SHADER} ${CMAKE_BINARY_DIR}/assets/shaders SPIRV_SHADERS)
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_SHADERS})

add_subdirectory(libs/imgui)
add_subdirectory(libs/glfw)
add_subdirectory(libs/stb)
add_subdirectory(libs/cgltf)

find_package(Vulkan REQUIRED)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME} PRIVATE imgui glfw stb cgltf Vulkan::Vulkan)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/glm)
