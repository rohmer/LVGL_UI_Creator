# Raspberry Pi Pico and Pico 2 display SDK

Runtime projects can also load LVGL UI Creator XML: generate with
`--enable-xml`, or set `-DLVGL_PB_ENABLE_XML=ON` when configuring an existing
generated runtime project. The default Protobuf-only build omits the parser.
Both formats use the same loaders and selected widgets; generate the profile
from the matching Protobuf or JSON design. Direct C projects have no input
loader and do not enable XML. This XML format is independent of LVGL Pro XML.

Generate a standalone Pico SDK project for **Pico 1 (RP2040)** or **Pico 2
(RP2350 Arm)**. Both outputs include the pinned LVGL 9.6 and only the LVGL
translation units selected by the saved projects. The application supplies its
physical display and touch driver callbacks; no panel, pinout or RTOS is assumed.

```sh
python Runtime/pico/make_sdk.py project.pb --board pico --output build/pico-display --zip
python Runtime/pico/make_sdk.py project.pb --board pico2 --output build/pico2-display --zip
```

The generated Protobuf library supports embedded immutable data in XIP flash,
owned memory input and application-registered LVGL filesystems. It shares the
portable runtime's screens, variables, bindings, images, fonts, Advanced Keyboard,
selected trie, autocorrect, barcodes and QR codes. Unused modules and optional
prediction/model data are omitted. Project-specific feature/source counts and
model bytes are recorded in `embedded/features.json`.

To build direct editor-exported C instead of loading Protobuf:

```sh
UICreatorCodeGen project.pb build/exported-c
python Runtime/pico/make_sdk.py project.pb --board pico --generated-c build/exported-c --output build/pico-c --zip
```

C mode links `ui/` and the selected LVGL sources. It omits the Protobuf decoder,
JSON, serialization and runtime source trees entirely. Advanced Keyboard adds
its own required C++17 sources when the UI uses it. Regenerate after changing the
project; CMake rejects modified embedded inputs that no longer match the profile.

## Build on Windows or Linux

Install Pico SDK **2.3.1 or newer**, its required submodules/tools, Arm GNU bare-metal GCC,
CMake 3.24+, Ninja and Python 3.10+. The SDK/toolchain is a build prerequisite;
it is not copied into each exported UI. Use separate build directories per board.

```sh
cmake -S build/pico-display -B build/pico-display/firmware -G Ninja -DPICO_SDK_PATH=/absolute/path/pico-sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build/pico-display/firmware --parallel
```

On Windows, use an absolute SDK path such as `C:/pico/pico-sdk` and put
`arm-none-eabi-gcc` on PATH (or set the SDK's `PICO_TOOLCHAIN_PATH`). For Pico 2,
configure the generated `pico2-display` directory instead. CMake selects
`PICO_BOARD=pico`/`PICO_PLATFORM=rp2040` or
`PICO_BOARD=pico2`/`PICO_PLATFORM=rp2350-arm-s`. RISC-V Pico 2 is not selected by
these Arm templates. The Pico SDK's picotool must be installed or available for
its normal extra-output build step.

The build produces `lvgl_pico_display.elf`, `.bin` and `.uf2`. Hold **BOOTSEL**
while connecting the matching board, then copy its UF2 to the USB drive. A Pico 1
UF2 and a Pico 2 UF2 are different targets; use the output built for that board.

## Display, touch and memory

`example/main.c` creates a display with one eight-line RGB565 partial draw buffer
by default. Its initial flush callback discards drawing and reports completion;
replace it with your display driver's transfer function. For asynchronous DMA,
call `lvgl_pico_flush_ready` once from the completion path. Optionally supply the
touch callback through `lvgl_pico_display_config_t`. There is no second hidden
framebuffer. `LVGL_PICO_BUFFER_LINES` is configurable from 1 to 64. The optional
`LVGL_PICO_USB_STDIO` build option enables USB CDC logging; UART logging is enabled
by default. Keep all LVGL calls on the same owner core/task.

The generated example sets `PICO_MALLOC_PANIC=0` for its firmware target so
allocation failures can reach the library's error handling. When integrating
the library into another Pico application, use that allocator setting on your
own firmware target if you need recoverable allocation failures.

The generated defaults reserve 16 KiB for the owner stack and enable C++17,
exceptions and RTTI. The SDK's ordinary 4 KiB scratch stack is too small for
the Protobuf loader; the project explicitly reserves a contiguous stack region
at the top of SRAM and caps the heap below it. `PICO_STACK_SIZE` can be configured
from 4 to 64 KiB in multiples of eight bytes. A separate 4 KiB scratch/core-1
region remains reserved; the example uses only core 0. Pico 1 has 264 KiB
SRAM/2 MiB flash; Pico 2 has 520 KiB
SRAM/4 MiB flash. Linker size checks cover static storage and flash, but dynamic
widgets, decoded screen metadata, variable history and keyboard workspace must
also fit. Use bounded histories, compact images/fonts, and borrowed embedded
models. Ordinary Pico/Pico 2 boards have no PSRAM, so trie data stays compact in
flash. Large authored designs can still exceed a board's available memory.

A successful compile/UF2 build verifies the software target, not a physical
display, touch controller, wiring or measured device memory consumption.
