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
foreach(shot gutter code menu tabs preview longtop scrolltext scrollpreview scrolluneven scrollpageuneven scrollpagelong)
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
  # A page with a tall code block and a table, the source scrolled to the middle: the page follows by source
  # line, so both views show the same part of the text although the blocks differ in height.
  if(shot STREQUAL "scrollpagelong")
    # A long page scrolled a third of the way: the source, whose lower part is not laid out yet, must follow.
    set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview_long.md")
    set(ENV{PRIMESTRUCT_UI_COMMANDS} "10")
    set(ENV{PRIMESTRUCT_UI_SCROLL} "preview:0.33")
  endif()
  if(shot STREQUAL "scrolluneven" OR shot STREQUAL "scrollpageuneven")
    set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview_uneven.md")
    set(ENV{PRIMESTRUCT_UI_COMMANDS} "10")
    # The same place reached from the other side: the page scrolled halfway, the source must follow it.
    set(ENV{PRIMESTRUCT_UI_SCROLL} "text:0.5")
    if(shot STREQUAL "scrollpageuneven")
      set(ENV{PRIMESTRUCT_UI_SCROLL} "preview:0.5")
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

# The editor's settings window on its Files tab (the fourth page of the tabs control 21): the list of file types with
# the application that opens each, and the same with two rows selected through the harness (the list is control 24).
foreach(var OPEN SCROLL CARET TYPE TYPE_AFTER_SCROLL MENU REPORT CLICK)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
set(ENV{PRIMESTRUCT_UI_PREFS} "restoreSession=0")
set(ENV{PRIMESTRUCT_UI_COMMANDS} "8")
set(ENV{PRIMESTRUCT_UI_TARGET_LAST} "1")
foreach(shot files filesrows)
  set(snapshot "${WORK_DIR}/editor_${shot}.png")
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${snapshot}")
  if(shot STREQUAL "filesrows")
    set(ENV{PRIMESTRUCT_UI_CLICK} "tab:21:3;rows:24:1,2")
  else()
    set(ENV{PRIMESTRUCT_UI_CLICK} "tab:21:3")
  endif()
  execute_process(
    COMMAND "${WORK_DIR}/EditorSmoke.app/Contents/MacOS/EditorSmoke"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
    TIMEOUT 90)
  if(NOT rc EQUAL 0 OR NOT EXISTS "${snapshot}")
    message(FATAL_ERROR "editor ${shot} snapshot run exited with '${rc}' and no picture: ${out}${err}")
  endif()
  file(SIZE "${snapshot}" size)
  file(READ "${snapshot}" magic LIMIT 4 HEX)
  if(NOT magic STREQUAL "89504e47" OR size LESS 10000)
    message(FATAL_ERROR "editor ${shot} snapshot is not a drawn PNG (magic ${magic}, ${size} bytes)")
  endif()
  message(STATUS "editor ${shot} snapshot ok (${size} bytes)")
endforeach()
foreach(var COMMANDS TARGET_LAST CLICK PREFS)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
file(SHA256 "${WORK_DIR}/editor_files.png" filesHash)
file(SHA256 "${WORK_DIR}/editor_filesrows.png" filesRowsHash)
if(filesHash STREQUAL filesRowsHash)
  message(FATAL_ERROR "selecting rows of the file type list changed nothing in the settings window")
endif()

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
foreach(shot settings settingsclick settingsbutton)
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/${shot}.png")
  if(shot STREQUAL "settingsclick")
    set(ENV{PRIMESTRUCT_UI_CLICK} "check:5;popup:7:3")
  elseif(shot STREQUAL "settingsbutton")
    set(ENV{PRIMESTRUCT_UI_CLICK} "button:9") # the "Make Default" button added last: handle 9
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
file(SHA256 "${WORK_DIR}/settingsbutton.png" settingsButtonHash)
if(settingsHash STREQUAL settingsButtonHash)
  message(FATAL_ERROR "clicking the button changed nothing in the settings window")
endif()

