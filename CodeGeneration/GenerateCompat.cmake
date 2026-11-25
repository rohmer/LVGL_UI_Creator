file(READ "${SOURCE_DIR}/Targets/Compat9.h" COMPAT9_SOURCE)
file(READ "${SOURCE_DIR}/Targets/Compat8.h" COMPAT8_SOURCE)
configure_file("${SOURCE_DIR}/Targets/CompatSource.h.in" "${OUTPUT_DIR}/Targets/CompatSource.h" @ONLY)
find_program(keyboard_python NAMES python3 python REQUIRED)
if(NOT ADVKBD_SOURCE_ROOT)
    include("${SOURCE_DIR}/../cmake/AdvancedKeyboardDependency.cmake")
endif()
execute_process(COMMAND "${keyboard_python}" "${SOURCE_DIR}/EmbedAdvancedKeyboard.py"
    "${ADVKBD_SOURCE_ROOT}" "${OUTPUT_DIR}/AdvancedKeyboardSource.h"
    RESULT_VARIABLE keyboard_result)
if(NOT keyboard_result EQUAL 0)
    message(FATAL_ERROR "Cannot embed Advanced Keyboard export sources")
endif()
execute_process(COMMAND "${keyboard_python}" "${SOURCE_DIR}/EmbedMicroPython.py"
    "${SOURCE_DIR}/MicroPython" "${OUTPUT_DIR}/MicroPythonSource.h"
    RESULT_VARIABLE micropython_result)
if(NOT micropython_result EQUAL 0)
    message(FATAL_ERROR "Cannot embed MicroPython export runtime")
endif()
