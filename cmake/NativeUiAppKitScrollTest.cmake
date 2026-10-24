# CTest driver for the preview scroll modes on the AppKit backend: runs the editor in snapshot mode on a markdown
# note with its preview on, scrolls a view or moves the cursor or types, and asserts where both views ended up from
# the report the harness writes (PRIMESTRUCT_UI_REPORT: text_line, text_fraction, caret_line, text_lines, web_line,
# web_y, web_max). docs/PreviewScrollTests.md lists the scenarios and what stays a by-hand test.
#
#   cmake -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DWORK_DIR=<scratch> -P NativeUiAppKitScrollTest.cmake
foreach(var SOURCE_DIR BUILD_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NativeUiAppKitScrollTest: ${var} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/bundle_macos_app.sh"
          "${SOURCE_DIR}/examples/apps/text_editor/main.prime" ScrollSmoke "${WORK_DIR}" "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "editor bundle failed (${rc}): ${out}${err}")
endif()

set(Long "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview_long.md")
set(Uneven "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview_uneven.md")
set(Failures "")

# Runs one scenario and leaves the report's values in report_<key> variables.
#   run(<name> <mode> <note> [SCROLL <spec>] [CARET <line>] [TYPE <text>] [AFTER_SCROLL])
function(run name mode note)
  cmake_parse_arguments(ARG "AFTER_SCROLL" "SCROLL;CARET;TYPE" "" ${ARGN})
  set(report "${WORK_DIR}/${name}.txt")
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/${name}.png")
  set(ENV{PRIMESTRUCT_UI_REPORT} "${report}")
  set(ENV{PRIMESTRUCT_UI_OPEN} "${note}")
  set(ENV{PRIMESTRUCT_UI_COMMANDS} "10")
  set(ENV{PRIMESTRUCT_UI_PREFS} "scrollMode=${mode},restoreSession=0")
  foreach(var SCROLL CARET TYPE)
    if(DEFINED ARG_${var})
      set(ENV{PRIMESTRUCT_UI_${var}} "${ARG_${var}}")
    else()
      unset(ENV{PRIMESTRUCT_UI_${var}})
    endif()
  endforeach()
  if(DEFINED ARG_TYPE)
    set(ENV{PRIMESTRUCT_UI_TYPE} "${ARG_TYPE}")
  endif()
  if(ARG_AFTER_SCROLL)
    set(ENV{PRIMESTRUCT_UI_TYPE_AFTER_SCROLL} "1")
  else()
    unset(ENV{PRIMESTRUCT_UI_TYPE_AFTER_SCROLL})
  endif()
  execute_process(
    COMMAND "${WORK_DIR}/ScrollSmoke.app/Contents/MacOS/ScrollSmoke"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
    TIMEOUT 90)
  if(NOT rc EQUAL 0 OR NOT EXISTS "${report}")
    message(FATAL_ERROR "scenario ${name}: run exited with '${rc}' and no report: ${out}${err}")
  endif()
  file(STRINGS "${report}" lines)
  set(shown "")
  foreach(line IN LISTS lines)
    if(line MATCHES "^([a-z_]+)=(.*)$")
      set(report_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
      string(APPEND shown " ${CMAKE_MATCH_1}=${CMAKE_MATCH_2}")
    endif()
  endforeach()
  message(STATUS "${name} (mode ${mode}):${shown}")
endfunction()

# expect(<name> <expression python>) evaluates a Python expression over the report's numbers.
function(expect name what expression)
  set(prelude "")
  foreach(key mode text_line text_fraction caret_line text_lines web_line web_y web_max)
    string(APPEND prelude "${key}=float('${report_${key}}');")
  endforeach()
  execute_process(
    COMMAND python3 -I -c "${prelude}\nimport sys\nsys.exit(0 if (${expression}) else 1)"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    set(Failures "${Failures}\n  ${name}: ${what}  [${expression}]" PARENT_SCOPE)
  endif()
endfunction()

# Modes: 0 independent, 1 linear, 2 by block, 3 with the cursor.

# Independent: scrolling one view leaves the other where it was.
run(off_text 0 "${Long}" SCROLL text:0.5)
expect(off_text "the source scrolled" "text_line > 40")
expect(off_text "the page stayed" "web_y == 0")
run(off_page 0 "${Long}" SCROLL preview:0.5)
expect(off_page "the page scrolled" "web_y > 100")
expect(off_page "the source stayed" "text_line < 1")
# ... and a reload after typing keeps an independent page where it was.
run(off_page_type 0 "${Long}" SCROLL preview:0.5 CARET 20 TYPE x AFTER_SCROLL)
expect(off_page_type "the page is still scrolled after the reload" "web_y > 100")

# Linear: the same fraction of what each can scroll.
run(linear_text 1 "${Long}" SCROLL text:0.5)
expect(linear_text "the source is halfway" "abs(text_fraction - 0.5) < 0.02")
expect(linear_text "the page is at the same fraction" "web_max > 0 and abs(web_y / web_max - text_fraction) < 0.03")
run(linear_page 1 "${Long}" SCROLL preview:0.5)
expect(linear_page "the page is halfway" "web_max > 0 and abs(web_y / web_max - 0.5) < 0.02")
expect(linear_page "the source is at the same fraction" "abs(text_fraction - web_y / web_max) < 0.03")
run(linear_type 1 "${Long}" SCROLL text:0.5 CARET 100 TYPE x AFTER_SCROLL)
expect(linear_type "after the reload the page is back at the source's fraction"
       "web_max > 0 and abs(web_y / web_max - text_fraction) < 0.06 and text_fraction > 0.3 and text_fraction < 0.7")

# By block: the same source line.
run(blocks_text 2 "${Long}" SCROLL text:0.5)
expect(blocks_text "the source scrolled" "text_line > 40")
expect(blocks_text "the page is at the same line" "abs(web_line - text_line) < 3")
run(blocks_page 2 "${Long}" SCROLL preview:0.33)
expect(blocks_page "the page scrolled" "web_y > 100")
expect(blocks_page "the source is at the same line" "text_line > 20 and abs(web_line - text_line) < 3")
run(blocks_uneven_text 2 "${Uneven}" SCROLL text:0.5)
expect(blocks_uneven_text "the page is at the same line in a page of tall blocks" "text_line > 20 and abs(web_line - text_line) < 4")
run(blocks_uneven_page 2 "${Uneven}" SCROLL preview:0.5)
expect(blocks_uneven_page "the source is at the same line in a page of tall blocks" "text_line > 20 and abs(web_line - text_line) < 4")
run(blocks_text_end 2 "${Long}" SCROLL text:1)
expect(blocks_text_end "the end of the source is the end of the page" "web_max > 0 and web_max - web_y < 2")
run(blocks_page_end 2 "${Long}" SCROLL preview:1)
expect(blocks_page_end "the end of the page is the end of the source" "text_fraction > 0.99")
run(blocks_type 2 "${Long}" SCROLL text:0.5 CARET 100 TYPE x AFTER_SCROLL)
expect(blocks_type "after the reload the page is back at the source's line (not at the top or the end)"
       "text_line > 60 and text_line < 150 and web_y > 100 and web_max - web_y > 100 and abs(web_line - text_line) < 4")

# With the cursor: the page follows the cursor; scrolling a view alone moves nothing.
run(cursor_caret 3 "${Long}" CARET 150)
expect(cursor_caret "the cursor is on line 150" "caret_line == 150")
expect(cursor_caret "the page shows the cursor's line" "web_y > 100 and web_line <= 150 and 150 - web_line < 60")
expect(cursor_caret "the source did not move" "text_line < 1")
run(cursor_text 3 "${Long}" SCROLL text:0.5)
expect(cursor_text "scrolling the source leaves the page" "web_y == 0")
run(cursor_page 3 "${Long}" SCROLL preview:0.5)
expect(cursor_page "scrolling the page leaves the source" "text_line < 1 and web_y > 100")
run(cursor_type 3 "${Long}" CARET 150 TYPE x)
expect(cursor_type "an edit does not send the page to the top" "web_y > 100 and web_line <= 151 and 151 - web_line < 60")
expect(cursor_type "the source's top line is believable" "text_line > 100 and text_line < 200")
run(cursor_uneven 3 "${Uneven}" CARET 100)
expect(cursor_uneven "the page shows the cursor's line in a page of tall blocks" "web_y > 100 and web_line <= 100")

if(NOT Failures STREQUAL "")
  message(FATAL_ERROR "preview scroll scenarios failed:${Failures}")
endif()
message(STATUS "preview scroll scenarios passed")
