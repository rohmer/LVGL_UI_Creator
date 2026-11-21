execute_process(COMMAND "${DEVICE_TEST}" "${FIXTURE}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "C codec fixture creation failed")
endif()
execute_process(COMMAND "${PROTOC}" --decode=lvserial.Record
    "-I${SOURCE_ROOT}/LVSerial/Proto" "-I${SOURCE_ROOT}/3rdParty/nanopb/generator/proto"
    "${SOURCE_ROOT}/LVSerial/Proto/lvserial.proto" INPUT_FILE "${FIXTURE}"
    OUTPUT_VARIABLE decoded ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT decoded MATCHES "signed_value: -42" OR NOT decoded MATCHES "text_value: \"hello\"")
    message(FATAL_ERROR "Official Protobuf compiler rejected the nanopb fixture: ${error} ${decoded}")
endif()
message(STATUS "Official Protobuf compiler decoded the plain-C nanopb fixture")
