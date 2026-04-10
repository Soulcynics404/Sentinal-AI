cmake_minimum_required(VERSION 3.10)
project(sentinel VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect imgcodecs)
find_package(dlib REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

add_executable(sentinel main.cpp)

target_include_directories(sentinel PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(sentinel PRIVATE
    ${OpenCV_LIBS}
    dlib::dlib
    Threads::Threads
    ${CURL_LIBRARIES}
    pthread
)