# CTest driver: installs PrimeStruct to a scratch prefix, configures and builds
# examples/embed against that installed package, then runs both hosts.
#
#   cmake -DBUILD_DIR=<primestruct build> -DSOURCE_DIR=<repo> -DPRIMEC=<primec>
#         -DWORK_DIR=<scratch> -DCONFIG=<build type> -P EmbedPackageTest.cmake
foreach(var BUILD_DIR SOURCE_DIR PRIMEC WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "EmbedPackageTest: ${var} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(run_step name)
  execute_process(COMMAND ${ARGN}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "EmbedPackageTest: step '${name}' failed (${rc})\n${out}\n${err}")
  endif()
  set(STEP_OUTPUT "${out}" PARENT_SCOPE)
endfunction()

run_step(install "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${WORK_DIR}/prefix")
run_step(configure "${CMAKE_COMMAND}" -S "${SOURCE_DIR}/examples/embed" -B "${WORK_DIR}/host"
         -DCMAKE_PREFIX_PATH=${WORK_DIR}/prefix -DCMAKE_BUILD_TYPE=Release)
run_step(build "${CMAKE_COMMAND}" --build "${WORK_DIR}/host")

# Built-in demo: 55 (sum 1..10) + 3 (program name + 2 args).
run_step(demo "${WORK_DIR}/host/embed_example")
if(NOT STEP_OUTPUT MATCHES "exitCode=58")
  message(FATAL_ERROR "EmbedPackageTest: demo output unexpected:\n${STEP_OUTPUT}")
endif()

# File mode with a script importing the stdlib (stdlib found via the package).
file(WRITE "${WORK_DIR}/math.prime"
     "import /std/math/*\n\n[return<int>]\nmain() {\n  return(convert<i32>(abs(-4.0f)))\n}\n")
set(ENV{PRIMESTRUCT_STDLIB} "${WORK_DIR}/prefix/share/primestruct/stdlib")
run_step(file_mode "${WORK_DIR}/host/embed_example" "${WORK_DIR}/math.prime")
if(NOT STEP_OUTPUT MATCHES "exit code 4")
  message(FATAL_ERROR "EmbedPackageTest: file mode output unexpected:\n${STEP_OUTPUT}")
endif()

# Runtime-only host runs bytecode emitted offline by primec.
file(WRITE "${WORK_DIR}/loop.prime"
     "[return<int>]\nmain() {\n  [mut] sum{0i32}\n  [mut] i{1i32}\n  while(i <= 10i32) {\n    sum = sum + i\n    i = i + 1i32\n  }\n  return(sum)\n}\n")
run_step(emit "${PRIMEC}" --emit=ir "${WORK_DIR}/loop.prime" -o "${WORK_DIR}/loop.psir")
run_step(bytecode "${WORK_DIR}/host/embed_bytecode_runner" "${WORK_DIR}/loop.psir")
if(NOT STEP_OUTPUT MATCHES "exit code 55")
  message(FATAL_ERROR "EmbedPackageTest: bytecode runner output unexpected:\n${STEP_OUTPUT}")
endif()
message(STATUS "EmbedPackageTest passed")
