if(NOT DEFINED TEST_BINARY OR TEST_BINARY STREQUAL "")
  message(FATAL_ERROR "TEST_BINARY is required")
endif()

if(NOT DEFINED EXPECTED_SUITES_FILE OR EXPECTED_SUITES_FILE STREQUAL "")
  message(FATAL_ERROR "EXPECTED_SUITES_FILE is required")
endif()

if(NOT EXISTS "${EXPECTED_SUITES_FILE}")
  message(FATAL_ERROR "EXPECTED_SUITES_FILE does not exist: ${EXPECTED_SUITES_FILE}")
endif()

execute_process(
  COMMAND "${TEST_BINARY}" --list-test-suites
  RESULT_VARIABLE list_result
  OUTPUT_VARIABLE list_output
  ERROR_VARIABLE list_error
)

if(NOT list_result EQUAL 0)
  message(FATAL_ERROR "Failed to list doctest suites: ${list_error}")
endif()

string(REPLACE "\r\n" "\n" normalized_output "${list_output}")
string(REPLACE "\r" "\n" normalized_output "${normalized_output}")
string(REPLACE "\n" ";" output_lines "${normalized_output}")
set(discovered_suites "")
foreach(line IN LISTS output_lines)
  if(line MATCHES "^primestruct\\.")
    list(APPEND discovered_suites "${line}")
  endif()
endforeach()
list(REMOVE_DUPLICATES discovered_suites)
list(SORT discovered_suites)

file(STRINGS "${EXPECTED_SUITES_FILE}" expected_suites)
list(REMOVE_DUPLICATES expected_suites)
list(SORT expected_suites)

set(missing_in_cmake "")
foreach(suite IN LISTS discovered_suites)
  list(FIND expected_suites "${suite}" expected_index)
  if(expected_index EQUAL -1)
    list(APPEND missing_in_cmake "${suite}")
  endif()
endforeach()

set(missing_in_binary "")
foreach(suite IN LISTS expected_suites)
  list(FIND discovered_suites "${suite}" discovered_index)
  if(discovered_index EQUAL -1)
    list(APPEND missing_in_binary "${suite}")
  endif()
endforeach()

if(missing_in_cmake OR missing_in_binary)
  if(missing_in_cmake)
    string(JOIN ", " missing_cmake_text ${missing_in_cmake})
    message(STATUS "Doctest suites not registered in CMake: ${missing_cmake_text}")
  endif()
  if(missing_in_binary)
    string(JOIN ", " missing_binary_text ${missing_in_binary})
    message(STATUS "CMake suites missing in doctest binary: ${missing_binary_text}")
  endif()
  message(FATAL_ERROR "Doctest/CMake suite registration mismatch")
endif()

message(STATUS "Doctest suite registration is synchronized.")
