cmake_minimum_required(VERSION 3.7)

project(
	llz80emu_tester
	DESCRIPTION "Unit tester for llz80emu"
	LANGUAGES CXX
)

add_executable(tester main.cpp)
target_link_libraries(tester PRIVATE llz80emu_static nlohmann_json::nlohmann_json)
