file(READ "${SOURCE_DIR}/Targets/Compat9.h" COMPAT9_SOURCE)
file(READ "${SOURCE_DIR}/Targets/Compat8.h" COMPAT8_SOURCE)
configure_file("${SOURCE_DIR}/Targets/CompatSource.h.in" "${OUTPUT_DIR}/Targets/CompatSource.h" @ONLY)
find_program(keyboard_python NAMES python3 python REQUIRED)
execute_process(COMMAND "${keyboard_python}" "${SOURCE_DIR}/EmbedAdvancedKeyboard.py"
    "${SOURCE_DIR}/../LVSerial/Widgets/AdvancedKeyboard" "${OUTPUT_DIR}/AdvancedKeyboardSource.h"
    RESULT_VARIABLE keyboard_result)
if(NOT keyboard_result EQUAL 0)
    message(FATAL_ERROR "Cannot embed Advanced Keyboard export sources")
endif()
