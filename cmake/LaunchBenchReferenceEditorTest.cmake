# CTest driver of the launch-time harness's reference editor (docs/NativeUiPlan.md section 1): builds
# tools/launch_bench/reference_editor.mm with clang++ through scripts/measure_startup.sh and launches it twice in
# snapshot mode (a transparent window parked off screen, no focus), then checks that the harness got its phase marks
# (a `text` row with a number) and that the editor exited cleanly. Times are printed, never checked.
#
#   cmake -DSOURCE_DIR=<repo> -DWORK_DIR=<scratch> -P LaunchBenchReferenceEditorTest.cmake
foreach(var SOURCE_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "LaunchBenchReferenceEditorTest: ${var} is required")
  endif()
endforeach()

if(NOT GUARD_ENABLED)
  message(STATUS "PRIMESTRUCT_UI_SKIPPED: the reference editor needs macOS")
  return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/NativeUiTestGuards.cmake")
if(PRIMESTRUCT_UI_SKIP)
  return()
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/measure_startup.sh" --runs 2 --work-dir "${WORK_DIR}"
          "${SOURCE_DIR}/tools/launch_bench/reference_editor.mm"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message(STATUS "reference editor:\n${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the reference editor did not build or launch (${rc}): ${out}${err}")
endif()
if(NOT out MATCHES "launches: 2 measured")
  message(FATAL_ERROR "the harness did not read two launches of the reference editor:\n${out}")
endif()
foreach(phase main app_init loop window text)
  if(NOT out MATCHES "\n${phase} +[0-9]+ +[0-9]+ +[0-9]+  ms")
    message(FATAL_ERROR "the reference editor wrote no '${phase}' mark:\n${out}")
  endif()
endforeach()
