cmake_minimum_required(VERSION 3.21)

project(cpp_prngs LANGUAGES CXX)

add_library(cpp_prngs INTERFACE)
add_library(cpp_prngs::cpp_prngs ALIAS cpp_prngs)

target_include_directories(cpp_prngs INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/includes>
)
target_compile_features(cpp_prngs INTERFACE cxx_std_23)

option(CPP_PRNGS_BUILD_TESTS "Build the cpp_prngs test suite" ${PROJECT_IS_TOP_LEVEL})

if(CPP_PRNGS_BUILD_TESTS)
	include(FetchContent)

	set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
	set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

	FetchContent_Declare(
		googletest
		URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)
	FetchContent_MakeAvailable(googletest)

	enable_testing()

	add_executable(cpp_prngs_tests tests/random_tests.cpp)
	target_link_libraries(cpp_prngs_tests PRIVATE cpp_prngs::cpp_prngs GTest::gtest)

	include(GoogleTest)
	gtest_discover_tests(cpp_prngs_tests)
endif()
