# CTest driver for compiled native UI programs on the AppKit backend (TODO-5559): builds the
# examples as native executables with scripts/build_native_app.sh (no VM, no bytecode) and runs
# them in snapshot mode, checking that PNGs of the laid-out windows were written.
#
#   cmake -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DWORK_DIR=<scratch> -P NativeUiCompiledAppTest.cmake
foreach(var SOURCE_DIR BUILD_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NativeUiCompiledAppTest: ${var} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(build_app program name)
  execute_process(
    COMMAND bash "${SOURCE_DIR}/scripts/build_native_app.sh" "${program}" "${WORK_DIR}/${name}"
            --backend appkit --build-dir "${BUILD_DIR}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "native build of ${name} failed (${rc}): ${out}${err}")
  endif()
endfunction()

function(check_png path minimum)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "no snapshot written: ${path}")
  endif()
  file(SIZE "${path}" size)
  file(READ "${path}" magic LIMIT 4 HEX)
  if(NOT magic STREQUAL "89504e47" OR size LESS ${minimum})
    message(FATAL_ERROR "${path} is not a drawn PNG (magic ${magic}, ${size} bytes)")
  endif()
  message(STATUS "${path} ok (${size} bytes)")
endfunction()

build_app("${SOURCE_DIR}/examples/native_ui/hello_window.prime" HelloNative)
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/hello.png")
execute_process(COMMAND "${WORK_DIR}/HelloNative" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "compiled hello exited with '${rc}': ${out}${err}")
endif()
check_png("${WORK_DIR}/hello.png" 2000)

build_app("${SOURCE_DIR}/examples/apps/text_editor/main.prime" EditorNative)
set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/showcase.prime")
foreach(shot code menu tabs)
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/editor_${shot}.png")
  if(shot STREQUAL "menu")
    set(ENV{PRIMESTRUCT_UI_MENU} "File")
  else()
    unset(ENV{PRIMESTRUCT_UI_MENU})
  endif()
  if(shot STREQUAL "tabs")
    set(ENV{PRIMESTRUCT_UI_COMMANDS} "1,1")
  else()
    unset(ENV{PRIMESTRUCT_UI_COMMANDS})
  endif()
  execute_process(COMMAND "${WORK_DIR}/EditorNative" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "compiled editor ${shot} run exited with '${rc}': ${out}${err}")
  endif()
  check_png("${WORK_DIR}/editor_${shot}.png" 50000)
endforeach()

# The file named on the command line opens in the first tab.
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/editor_argument.png")
unset(ENV{PRIMESTRUCT_UI_OPEN})
unset(ENV{PRIMESTRUCT_UI_MENU})
unset(ENV{PRIMESTRUCT_UI_COMMANDS})
execute_process(COMMAND "${WORK_DIR}/EditorNative" "${SOURCE_DIR}/tests/fixtures/ui/screenshot/showcase.prime"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "compiled editor with a file argument exited with '${rc}': ${out}${err}")
endif()
check_png("${WORK_DIR}/editor_argument.png" 50000)
