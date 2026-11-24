# Writes the version text of the compiler in COMPILER to STAMP, and only when the text differs from what the
# file holds, so a file that depends on STAMP is rebuilt after a toolchain change and not otherwise.
execute_process(COMMAND "${COMPILER}" --version
                OUTPUT_VARIABLE compilerVersionText
                ERROR_VARIABLE compilerVersionError
                RESULT_VARIABLE compilerVersionResult)
if(NOT compilerVersionResult EQUAL 0)
  set(compilerVersionText "unknown: ${compilerVersionError}")
endif()
set(previousText "")
if(EXISTS "${STAMP}")
  file(READ "${STAMP}" previousText)
endif()
if(NOT previousText STREQUAL compilerVersionText)
  file(WRITE "${STAMP}" "${compilerVersionText}")
endif()
