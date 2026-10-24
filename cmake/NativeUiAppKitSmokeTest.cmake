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

# The editor with a highlighted Unicode file, and with its File menu open: both
# pictures must be real PNGs with the text drawn in them (a blank window is a few
# KB, these are well over 50 KB).
execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/bundle_macos_app.sh"
          "${SOURCE_DIR}/examples/apps/text_editor/main.prime" EditorSmoke "${WORK_DIR}" "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "editor bundle failed (${rc}): ${out}${err}")
endif()
set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/showcase.prime")
foreach(shot gutter code menu tabs preview longtop scrolltext scrollpreview)
  set(snapshot "${WORK_DIR}/editor_${shot}.png")
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${snapshot}")
  if(shot STREQUAL "menu")
    set(ENV{PRIMESTRUCT_UI_MENU} "File")
  else()
    unset(ENV{PRIMESTRUCT_UI_MENU})
  endif()
  # "tabs": two New Tab commands make the editor open two more native tabs.
  if(shot STREQUAL "tabs")
    set(ENV{PRIMESTRUCT_UI_COMMANDS} "1,1")
  elseif(shot STREQUAL "preview")
    # View > Show Markdown Preview (command 10) on a markdown file: the page beside the source.
    set(ENV{PRIMESTRUCT_UI_COMMANDS} "10")
  else()
    unset(ENV{PRIMESTRUCT_UI_COMMANDS})
  endif()
  if(shot STREQUAL "preview")
    set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview.md")
  else()
    set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/showcase.prime")
  endif()
  # A long markdown file with the preview on, left at the top ("longtop"), or with one view scrolled to the
  # bottom by the harness: the other view must follow ("scrolltext" scrolls the source, "scrollpreview" the page).
  unset(ENV{PRIMESTRUCT_UI_SCROLL})
  if(shot STREQUAL "longtop" OR shot STREQUAL "scrolltext" OR shot STREQUAL "scrollpreview")
    set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview_long.md")
    set(ENV{PRIMESTRUCT_UI_COMMANDS} "10")
    if(shot STREQUAL "scrolltext")
      set(ENV{PRIMESTRUCT_UI_SCROLL} "text:1")
    elseif(shot STREQUAL "scrollpreview")
      set(ENV{PRIMESTRUCT_UI_SCROLL} "preview:1")
    endif()
  endif()
  # "gutter": the bar of line numbers, switched on through the preference override.
  if(shot STREQUAL "gutter")
    set(ENV{PRIMESTRUCT_UI_PREFS} "lineNumbers=1")
  else()
    unset(ENV{PRIMESTRUCT_UI_PREFS})
  endif()
  execute_process(
    COMMAND "${WORK_DIR}/EditorSmoke.app/Contents/MacOS/EditorSmoke"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
    TIMEOUT 90)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "editor ${shot} snapshot run exited with '${rc}': ${out}${err}")
  endif()
  if(NOT EXISTS "${snapshot}")
    message(FATAL_ERROR "no editor ${shot} snapshot written: ${out}${err}")
  endif()
  file(SIZE "${snapshot}" size)
  file(READ "${snapshot}" magic LIMIT 4 HEX)
  if(NOT magic STREQUAL "89504e47" OR size LESS 50000)
    message(FATAL_ERROR "editor ${shot} snapshot is not a drawn PNG (magic ${magic}, ${size} bytes)")
  endif()
  message(STATUS "editor ${shot} snapshot ok (${size} bytes)")
endforeach()

# Scrolling either view of a preview moved both: the pictures differ from the unscrolled one.
unset(ENV{PRIMESTRUCT_UI_SCROLL})
file(SHA256 "${WORK_DIR}/editor_longtop.png" longTopHash)
foreach(scrolled scrolltext scrollpreview)
  file(SHA256 "${WORK_DIR}/editor_${scrolled}.png" scrolledHash)
  if(scrolledHash STREQUAL longTopHash)
    message(FATAL_ERROR "scrolling (${scrolled}) did not change the picture of the preview")
  endif()
endforeach()
message(STATUS "scroll lock snapshots differ from the top ones")

# A settings window built from the stdlib controls (tabs, a form, a heading, check boxes, a pop-up): its picture,
# and two clicks (a check box and a pop-up item) that reach the program, which says so in the window's title.
execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/bundle_macos_app.sh"
          "${SOURCE_DIR}/tests/fixtures/ui/screenshot/settings_demo.prime" SettingsDemo "${WORK_DIR}" "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "settings demo bundle failed (${rc}): ${out}${err}")
endif()
unset(ENV{PRIMESTRUCT_UI_OPEN})
unset(ENV{PRIMESTRUCT_UI_COMMANDS})
unset(ENV{PRIMESTRUCT_UI_MENU})
unset(ENV{PRIMESTRUCT_UI_SCROLL})
foreach(shot settings settingsclick)
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/${shot}.png")
  if(shot STREQUAL "settingsclick")
    set(ENV{PRIMESTRUCT_UI_CLICK} "check:5;popup:7:3")
  else()
    unset(ENV{PRIMESTRUCT_UI_CLICK})
  endif()
  execute_process(
    COMMAND "${WORK_DIR}/SettingsDemo.app/Contents/MacOS/SettingsDemo"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
    TIMEOUT 90)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "settings demo ${shot} run exited with '${rc}': ${out}${err}")
  endif()
  file(SIZE "${WORK_DIR}/${shot}.png" size)
  file(READ "${WORK_DIR}/${shot}.png" magic LIMIT 4 HEX)
  if(NOT magic STREQUAL "89504e47" OR size LESS 10000)
    message(FATAL_ERROR "settings demo ${shot} snapshot is not a drawn PNG (magic ${magic}, ${size} bytes)")
  endif()
  message(STATUS "settings demo ${shot} snapshot ok (${size} bytes)")
endforeach()
unset(ENV{PRIMESTRUCT_UI_CLICK})
file(SHA256 "${WORK_DIR}/settings.png" settingsHash)
file(SHA256 "${WORK_DIR}/settingsclick.png" settingsClickHash)
if(settingsHash STREQUAL settingsClickHash)
  message(FATAL_ERROR "the clicks changed nothing in the settings window")
endif()
