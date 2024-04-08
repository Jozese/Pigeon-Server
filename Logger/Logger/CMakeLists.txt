cmake_minimum_required(VERSION 3.24)
project(Logger)

set(CMAKE_CXX_STANDARD 23)

add_executable(Logger main.cpp Logger.cpp Logger.h)
