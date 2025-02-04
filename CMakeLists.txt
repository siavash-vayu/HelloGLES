cmake_minimum_required(VERSION 3.15)

project(helloGl_cuda LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(OpenCV REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Concurrent OpenGL)
set(CMAKE_AUTOMOC ON)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(helloGl_cuda main.cpp)
target_sources(helloGl_cuda PRIVATE glwindow.cpp glwindow.h)

target_link_libraries(helloGl_cuda PUBLIC
	${OpenCV_LIBS} 	Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL GL GLEW GLU X11)

