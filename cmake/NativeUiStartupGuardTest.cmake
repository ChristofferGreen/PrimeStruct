# CTest driver of the launch guard (TODO-5603, docs/NativeUiPlan.md section 1): builds hello_window and PrimeScroll
# natively (the direct native build that bundles ship) and a plain AppKit reference program
# (tools/startup_probe/reference_app.m), then launches each app with scripts/measure_startup.sh in snapshot mode (no
# window on screen, no focus) and fails when it loads more images than the reference beyond a small allowance. A new
# framework link or an eager system lookup adds images (one framework adds about three); the time of a launch is
# printed but not checked, since it moves by 30 ms or more between runs and with the machine's load.
#
#   cmake -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DWORK_DIR=<scratch> -P NativeUiStartupGuardTest.cmake
foreach(var SOURCE_DIR BUILD_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NativeUiStartupGuardTest: ${var} is required")
  endif()
endforeach()

if(NOT GUARD_ENABLED)
  # CTest reads this line through SKIP_REGULAR_EXPRESSION and reports the test as skipped.
  message(STATUS "PRIMESTRUCT_UI_SKIPPED: the launch guard needs macOS and the AppKit backend")
  return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/NativeUiTestGuards.cmake")
if(PRIMESTRUCT_UI_SKIP)
  return()
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# The images an app may load beyond the reference: libps_ui_appkit.dylib, and the two frameworks AppKit loads for an
# app's own menu bar (CampoUIServices and MenuBarClientCore on macOS 27), plus one of slack. One more framework link
# (AVKit: AVKit, AVKitCore and PIP) goes past it.
set(maxExtraImages 4)

execute_process(
  COMMAND xcrun clang -fobjc-arc -O1 -framework AppKit -o "${WORK_DIR}/reference_app"
          "${SOURCE_DIR}/tools/startup_probe/reference_app.m"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the reference program did not build (${rc}): ${out}${err}")
endif()

function(guard_app program name)
  execute_process(
    COMMAND bash "${SOURCE_DIR}/scripts/build_native_app.sh" "${program}" "${WORK_DIR}/${name}/${name}"
            --backend appkit --direct --build-dir "${BUILD_DIR}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "native build of ${name} failed (${rc}): ${out}${err}")
  endif()
  execute_process(
    COMMAND bash "${SOURCE_DIR}/scripts/measure_startup.sh" --runs 3 --work-dir "${WORK_DIR}/${name}_measure"
            --reference "${WORK_DIR}/reference_app" --max-extra-images ${maxExtraImages} "${WORK_DIR}/${name}/${name}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  message(STATUS "${name}:\n${out}")
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "the launch guard failed for ${name} (${rc}): ${err}")
  endif()
endfunction()

guard_app("${SOURCE_DIR}/examples/native_ui/hello_window.prime" HelloWindow)
guard_app("${SOURCE_DIR}/examples/apps/text_editor/main.prime" PrimeScroll)
