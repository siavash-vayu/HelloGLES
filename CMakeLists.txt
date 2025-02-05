cmake_minimum_required(VERSION 3.15)

project(helloGl_cuda LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Ensure CUDA architectures are specified
cmake_policy(SET CMP0104 NEW)

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 75 80 86)  # Modify based on GPU
endif()

# Find dependencies
find_package(OpenCV REQUIRED)
find_package(CUDAToolkit REQUIRED)
find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_AUTOMOC ON)

# Include directories
include_directories(${OpenCV_INCLUDE_DIRS} ${CUDAToolkit_INCLUDE_DIRS})

# Compile the CUDA source file into a library
add_library(depth_to_gpu SHARED depth_to_gpu.cu)
set_target_properties(depth_to_gpu PROPERTIES CUDA_SEPARABLE_COMPILATION ON CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}")

# Executable
add_executable(helloGl_cuda main.cpp glwindow.cpp)

# Link libraries
target_link_libraries(helloGl_cuda PUBLIC
    ${OpenCV_LIBS}
    depth_to_gpu
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL
    OpenGL::GL OpenGL::GLU GLEW X11
)