# The editor's bundle declares its file types (file_types.txt next to the program): the document types, and the
# exported type of .prime; the plist must be valid.
file(READ "${WORK_DIR}/EditorSmoke.app/Contents/Info.plist" editorPlist)
foreach(needle "CFBundleDocumentTypes" "UTExportedTypeDeclarations" "org.primestruct.prime-source" "public.data"
               "<string>Any file</string>"
               "<string>Markdown</string>" "<string>prime</string>" "<string>markdown</string>")
  string(FIND "${editorPlist}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "the editor's Info.plist does not contain ${needle}")
  endif()
endforeach()
execute_process(COMMAND plutil -lint "${WORK_DIR}/EditorSmoke.app/Contents/Info.plist"
                RESULT_VARIABLE lintRc OUTPUT_VARIABLE lintOut ERROR_VARIABLE lintErr)
if(NOT lintRc EQUAL 0)
  message(FATAL_ERROR "the editor's Info.plist is not valid: ${lintOut}${lintErr}")
endif()
message(STATUS "editor Info.plist declares its file types")

# A file dropped on the editor (the harness runs the code of a real drop), and a file of an undeclared kind opened
# the way the Finder does it (Launch Services, `open -a`): each opens in the editor, so its picture shows the file
# (its path as the window's title) and differs from the editor started with nothing.
set(unknownFile "${WORK_DIR}/dropped_note.unknownext")
file(WRITE "${unknownFile}" "A file with an extension the editor does not declare.\n")
foreach(var OPEN SCROLL CARET TYPE TYPE_AFTER_SCROLL MENU REPORT CLICK COMMANDS DROP)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
set(ENV{PRIMESTRUCT_UI_PREFS} "restoreSession=0")
foreach(shot opennothing opendrop openfinder)
  set(snapshot "${WORK_DIR}/editor_${shot}.png")
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${snapshot}")
  if(shot STREQUAL "opendrop")
    set(ENV{PRIMESTRUCT_UI_DROP} "${unknownFile}")
  else()
    unset(ENV{PRIMESTRUCT_UI_DROP})
  endif()
  if(shot STREQUAL "openfinder")
    execute_process(
      COMMAND open -n -W -a "${WORK_DIR}/EditorSmoke.app"
              --env "PRIMESTRUCT_UI_SNAPSHOT=${snapshot}" --env "PRIMESTRUCT_UI_PREFS=restoreSession=0" "${unknownFile}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
      TIMEOUT 90)
  else()
    execute_process(
      COMMAND "${WORK_DIR}/EditorSmoke.app/Contents/MacOS/EditorSmoke"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
      TIMEOUT 90)
  endif()
  if(NOT rc EQUAL 0 OR NOT EXISTS "${snapshot}")
    message(FATAL_ERROR "editor ${shot} run exited with '${rc}' and no picture: ${out}${err}")
  endif()
  file(SIZE "${snapshot}" size)
  message(STATUS "editor ${shot} snapshot ok (${size} bytes)")
endforeach()
unset(ENV{PRIMESTRUCT_UI_PREFS})
unset(ENV{PRIMESTRUCT_UI_DROP})
file(SHA256 "${WORK_DIR}/editor_opennothing.png" nothingHash)
foreach(shot opendrop openfinder)
  file(SHA256 "${WORK_DIR}/editor_${shot}.png" shotHash)
  if(shotHash STREQUAL nothingHash)
    message(FATAL_ERROR "the editor did not open the file of ${shot}")
  endif()
endforeach()
message(STATUS "a dropped file and a Launch Services open of an undeclared file type open in the editor")

# A program that reports the declared file types, the app's path and the default application of .txt files into
# a file, and the files the system asks it to open (the harness stands in for the Finder).
set(ENV{PS_DOCUMENT_TYPES} "${SOURCE_DIR}/examples/apps/text_editor/file_types.txt")
execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/bundle_macos_app.sh"
          "${SOURCE_DIR}/tests/fixtures/ui/screenshot/file_types_demo.prime" FileTypesDemo "${WORK_DIR}" "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
unset(ENV{PS_DOCUMENT_TYPES})
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "file types demo bundle failed (${rc}): ${out}${err}")
endif()
set(typesReport "${WORK_DIR}/types_report.txt")
file(REMOVE "${typesReport}")
unset(ENV{PRIMESTRUCT_UI_CLICK})
unset(ENV{PRIMESTRUCT_UI_PREFS})
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/types.png")
set(ENV{PRIMESTRUCT_UI_OPEN_FILE_EVENT} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview.md")
execute_process(
  COMMAND "${WORK_DIR}/FileTypesDemo.app/Contents/MacOS/FileTypesDemo" "${typesReport}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
  TIMEOUT 90)
unset(ENV{PRIMESTRUCT_UI_OPEN_FILE_EVENT})
if(NOT rc EQUAL 0 OR NOT EXISTS "${typesReport}")
  message(FATAL_ERROR "file types demo exited with '${rc}' and no report: ${out}${err}")
endif()
file(READ "${typesReport}" typesText)
message(STATUS "file types report:\n${typesText}")
foreach(needle "Markdown|md,markdown" "PrimeStruct source|prime" "Plain text|txt,text" "app:" "FileTypesDemo.app"
               "txt:" "opened:${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview.md")
  string(FIND "${typesText}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "the file types report does not contain ${needle}")
  endif()
endforeach()
message(STATUS "file types, the app path, the default application and an open-file request reach a program")

# Outside a bundle the runner still takes its first argument for the program: the same bytecode run by path, with the
# report file as the program's only argument.
set(pathReport "${WORK_DIR}/types_report_by_path.txt")
file(REMOVE "${pathReport}")
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/types_by_path.png")
execute_process(
  COMMAND "${BUILD_DIR}/primestruct_app_runtime" "${WORK_DIR}/FileTypesDemo.app/Contents/Resources/app.psir" "${pathReport}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
  TIMEOUT 90)
if(NOT rc EQUAL 0 OR NOT EXISTS "${pathReport}")
  message(FATAL_ERROR "the runner outside a bundle exited with '${rc}' and no report: ${out}${err}")
endif()
file(READ "${pathReport}" pathText)
# (Outside a bundle there is no Info.plist: no declared file types and no app path, but the program ran and its
# argument reached it.)
foreach(needle "app:" "txt:")
  string(FIND "${pathText}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "the runner outside a bundle did not run the program with its argument (${needle}): ${pathText}")
  endif()
endforeach()
message(STATUS "the runner outside a bundle still takes the program path first")

# Make Default for every file type of the Files list, as a dry run (PRIMESTRUCT_UI_DEFAULT_APP_PLAN: the content types the
# system would be asked about are written to the file and nothing is changed): all selected types go in one request, and
# a content type that several extensions share (txt and text, md and markdown, ...) is asked about once.
foreach(var OPEN SCROLL CARET TYPE TYPE_AFTER_SCROLL MENU REPORT DROP DROP_PREVIEW)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
set(planFile "${WORK_DIR}/default_app_plan.txt")
file(REMOVE "${planFile}")
set(ENV{PRIMESTRUCT_UI_PREFS} "restoreSession=0")
set(ENV{PRIMESTRUCT_UI_COMMANDS} "8")
set(ENV{PRIMESTRUCT_UI_TARGET_LAST} "1")
set(ENV{PRIMESTRUCT_UI_DEFAULT_APP_PLAN} "${planFile}")
set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/editor_plan.png")
set(ENV{PRIMESTRUCT_UI_CLICK} "tab:21:3;rows:24:0,1,2,3,4,5,6,7,8,9,10;button:25")
execute_process(
  COMMAND "${WORK_DIR}/EditorSmoke.app/Contents/MacOS/EditorSmoke"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
  TIMEOUT 90)
foreach(var COMMANDS TARGET_LAST DEFAULT_APP_PLAN CLICK PREFS)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
if(NOT rc EQUAL 0 OR NOT EXISTS "${planFile}")
  message(FATAL_ERROR "the Make Default dry run exited with '${rc}' and wrote no plan: ${out}${err}")
endif()
file(STRINGS "${planFile}" planTypes)
set(uniqueTypes ${planTypes})
list(REMOVE_DUPLICATES uniqueTypes)
list(LENGTH planTypes planCount)
list(LENGTH uniqueTypes uniqueCount)
message(STATUS "Make Default would ask about ${planCount} content types for 25 extensions: ${planTypes}")
if(NOT planCount EQUAL uniqueCount)
  message(FATAL_ERROR "a content type would be asked about twice: ${planTypes}")
endif()
if(planCount LESS 12 OR NOT planCount LESS 25)
  message(FATAL_ERROR "expected fewer content types than the 25 extensions (the shared ones count once), got ${planCount}")
endif()
list(FIND planTypes "org.primestruct.prime-source" primeIndex)
if(primeIndex EQUAL -1)
  message(FATAL_ERROR "the .prime type is not among the types Make Default would ask about: ${planTypes}")
endif()

# A file dropped on the markdown preview (a web view): the editor opens it in a tab of its own. The picture is the last
# window, the new tab with the file, and differs from the one of the same run without the drop.
foreach(shot previewonly previewdrop)
  set(snapshot "${WORK_DIR}/editor_${shot}.png")
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${snapshot}")
  set(ENV{PRIMESTRUCT_UI_PREFS} "restoreSession=0")
  set(ENV{PRIMESTRUCT_UI_OPEN} "${SOURCE_DIR}/tests/fixtures/ui/screenshot/preview.md")
  set(ENV{PRIMESTRUCT_UI_COMMANDS} "10")
  set(ENV{PRIMESTRUCT_UI_TARGET_LAST} "1")
  if(shot STREQUAL "previewdrop")
    set(ENV{PRIMESTRUCT_UI_DROP_PREVIEW} "${unknownFile}")
  else()
    unset(ENV{PRIMESTRUCT_UI_DROP_PREVIEW})
  endif()
  execute_process(
    COMMAND "${WORK_DIR}/EditorSmoke.app/Contents/MacOS/EditorSmoke"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
    TIMEOUT 90)
  if(NOT rc EQUAL 0 OR NOT EXISTS "${snapshot}")
    message(FATAL_ERROR "editor ${shot} run exited with '${rc}' and no picture: ${out}${err}")
  endif()
endforeach()
foreach(var OPEN COMMANDS TARGET_LAST DROP_PREVIEW PREFS)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
file(SHA256 "${WORK_DIR}/editor_previewonly.png" previewOnlyHash)
file(SHA256 "${WORK_DIR}/editor_previewdrop.png" previewDropHash)
if(previewOnlyHash STREQUAL previewDropHash)
  message(FATAL_ERROR "a file dropped on the markdown preview did not open in the editor")
endif()
message(STATUS "a file dropped on the markdown preview opens in the editor")

# A markdown window with a toolbar (segments, Bold, a Table menu): the pictures show the toolbar in every preview mode,
# and the commands change the text through the selection as one undoable edit and switch the mode.
execute_process(
  COMMAND bash "${SOURCE_DIR}/scripts/bundle_macos_app.sh"
          "${SOURCE_DIR}/tests/fixtures/ui/screenshot/toolbar_demo.prime" ToolbarDemo "${WORK_DIR}" "${BUILD_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "toolbar demo bundle failed (${rc}): ${out}${err}")
endif()
foreach(var OPEN COMMANDS MENU SCROLL CLICK SELECT UNDO PREFS OPEN_FILE_EVENT DROP DROP_PREVIEW TARGET_LAST TYPE
               TYPE_AFTER_SCROLL CARET REPORT DEFAULT_APP_PLAN)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
# name|selection|commands|undo
set(toolbarRuns "toolbar|||" "toolbarbold|8:12|20|" "toolbarundo|8:12|20|1" "toolbarpreview||3|" "toolbarsplit||2|"
                "toolbarformat|22:22|41|" "toolbaradd|22:22|42|" "toolbarinsert|0:0|40|")
foreach(run IN LISTS toolbarRuns)
  string(REGEX MATCH "^([^|]*)\\|([^|]*)\\|([^|]*)\\|([^|]*)$" ignored "${run}")
  set(name "${CMAKE_MATCH_1}")
  set(selection "${CMAKE_MATCH_2}")
  set(commands "${CMAKE_MATCH_3}")
  set(undo "${CMAKE_MATCH_4}")
  set(textReport "${WORK_DIR}/${name}_text.txt")
  set(modeReport "${WORK_DIR}/${name}_mode.txt")
  file(REMOVE "${textReport}" "${modeReport}")
  set(ENV{PRIMESTRUCT_UI_SNAPSHOT} "${WORK_DIR}/${name}.png")
  foreach(var SELECT COMMANDS UNDO)
    string(TOLOWER "${var}" lowered)
    if(var STREQUAL "SELECT")
      set(value "${selection}")
    elseif(var STREQUAL "COMMANDS")
      set(value "${commands}")
    else()
      set(value "${undo}")
    endif()
    if(value STREQUAL "")
      unset(ENV{PRIMESTRUCT_UI_${var}})
    else()
      set(ENV{PRIMESTRUCT_UI_${var}} "${value}")
    endif()
  endforeach()
  execute_process(
    COMMAND "${WORK_DIR}/ToolbarDemo.app/Contents/MacOS/ToolbarDemo" "${textReport}" "${modeReport}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
    TIMEOUT 90)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "toolbar demo ${name} run exited with '${rc}': ${out}${err}")
  endif()
  file(SIZE "${WORK_DIR}/${name}.png" size)
  if(size LESS 10000)
    message(FATAL_ERROR "toolbar demo ${name} snapshot is not a drawn picture (${size} bytes)")
  endif()
endforeach()
foreach(var SNAPSHOT SELECT COMMANDS UNDO)
  unset(ENV{PRIMESTRUCT_UI_${var}})
endforeach()
function(toolbar_report name which out)
  set(path "${WORK_DIR}/${name}_${which}.txt")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "toolbar demo ${name} wrote no ${which} report")
  endif()
  file(READ "${path}" content)
  set(${out} "${content}" PARENT_SCOPE)
endfunction()
set(original "A short word here.\n\n|a|bb|\n|-|:-:|\n|ccc|d|\n")
toolbar_report(toolbarbold text boldText)
string(FIND "${boldText}" "A short **word** here." found)
if(found EQUAL -1)
  message(FATAL_ERROR "the Bold command did not wrap the selected word: ${boldText}")
endif()
toolbar_report(toolbarundo text undoText)
string(FIND "${undoText}" "A short word here." found)
string(FIND "${undoText}" "**" stars)
if(found EQUAL -1 OR NOT stars EQUAL -1)
  message(FATAL_ERROR "one undo did not restore the text after Bold: ${undoText}")
endif()
toolbar_report(toolbarpreview mode previewMode)
if(NOT previewMode STREQUAL "2")
  message(FATAL_ERROR "the Preview segment did not switch to the preview-only mode: ${previewMode}")
endif()
toolbar_report(toolbarsplit mode splitMode)
if(NOT splitMode STREQUAL "1")
  message(FATAL_ERROR "the Split segment did not switch to the split mode: ${splitMode}")
endif()
toolbar_report(toolbarformat text formatText)
string(FIND "${formatText}" "| a   | bb  |" found)
if(found EQUAL -1)
  message(FATAL_ERROR "the Format command did not align the table: ${formatText}")
endif()
toolbar_report(toolbaradd text addText)
string(FIND "${addText}" "| ccc |" found)
if(found EQUAL -1)
  message(FATAL_ERROR "the Add Row command did not format the table: ${addText}")
endif()
toolbar_report(toolbarinsert text insertText)
string(FIND "${insertText}" "Column 3" found)
if(found EQUAL -1)
  message(FATAL_ERROR "the Insert table command did not insert a table: ${insertText}")
endif()
foreach(pair "toolbar;toolbarbold" "toolbar;toolbarpreview" "toolbarpreview;toolbarsplit")
  list(GET pair 0 first)
  list(GET pair 1 second)
  file(SHA256 "${WORK_DIR}/${first}.png" firstHash)
  file(SHA256 "${WORK_DIR}/${second}.png" secondHash)
  if(firstHash STREQUAL secondHash)
    message(FATAL_ERROR "the pictures ${first} and ${second} of the toolbar demo are the same")
  endif()
endforeach()
message(STATUS "a toolbar's commands edit the text through the selection, undo as one edit and switch the preview mode")

# The editor and file types bundles of this test declare document types. Launch Services must not keep them: a bundle
# in a build directory could otherwise become the handler of the user's .prime and configuration files.
set(lsregister "/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister")
if(EXISTS "${lsregister}")
  foreach(bundle EditorSmoke FileTypesDemo)
    execute_process(COMMAND "${lsregister}" -u "${WORK_DIR}/${bundle}.app" OUTPUT_QUIET ERROR_QUIET)
  endforeach()
endif()
# Launch Services registers an app again whenever it finds it on disk, so the bundles that declare types are deleted
# (the pictures and reports stay for looking at).
file(REMOVE_RECURSE "${WORK_DIR}/EditorSmoke.app" "${WORK_DIR}/FileTypesDemo.app")
