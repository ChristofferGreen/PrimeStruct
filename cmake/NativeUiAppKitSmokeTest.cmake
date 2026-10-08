# CTest driver for the AppKit backend smoke test (TODO-5531): bundles an example
# as a .app, runs its bytecode-only executable in snapshot mode, and checks that
# a PNG of the laid-out window was written.
#
#   cmake -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DWORK_DIR=<scratch> -P NativeUiAppKitSmokeTest.cmake
foreach(var SOURCE_DIR BUILD_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NativeUiAppKitSmokeTest: ${var} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/bundle_macos_app.sh"
          "${SOURCE_DIR}/examples/native_ui/hello_window.prime" HelloSmoke "${WORK_DIR}" "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "bundle failed (${rc}): ${out}${err}")
endif()

set(snapshot "${WORK_DIR}/hello.png")
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${snapshot}")
execute_process(
  COMMAND "${WORK_DIR}/HelloSmoke.app/Contents/MacOS/HelloSmoke"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
  TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "bundled app exited with '${rc}': ${out}${err}")
endif()
if(NOT EXISTS "${snapshot}")
  message(FATAL_ERROR "no snapshot written: ${out}${err}")
endif()
file(SIZE "${snapshot}" size)
file(READ "${snapshot}" magic LIMIT 4 HEX)
if(NOT magic STREQUAL "89504e47")
  message(FATAL_ERROR "snapshot is not a PNG (magic ${magic})")
endif()
if(size LESS 2000)
  message(FATAL_ERROR "snapshot is suspiciously small (${size} bytes)")
endif()
message(STATUS "AppKit smoke test passed (${size} byte snapshot)")
