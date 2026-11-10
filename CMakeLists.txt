cmake_minimum_required(VERSION 3.16)
project(recipe_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(recipe_core
  src/normalize.cpp
  src/index.cpp
  src/search.cpp
  src/synthetic.cpp
)
target_include_directories(recipe_core PUBLIC include)
if(MSVC)
  target_compile_options(recipe_core PRIVATE /W4)
else()
  target_compile_options(recipe_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(recipe_search tools/recipe_search.cpp)
target_link_libraries(recipe_search PRIVATE recipe_core)

add_executable(recipe_bench bench/bench.cpp)
target_link_libraries(recipe_bench PRIVATE recipe_core)

add_executable(recipe_tests tests/tests.cpp)
target_link_libraries(recipe_tests PRIVATE recipe_core)
target_compile_definitions(recipe_tests PRIVATE
  SAMPLE_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/sample_recipes.tsv")

# C interface used by the iOS app (Swift calls it through a bridging header).
add_library(fridge_bridge bridge/fridge_bridge.cpp)
target_include_directories(fridge_bridge PUBLIC bridge)
target_link_libraries(fridge_bridge PUBLIC recipe_core)

add_executable(bridge_test tests/bridge_test.c)
target_link_libraries(bridge_test PRIVATE fridge_bridge)
set_target_properties(bridge_test PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(bridge_test PRIVATE
  SAMPLE_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/sample_recipes.tsv")

enable_testing()
add_test(NAME recipe_tests COMMAND recipe_tests)
add_test(NAME bridge_test COMMAND bridge_test)
