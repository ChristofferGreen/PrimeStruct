# CTest driver for compiled native UI programs on the AppKit backend (TODO-5559): builds the
# examples as native executables with scripts/build_native_app.sh (no VM, no bytecode) and runs
# them in snapshot mode, checking that PNGs of the laid-out windows were written. The hello example
# is also built with --direct, by primec's own native backend and the AppKit dylib, and must draw the
# same picture.
#
#   cmake -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DWORK_DIR=<scratch> -P NativeUiCompiledAppTest.cmake
foreach(var SOURCE_DIR BUILD_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NativeUiCompiledAppTest: ${var} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/NativeUiTestGuards.cmake")
if(PRIMESTRUCT_UI_SKIP)
  return()
endif()

# This test does not look at the bundle's document types: leave them out, so the bundle cannot become the handler of
# the user's files in Launch Services.
set(ENV{PS_NO_DOCUMENT_TYPES} "1")
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

# Built by primec's native backend (no C++ compiler), loading the AppKit backend as a dylib.
execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/build_native_app.sh" "${SOURCE_DIR}/examples/native_ui/hello_window.prime"
          "${WORK_DIR}/direct/HelloDirect" --backend appkit --direct --build-dir "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "direct native build of hello failed (${rc}): ${out}${err}")
endif()
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/hello_direct.png")
execute_process(COMMAND "${WORK_DIR}/direct/HelloDirect" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "direct native hello exited with '${rc}': ${out}${err}")
endif()
check_png("${WORK_DIR}/hello_direct.png" 2000)
# The window has the same size and look as the one the C++ build draws (the PNG header holds the pixel size;
# the pictures themselves are not byte-identical from run to run).
file(READ "${WORK_DIR}/hello.png" helloHeader LIMIT 24 HEX)
file(READ "${WORK_DIR}/hello_direct.png" helloDirectHeader LIMIT 24 HEX)
if(NOT helloHeader STREQUAL helloDirectHeader)
  message(FATAL_ERROR "the direct native hello drew a window of another size than the C++ build "
                      "(${helloDirectHeader} against ${helloHeader})")
endif()

# Pointer and key handlers (TODO-5725, tests/fixtures/ui/input_handlers.prime): the harness moves the pointer onto
# button 4 (its tracking area's owner hears of it), presses and releases it at (10, 5) (found by hit testing, not
# handed to the button) and clicks it, then types "a" and Escape into text view 2, which types the first itself and
# hands the second to the program. Nothing takes focus: the keys go to a window that is not key. The exit code has a
# bit per kind of event the handlers saw as expected; 127 is all of them.
build_app("${SOURCE_DIR}/tests/fixtures/ui/input_handlers.prime" InputNative)
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/input.png")
set(ENV{PRIMESTRUCT_UI_CLICK} "enter:4:10:5;press:4:10:5;button:4;key:2:97:0;key:2:-3:0")
execute_process(COMMAND "${WORK_DIR}/InputNative" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
unset(ENV{PRIMESTRUCT_UI_CLICK})
if(NOT rc EQUAL 127)
  message(FATAL_ERROR "compiled input handlers exited with '${rc}' instead of 127 (a bit per event seen): ${out}${err}")
endif()
check_png("${WORK_DIR}/input.png" 2000)

build_app("${SOURCE_DIR}/examples/apps/text_editor/main.prime" EditorNative)

# WebKit is loaded when a preview turns on, never linked: neither the compiled editor nor the AppKit dylib
# lists it, so a session that does not preview pays nothing for it.
foreach(binary "${WORK_DIR}/EditorNative" "${BUILD_DIR}/libps_ui_appkit.dylib")
  execute_process(COMMAND otool -L "${binary}" OUTPUT_VARIABLE linked RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0 OR linked MATCHES "WebKit")
    message(FATAL_ERROR "${binary} links WebKit or cannot be inspected: ${linked}")
  endif()
endforeach()
set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/showcase.prime")
foreach(shot gutter code menu tabs)
  if(shot STREQUAL "menu" AND NOT PRIMESTRUCT_UI_INTRUSIVE)
    message(STATUS "skipping the open-menu snapshot (it grabs the mouse); set PRIMESTRUCT_TEST_UI_INTRUSIVE=1 to run it")
    continue()
  endif()
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/editor_${shot}.png")
  if(shot STREQUAL "menu")
    set(ENV{PRIMESTRUCT_UI_MENU} "File")
  else()
    unset(ENV{PRIMESTRUCT_UI_MENU})
  endif()
  if(shot STREQUAL "tabs")
    set(ENV{PRIMESTRUCT_UI_COMMANDS} "1000000,1000000")
  else()
    unset(ENV{PRIMESTRUCT_UI_COMMANDS})
  endif()
  # "gutter": the bar of line numbers, switched on through the preference override.
  if(shot STREQUAL "gutter")
    set(ENV{PRIMESTRUCT_UI_PREFS} "lineNumbers=1")
  else()
    unset(ENV{PRIMESTRUCT_UI_PREFS})
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

# The last session comes back after the first frame (TODO-5753): the window is drawn with no tab of the session, then
# the tabs open in their order and the active one is selected (PRIMESTRUCT_UI_TABS_REPORT lists them at the picture,
# with how many windows there were when the first frame came).
set(sessionText "${SOURCE_DIR}/tests/fixtures/ui/unicode_sample.txt")
set(sessionScript "${SOURCE_DIR}/tests/fixtures/ui/screenshot/showcase.prime")
set(sessionNote "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview.md")
set(launchDocument "${SOURCE_DIR}/tests/fixtures/ui/screenshot/toolbar_demo.prime")
function(check_session_tabs name expected)
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/editor_${name}.png")
  set(ENV{PRIMESTRUCT_UI_LISTS} "session.files=${sessionText}:${sessionScript}:${sessionNote};session.previews=0:0:1")
  set(ENV{PRIMESTRUCT_UI_PREFS} "session.active=1")
  set(ENV{PRIMESTRUCT_UI_TABS_REPORT} "${WORK_DIR}/${name}_tabs.txt")
  execute_process(COMMAND "${WORK_DIR}/EditorNative" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
  unset(ENV{PRIMESTRUCT_UI_LISTS})
  unset(ENV{PRIMESTRUCT_UI_PREFS})
  unset(ENV{PRIMESTRUCT_UI_TABS_REPORT})
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "compiled editor ${name} run exited with '${rc}': ${out}${err}")
  endif()
  # The picture is of the first window, which is a tab behind the selected one in the session run, so its text need
  # not be laid out: only check that it was drawn.
  check_png("${WORK_DIR}/editor_${name}.png" 5000)
  file(READ "${WORK_DIR}/${name}_tabs.txt" tabs)
  if(NOT tabs STREQUAL expected)
    message(FATAL_ERROR "the ${name} launch has these tabs:\n${tabs}\ninstead of:\n${expected}")
  endif()
endfunction()
check_session_tabs(session
  "first_frame=1\nwindows_at_first_frame=1\ntab=${sessionText}\ntab=*${sessionScript}\ntab=${sessionNote}\n")
# Finder launches the app to open a document: the request is there before the first wait, the document opens in the
# first window, and the session's tabs open behind it without taking the selection.
set(ENV{PRIMESTRUCT_UI_OPEN_FILE_EVENT} "!${launchDocument}")
check_session_tabs(launch
  "first_frame=1\nwindows_at_first_frame=1\ntab=*${launchDocument}\ntab=${sessionText}\ntab=${sessionScript}\ntab=${sessionNote}\n")
unset(ENV{PRIMESTRUCT_UI_OPEN_FILE_EVENT})

# The native bundle: Name.app holds the compiled executable (no runtime, no bytecode) and launches.
execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/bundle_macos_app.sh" --native
          "${SOURCE_DIR}/examples/apps/text_editor/main.prime" NativeBundle "${WORK_DIR}" "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "native bundle failed (${rc}): ${out}${err}")
endif()
if(EXISTS "${WORK_DIR}/NativeBundle.app/Contents/Resources/app.psir")
  message(FATAL_ERROR "the native bundle carries bytecode")
endif()
# The bundle is the direct native build: the AppKit backend sits beside the executable, and the signatures hold.
if(NOT EXISTS "${WORK_DIR}/NativeBundle.app/Contents/MacOS/libps_ui_appkit.dylib")
  message(FATAL_ERROR "the native bundle has no AppKit dylib")
endif()
execute_process(COMMAND codesign --verify --deep --strict "${WORK_DIR}/NativeBundle.app"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the native bundle's signature does not verify (${rc}): ${out}${err}")
endif()
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/bundle.png")
set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/showcase.prime")
execute_process(COMMAND "${WORK_DIR}/NativeBundle.app/Contents/MacOS/NativeBundle"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "native bundle exited with '${rc}': ${out}${err}")
endif()
check_png("${WORK_DIR}/bundle.png" 50000)

# The native bundle with the markdown preview turned on (the page is drawn by a web view loaded on demand).
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/bundle_preview.png")
set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview.md")
set(ENV{PRIMESTRUCT_UI_COMMANDS} "1000008")
execute_process(COMMAND "${WORK_DIR}/NativeBundle.app/Contents/MacOS/NativeBundle"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "native bundle with a preview exited with '${rc}': ${out}${err}")
endif()
check_png("${WORK_DIR}/bundle_preview.png" 50000)
unset(ENV{PRIMESTRUCT_UI_COMMANDS})
