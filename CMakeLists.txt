cmake_minimum_required(VERSION 3.7)

project(
	llz80emu_tests
	DESCRIPTION "Unit tests for llz80emu"
)

add_subdirectory(llz80emu)
add_subdirectory(json)
add_subdirectory(tester)
