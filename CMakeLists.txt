cmake_minimum_required(VERSION 3.16)

project(Jarat_ghaz
  VERSION 0.1.0
  LANGUAGES C CXX
)

# C standard for main code
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++ standard for GoogleTest
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
endif()

# Threads
find_package(Threads REQUIRED)

# Core C sources used by truck and client (pure C)
set(CORE_SRC
  src/util.c
  src/protocol.c
  src/gps.c
  src/net.c
  src/logger.c
)

add_library(core STATIC ${CORE_SRC})
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(core PUBLIC Threads::Threads)

if(UNIX)
  target_link_libraries(core PUBLIC m)
endif()

# ---- truck (C) ----
add_executable(truck src/truck.c)
target_link_libraries(truck PRIVATE core)

# ---- client (C) ----
add_executable(client src/client.c)
target_link_libraries(client PRIVATE core)

# =======================
# GoogleTest for C tests
# =======================
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

# For MSVC, avoid CRT conflicts (not really needed on WSL but harmless)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(test_all tests/test_all.cpp)
target_link_libraries(test_all PRIVATE core GTest::gtest_main)
target_include_directories(test_all PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(test_all)
