#!/bin/sh
# Real official binding used by UICreatorMicroPythonNative, independent of editor LVGL.
set -eu
task_root="$PWD/build/ci-micropython"
mkdir -p "$task_root"
git clone --filter=blob:none https://github.com/lvgl/lv_micropython.git "$task_root/lv_micropython"
git -C "$task_root/lv_micropython" checkout --detach 44aa2b153dd3aa1ba183d05bd867fe3ffe2a1d75
git clone --filter=blob:none https://github.com/lvgl/lv_binding_micropython.git "$task_root/lv_binding_micropython"
git -C "$task_root/lv_binding_micropython" checkout --detach 032dda4b7e8419702a4b9c4c8748a4d85d51be61
git -C "$task_root/lv_binding_micropython" submodule update --init --recursive
test "$(git -C "$task_root/lv_binding_micropython/lvgl" rev-parse HEAD)" = c033a98afddd65aaafeebea625382a94020fe4a7
make -C "$task_root/lv_micropython/mpy-cross" -j2
make -C "$task_root/lv_micropython/ports/unix" -j2 VARIANT=lvgl \
    USER_C_MODULES="$task_root" FROZEN_MANIFEST= \
    MICROPY_PY_FFI=0 MICROPY_PY_SSL=0 MICROPY_PY_BTREE=0
"$task_root/lv_micropython/ports/unix/build-lvgl/micropython" -c 'import lvgl; print("Real LVGL binding loaded")'
