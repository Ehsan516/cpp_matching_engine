# cmake -DBIN=... -DIN=... -DEXP=... -DCODE=... -P run_fixture.cmake
# runs both --input and stdin mode, both have to match EXP + exit code

file(READ ${EXP} want)
string(REPLACE "\r" "" want "${want}")   # in case git on windows gave us crlf

execute_process(COMMAND ${BIN} --input ${IN}
    OUTPUT_VARIABLE got1 RESULT_VARIABLE rc1 ERROR_QUIET)
execute_process(COMMAND ${BIN}
    INPUT_FILE ${IN}
    OUTPUT_VARIABLE got2 RESULT_VARIABLE rc2 ERROR_QUIET)

if(NOT got1 STREQUAL want)
    message(FATAL_ERROR "--input output differs\n--- want\n${want}--- got\n${got1}")
endif()
if(NOT got2 STREQUAL got1)
    message(FATAL_ERROR "stdin and --input disagree")
endif()
if(NOT rc1 EQUAL ${CODE} OR NOT rc2 EQUAL ${CODE})
    message(FATAL_ERROR "exit code ${rc1}/${rc2}, wanted ${CODE}")
endif()
