cmake_minimum_required(VERSION 3.20)
project(PrimeStruct LANGUAGES CXX)

option(PRIMESTRUCT_BUILD_TESTS "Build PrimeStruct tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(primec_lib
  src/AstPrinter.cpp
  src/Emitter.cpp
  src/IncludeResolver.cpp
  src/IrPrinter.cpp
  src/Lexer.cpp
  src/Parser.cpp
  src/Semantics.cpp
  src/TextFilterPipeline.cpp
)

target_include_directories(primec_lib PUBLIC include)
target_compile_options(primec_lib PRIVATE -Wall -Wextra -Wpedantic)

add_executable(primec src/main.cpp)
target_link_libraries(primec PRIVATE primec_lib)
target_compile_options(primec PRIVATE -Wall -Wextra -Wpedantic)

if(PRIMESTRUCT_BUILD_TESTS)
  enable_testing()
  add_executable(PrimeStruct_tests
    tests/unit/test_main.cpp
    tests/unit/test_parser_basic.cpp
    tests/unit/test_include_resolver.cpp
    tests/unit/test_include_errors.cpp
    tests/unit/test_include_multiple.cpp
    tests/unit/test_semantics.cpp
    tests/unit/test_ast_ir_dump.cpp
    tests/unit/test_ast_ir_dump_namespace.cpp
    tests/unit/test_parser_errors.cpp
    tests/unit/test_parser_errors_more.cpp
    tests/unit/test_execution_parsing.cpp
    tests/unit/test_execution_whitespace.cpp
    tests/unit/test_template_parsing.cpp
    tests/unit/test_compile_run.cpp
    tests/unit/test_text_filter_pipeline.cpp
    tests/unit/test_printers_manual.cpp
    tests/unit/test_semantics_manual.cpp
  )
  target_include_directories(PrimeStruct_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeStruct_tests PRIVATE primec_lib)
  target_compile_options(PrimeStruct_tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME PrimeStruct_tests COMMAND $<TARGET_FILE:PrimeStruct_tests>)
endif()
