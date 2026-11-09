# Shared switches of the AppKit CTest drivers (NativeUiAppKitSmokeTest, NativeUiAppKitScrollTest,
# NativeUiCompiledAppTest). These tests run the real AppKit backend. By default they stay out of the way of whoever
# works at the machine: in snapshot mode the app can never become active (NSApplicationActivationPolicyProhibited)
# and its windows are transparent, ignore the mouse and are ordered in without becoming key
# (src/ui/appkit/NativeUiAppKit.mm, ps_ui_init and presentWindow), so the keyboard focus stays in the user's app.
# A snapshot run that becomes active anyway exits with status 3, which fails the driver. Only the open-menu steps
# cannot avoid activating, and they are opt-in:
#
#   PRIMESTRUCT_TEST_UI_INTRUSIVE=1     also run the steps that open a real menu (they activate the app and grab
#                                       the mouse for a moment)
#   PRIMESTRUCT_SKIP_UI_WINDOWS=1       skip the whole driver (an opt-out; not needed for focus any more)
#
# Every snapshot window is also transparent and parked beyond the bottom right corner of the screens, and the bundles
# the drivers build are agent apps (PS_UI_ELEMENT=1: LSUIElement, so a Launch Services launch shows no Dock icon).
#
# Include this file near the top of a driver, then `if(PRIMESTRUCT_UI_SKIP) return() endif()` (a return() inside an
# included file would only leave the include).

if(DEFINED ENV{PRIMESTRUCT_SKIP_UI_WINDOWS} AND NOT "$ENV{PRIMESTRUCT_SKIP_UI_WINDOWS}" STREQUAL "" AND
   NOT "$ENV{PRIMESTRUCT_SKIP_UI_WINDOWS}" STREQUAL "0")
  # CTest reads this line through SKIP_REGULAR_EXPRESSION and reports the test as skipped, not passed.
  message(STATUS "PRIMESTRUCT_UI_SKIPPED: PRIMESTRUCT_SKIP_UI_WINDOWS is set")
  set(PRIMESTRUCT_UI_SKIP ON)
endif()

# True when steps that pop a real menu (and so grab the mouse for a moment) may run.
set(PRIMESTRUCT_UI_INTRUSIVE OFF)
if(DEFINED ENV{PRIMESTRUCT_TEST_UI_INTRUSIVE} AND NOT "$ENV{PRIMESTRUCT_TEST_UI_INTRUSIVE}" STREQUAL "" AND
   NOT "$ENV{PRIMESTRUCT_TEST_UI_INTRUSIVE}" STREQUAL "0")
  set(PRIMESTRUCT_UI_INTRUSIVE ON)
endif()

# The bundles the drivers build never show a Dock icon (scripts/bundle_macos_app.sh).
set(ENV{PS_UI_ELEMENT} "1")
