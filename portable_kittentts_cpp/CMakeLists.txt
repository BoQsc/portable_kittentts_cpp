cmake_minimum_required(VERSION 3.20)

project(portable_kittentts_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(kitten_tts
    src/main.cpp
    src/kitten_tts.cpp
    src/npy_reader.cpp
    src/text_preprocessor.cpp
)

target_include_directories(kitten_tts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/include
)

if (WIN32)
    target_link_libraries(kitten_tts PRIVATE winmm)
else()
    target_link_libraries(kitten_tts PRIVATE pthread dl)
endif()
