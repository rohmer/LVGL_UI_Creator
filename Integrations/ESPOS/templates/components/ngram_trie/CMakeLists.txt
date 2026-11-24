# A headless engine component: no display or LVGL dependency. The UI component
# consumes these same symbols and PSRAM cache rather than compiling a second copy.
set(keyboard_core "${CMAKE_CURRENT_LIST_DIR}/../../Keyboard/AdvancedKeyboard")
idf_component_register(SRCS "${keyboard_core}/PredictionModel.cpp"
                            "${keyboard_core}/PredictionPrefix.cpp"
                            "${keyboard_core}/Utf8Fold.cpp"
                            "${keyboard_core}/PredictionStorage.cpp" "esp_runtime.cpp"
                       INCLUDE_DIRS "include" "${keyboard_core}" "${keyboard_core}/include"
                       REQUIRES esp_partition
                       PRIV_REQUIRES esp_timer freertos heap log task_scheduler)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ESPOS_KEYBOARD_HEADLESS_PREDICTION=1)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)

if(ESPOS_NGRAM_BENCH_CASES)
    get_filename_component(benchmark_cases "${ESPOS_NGRAM_BENCH_CASES}" ABSOLUTE BASE_DIR "${PROJECT_DIR}")
    configure_file("${benchmark_cases}" "${CMAKE_CURRENT_BINARY_DIR}/ngram_bench_cases.h" COPYONLY)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ESPOS_NGRAM_BENCHMARK=1)
    target_sources(${COMPONENT_LIB} PRIVATE "benchmark.cpp")
endif()
