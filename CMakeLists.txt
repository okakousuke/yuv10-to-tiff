cmake_minimum_required(VERSION 3.20)
project(yuv10_to_tiff LANGUAGES CXX)

add_executable(yuv10_to_tiff yuv10_to_tiff.cpp)
target_compile_features(yuv10_to_tiff PRIVATE cxx_std_17)
if (MSVC)
  target_compile_options(yuv10_to_tiff PRIVATE /W4)
else()
  target_compile_options(yuv10_to_tiff PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if (BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter)
  if (Python3_Interpreter_FOUND)
    add_test(
      NAME yuv10_reference
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_reference.py
              $<TARGET_FILE:yuv10_to_tiff>
    )
  endif()
endif()
