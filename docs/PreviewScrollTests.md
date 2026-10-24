# Markdown preview scroll modes: test set

The preview of a markdown tab scrolls with its source in one of four modes (`ps_ui_preview_set_scroll_mode`, the
`scrollMode` preference, View > Scroll Preview and Settings > Markdown > "Scroll source and preview"):

| Mode | Meaning |
| --- | --- |
| 0 Independently | the views do not follow each other |
| 1 Linearly | both sit at the same fraction of what they can scroll |
| 2 By block (default) | both show the same source line; the page maps lines to its blocks (`data-line`) |
| 3 With the cursor | the page follows the text cursor (its line a third of the page below the top); scrolling a view alone moves nothing |

A page that is reloaded (every edit rebuilds it) starts at the top, so after each load the position is put back by
mode: scrolled together it goes to where the text view is, with the cursor to the cursor's line, independent to where
it was.

## Automated: `PrimeStruct_native_ui_appkit_scroll` (macOS, window server needed)

`cmake/NativeUiAppKitScrollTest.cmake` runs the bundled editor in snapshot mode on `tests/fixtures/ui/screenshot/preview_long.md`
(211 lines of even sections) or `preview_uneven.md` (a 40 line code block and a 15 row table), with the preview on and
`scrollMode` set through `PRIMESTRUCT_UI_PREFS`. The harness scrolls a view (`PRIMESTRUCT_UI_SCROLL=text:0.5` or
`preview:0.5`), puts the cursor on a line (`PRIMESTRUCT_UI_CARET=N`), types (`PRIMESTRUCT_UI_TYPE`, after the scroll with
`PRIMESTRUCT_UI_TYPE_AFTER_SCROLL`) and writes where both views ended up (`PRIMESTRUCT_UI_REPORT`: `text_line`,
`text_fraction`, `caret_line`, `text_lines`, `web_line`, `web_y`, `web_max`); the script asserts on those numbers.

| Scenario | Mode | What is asserted |
| --- | --- | --- |
| `off_text` | 0 | scrolling the source leaves the page at the top |
| `off_page` | 0 | scrolling the page leaves the source at the top |
| `off_page_type` | 0 | an edit does not send a scrolled page back to the top |
| `linear_text`, `linear_page` | 1 | the same fraction in both views, from either side |
| `linear_type` | 1 | after an edit's reload the page is back at the source's fraction |
| `blocks_text`, `blocks_page` | 2 | the same source line in both views, from either side |
| `blocks_uneven_text`, `blocks_uneven_page` | 2 | the same line in a page with a tall code block and a table |
| `blocks_text_end`, `blocks_page_end` | 2 | the end of one view is the end of the other |
| `blocks_type` | 2 | after an edit's reload the page is at the source's line, not at the top or the end |
| `cursor_caret` | 3 | the page shows the cursor's line; the source does not move |
| `cursor_text`, `cursor_page` | 3 | scrolling one view alone moves nothing |
| `cursor_type` | 3 | an edit does not send the page to the top; it stays at the cursor's line |
| `cursor_uneven` | 3 | the cursor's line in a page with tall blocks |

Also covered by other tests: the menu and the settings pop-up structure, the preference's default and saved value, the
mode function (headless and bindings), the markdown `data-line` tags, and the `scrollpagelong`, `scrolluneven` and
`scrollpageuneven` pictures of the smoke test.

## By hand (not automated)

Open a long markdown file, switch the preview on (Command-Shift-M), and for each mode check:

1. **Wheel, keys, drag.** Mouse wheel, trackpad, Page Up/Down, Home/End and dragging the scroll bar in the source and in
   the page: modes 1 and 2 move the other view along (no jump to the top or the bottom), 0 and 3 do not.
2. **Cursor.** Arrow keys, Page keys, a click and Select All in the source: mode 3 scrolls the page to the cursor's line,
   the others do not. Typing in the middle of a long note keeps the page where it is (modes 0, 1, 2) or at the
   cursor (3), never at the top.
3. **Switching mode while a preview is open**, from View > Scroll Preview and from Settings: the check mark moves in
   both places, the page goes to the new mode's place at once (not for mode 0), and the choice survives a restart.
4. **Preview off and on again**: the page opens at the source's place (modes 1, 2) or the cursor (3).
5. **Several tabs with previews**: each tab keeps its own positions; switching tabs does not move the views; the
   mode applies to every tab.
6. **Window**: resizing the window or dragging the divider keeps the views together; full screen, too.
7. **Documents**: an empty file; a short file that cannot scroll (the other view must not jump); a file with CRLF line
   endings; long lines that wrap; a document that ends without a newline; a huge file (several MB: no stall when
   scrolling); a note that is only a code block or only a table.
8. **Editing near the ends**: typing at the first and the last line, deleting a block, pasting a long text, undo and redo.
9. **Opening**: opening a file puts the cursor at its start (mode 3 shows the top); reopening a different file in the
   same tab resets both views.
10. **Links**: clicking a link in the page opens the browser and does not move the source; a `#fragment` link scrolls the
    page, and in modes 1 and 2 the source follows.

Not yet seen by eye: the settings window's tab titles (the snapshot draws the tab selector as an empty pill).
