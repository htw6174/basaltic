cmake_minimum_required(VERSION 3.0)

project(kingdom LANGUAGES C)

add_executable(kingdom main.c)

install(TARGETS kingdom RUNTIME DESTINATION bin)
