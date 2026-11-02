# Shared switches of the AppKit CTest drivers (NativeUiAppKitSmokeTest, NativeUiAppKitScrollTest,
# NativeUiCompiledAppTest). These tests run the real AppKit backend; its windows are invisible in snapshot mode
# (src/ui/appkit/NativeUiAppKit.mm, presentWindow), but a few steps cannot be:
#
#   PRIMESTRUCT_SKIP_UI_WINDOWS=1       skip the whole driver (use it while you work at the machine)
#   PRIMESTRUCT_TEST_UI_INTRUSIVE=1     also run the steps that open a real menu and grab the mouse
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
