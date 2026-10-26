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
set(ENV{PRIMESTRUCT_UI_COMMANDS} "10")
execute_process(COMMAND "${WORK_DIR}/NativeBundle.app/Contents/MacOS/NativeBundle"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "native bundle with a preview exited with '${rc}': ${out}${err}")
endif()
check_png("${WORK_DIR}/bundle_preview.png" 50000)
unset(ENV{PRIMESTRUCT_UI_COMMANDS})
