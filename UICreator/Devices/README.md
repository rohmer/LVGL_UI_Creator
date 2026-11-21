# Generate, build, and test a project

After **Export > Create firmware project** finishes, click **Build this project**. The build window receives the generated folder and selects Arduino CLI, ESP-IDF or CMake from its project files and manifest. It does not start a compiler or upload firmware until the corresponding action is selected.

The Configure tab also accepts an existing project folder. **Use project settings** detects its toolchain and, for generated Linux projects, the viewer executable and Protobuf argument. **Tool version** checks the selected environment without building or accessing hardware.

## Project targets

Enter a descriptive **Target name** and use **Save target**. **Save as new** creates another named configuration; selecting a target restores its project, build directory, board FQBN, port, baud, compiler/tool paths and run command. **Remove target** removes only the saved configuration. These authoring changes participate in project Undo/Redo. Unsaved session preferences remain available separately.

Targets are stored under the project's `deviceTargets` key. Tool paths and serial ports are host-specific; review them when opening a project on another computer. Device export removes this authoring metadata rather than embedding host paths in firmware.

## Build output and running

Output reports the actual configuring, compiling or linking phase. Ninja's `[current/total]` and Make/CMake's `[percent%]` values produce a progress bar. When a tool does not report a percentage, a spinner and phase text explicitly indicate that progress is unknown. Completion and failure are determined by the tool's exit status.

**Run built app** starts the configured executable as another cancellable job. Generated Raspberry Pi/Linux builds on Windows can use **Build / run this Linux CMake project through WSL**. These commands execute `cmake` and the viewer inside WSL using individual arguments, without shell interpolation. A working WSL Linux toolchain and its graphics support must already be installed. Linux builds use their native toolchain. CMake projects do not offer a universal Flash action.

Build and Flash first check a generated `ui/project.json`, or the current design if no generated design is available. Errors such as missing widget bindings or action targets block the operation and appear in the status text; advisory layout, contrast and touch warnings do not block a build. Validation does not install toolchains or configure a physical panel.

**Stop** terminates the running tool and its descendants. Closing the build window stops jobs and releases its serial monitor. Flash disconnects the monitor before opening the upload port. The Flash and serial-monitor controls remain explicit user actions.

Core checks live in `Tests/Integration/Devices/Workflow.cpp`; the editor's WorkflowProblems test exercises handoff, target persistence and validation without opening hardware.
