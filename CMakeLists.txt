cmake_minimum_required(VERSION 3.24)
project(VeriQueue VERSION 0.1.0 LANGUAGES CXX)

option(VERIQUEUE_BUILD_TESTS "Build deterministic/model tests" ON)
option(VERIQUEUE_BUILD_STRESS "Build concurrent stress tests" ON)
option(VERIQUEUE_BUILD_BENCHMARKS "Build benchmark harnesses" OFF)
option(VERIQUEUE_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
option(VERIQUEUE_BUILD_RELACY "Build Relacy model-check target" OFF)
option(VERIQUEUE_BUILD_LINEARIZABILITY "Build C++ history generator" OFF)
option(VERIQUEUE_BUILD_BASELINES "Build pinned comparative benchmark baselines" OFF)
set(VERIQUEUE_RELACY_MUTATION_ID "0" CACHE STRING "Relacy mutant id (0 = correct)")
option(VERIQUEUE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(VERIQUEUE_DISABLE_PADDING "Deliberately disable cache-line padding" OFF)
option(VERIQUEUE_TEST_FORCE_SINGLE_OWNER_CURSOR
       "Force the ARM64 single-owner cursor strategy for verification builds" OFF)

add_library(veriqueue INTERFACE)
add_library(veriqueue::veriqueue ALIAS veriqueue)
target_include_directories(veriqueue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(veriqueue INTERFACE cxx_std_20)
if(VERIQUEUE_DISABLE_PADDING)
  target_compile_definitions(veriqueue INTERFACE VERIQUEUE_DISABLE_PADDING=1)
endif()
if(VERIQUEUE_TEST_FORCE_SINGLE_OWNER_CURSOR)
  target_compile_definitions(veriqueue INTERFACE VERIQUEUE_FORCE_SINGLE_OWNER_CURSOR=1)
endif()

function(veriqueue_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(VERIQUEUE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wold-style-cast -Wcast-align -Wundef)
    if(VERIQUEUE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

include(CTest)

function(veriqueue_add_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE veriqueue::veriqueue)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_definitions(${name} PRIVATE VERIQUEUE_TESTING=1)
  veriqueue_apply_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

if(VERIQUEUE_BUILD_TESTS)
  veriqueue_add_test(test_basic tests/unit/test_basic.cpp)
  veriqueue_add_test(test_bulk tests/unit/test_bulk.cpp)
  veriqueue_add_test(test_capacity tests/unit/test_capacity.cpp)
  veriqueue_add_test(test_wraparound tests/unit/test_wraparound.cpp)
  veriqueue_add_test(test_lifetime tests/unit/test_lifetime.cpp)
  veriqueue_add_test(test_alignment tests/unit/test_alignment.cpp)
  veriqueue_add_test(test_move_only tests/unit/test_move_only.cpp)
  veriqueue_add_test(test_exceptions tests/unit/test_exceptions.cpp)
  veriqueue_add_test(model_random tests/model/model_random.cpp)
  veriqueue_add_test(exhaustive_small tests/model/exhaustive_small.cpp)
endif()

if(VERIQUEUE_BUILD_STRESS)
  find_package(Threads REQUIRED)
  foreach(stress_name IN ITEMS stress_spsc stress_burst stress_jitter stress_affinity stress_bulk)
    add_executable(${stress_name} tests/stress/${stress_name}.cpp)
    target_link_libraries(${stress_name} PRIVATE veriqueue::veriqueue Threads::Threads)
    target_include_directories(${stress_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    veriqueue_apply_warnings(${stress_name})
    add_test(NAME ${stress_name} COMMAND ${stress_name})
  endforeach()
endif()

if(VERIQUEUE_BUILD_BENCHMARKS)
  find_package(Threads REQUIRED)
  foreach(bench_name IN ITEMS bench_throughput bench_latency bench_bursts bench_payload bench_hotpath_variants bench_bulk)
    add_executable(${bench_name} bench/${bench_name}.cpp bench/environment.cpp)
    target_link_libraries(${bench_name} PRIVATE veriqueue::veriqueue Threads::Threads)
    target_include_directories(${bench_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bench)
    veriqueue_apply_warnings(${bench_name})
  endforeach()
endif()

if(VERIQUEUE_BUILD_FUZZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "VERIQUEUE_BUILD_FUZZERS requires Clang/libFuzzer")
  endif()
  foreach(fuzz_name IN ITEMS fuzz_model fuzz_lifetime fuzz_wraparound)
    add_executable(${fuzz_name} fuzz/${fuzz_name}.cpp)
    target_link_libraries(${fuzz_name} PRIVATE veriqueue::veriqueue)
    target_compile_options(${fuzz_name} PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(${fuzz_name} PRIVATE -fsanitize=fuzzer,address,undefined)
    veriqueue_apply_warnings(${fuzz_name})
  endforeach()
endif()

if(VERIQUEUE_BUILD_RELACY)
  include(FetchContent)
  FetchContent_Declare(relacy
    GIT_REPOSITORY https://github.com/dvyukov/relacy.git
    GIT_TAG 2c87447b56b90cbccb5129c93a4e82eda5e7868e)
  FetchContent_MakeAvailable(relacy)
  add_executable(spsc_relacy verify/relacy/spsc_relacy.cpp)
  target_link_libraries(spsc_relacy PRIVATE relacy::relacy)
  target_compile_features(spsc_relacy PRIVATE cxx_std_23)
  # Relacy is a third-party header-only model checker. Keep project warning policy
  # on production/tests, but do not promote warnings originating inside Relacy to errors.
  add_test(NAME spsc_relacy COMMAND spsc_relacy)

  add_executable(spsc_single_owner_relacy verify/relacy/spsc_single_owner_relacy.cpp)
  target_link_libraries(spsc_single_owner_relacy PRIVATE relacy::relacy)
  target_compile_features(spsc_single_owner_relacy PRIVATE cxx_std_23)
  add_test(NAME spsc_single_owner_relacy COMMAND spsc_single_owner_relacy)

  add_executable(mutation_relacy verify/relacy/mutation_relacy.cpp)
  target_link_libraries(mutation_relacy PRIVATE relacy::relacy)
  target_compile_features(mutation_relacy PRIVATE cxx_std_23)
  target_compile_definitions(mutation_relacy PRIVATE VERIQUEUE_RELACY_MUTATION_ID=${VERIQUEUE_RELACY_MUTATION_ID})
endif()

if(VERIQUEUE_BUILD_LINEARIZABILITY)
  find_package(Threads REQUIRED)
  add_executable(history_generator verify/linearizability/history_generator.cpp)
  target_link_libraries(history_generator PRIVATE veriqueue::veriqueue Threads::Threads)
  target_include_directories(history_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/verify/linearizability)
  veriqueue_apply_warnings(history_generator)

  add_executable(history_generator_single_owner verify/linearizability/history_generator.cpp)
  target_link_libraries(history_generator_single_owner PRIVATE veriqueue::veriqueue Threads::Threads)
  target_include_directories(history_generator_single_owner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/verify/linearizability)
  target_compile_definitions(history_generator_single_owner PRIVATE
    VERIQUEUE_FORCE_SINGLE_OWNER_CURSOR=1)
  veriqueue_apply_warnings(history_generator_single_owner)
endif()

if(VERIQUEUE_BUILD_BASELINES)
  include(FetchContent)

  FetchContent_Declare(rigtorp_spsc
    GIT_REPOSITORY https://github.com/rigtorp/SPSCQueue.git
    GIT_TAG 59a6a938513ea5004817383711ed35d32385d3ee)

  # These two projects are used only as source/header baselines. SOURCE_SUBDIR
  # deliberately points at a non-project directory so MakeAvailable downloads
  # the pinned sources without importing their build flags or test targets.
  FetchContent_Declare(moodycamel_readerwriterqueue
    GIT_REPOSITORY https://github.com/cameron314/readerwriterqueue.git
    GIT_TAG 6867b56452352acf077fccd5f6cc7e3a8cfde0fb
    SOURCE_SUBDIR __veriqueue_headers_only)
  FetchContent_Declare(drogalis_spsc
    GIT_REPOSITORY https://github.com/drogalis/SPSC-Queue.git
    GIT_TAG c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96
    SOURCE_SUBDIR __veriqueue_headers_only)

  FetchContent_MakeAvailable(rigtorp_spsc moodycamel_readerwriterqueue drogalis_spsc)

  find_package(Boost REQUIRED)
  find_package(Threads REQUIRED)
  add_executable(bench_compare bench/bench_compare.cpp bench/environment.cpp)
  target_link_libraries(bench_compare PRIVATE veriqueue::veriqueue Threads::Threads)
  target_include_directories(bench_compare PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bench)
  # Baseline headers are third-party comparison subjects, not project code. Mark
  # them SYSTEM so their compiler-specific warnings cannot fail our -Werror gate.
  target_include_directories(bench_compare SYSTEM PRIVATE
    ${rigtorp_spsc_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${moodycamel_readerwriterqueue_SOURCE_DIR}
    ${drogalis_spsc_SOURCE_DIR}/include)
  veriqueue_apply_warnings(bench_compare)
endif()
