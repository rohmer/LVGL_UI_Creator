# LVGL C generation targets

`LVGLCompiler --target` selects 9.6, 9.5, 9.3, 9.2, 9.1 or 8.3. The default is 9.6. All six generate compilable C and public headers usable from C++. Generated manifests and compile guards identify the target version. Protobuf uses the same target selection as a runtime stamp; matching runtime backends are available for all six targets (see ../../Runtime/README.md).

The functional matrix uses vendored 9.6 and official tags v9.5.0, v9.3.0, v9.2.2, v9.1.0 and v8.3.11. Capability tables reflect those headers. The older helpers translate flag/state convenience setters, coordinate sentinels, font descriptors and changed widget APIs. The 8.3 backend provides composite equivalents for newer image, messagebox, tabview and scale behavior. Firmware should interact through the generated API rather than assume every composite root has a particular native class.

Older image output uses ARGB8888 from embedded previews/originals, and raw canvas pixels are normalized. LVGL 8.3 requires 32-bit color. Fonts with row padding are repacked where that descriptor feature is unavailable. Export rejects properties that cannot be represented, including forced slider orientation on 9.2/9.1/8.3, forced bar orientation on 9.1/8.3, nonzero arc knob offsets and nonuniform image scaling on 8.3, and unsupported complex gradients. Unsupported default flags/styles are omitted; nondefault values fail with an explanation.

CMake embeds Compat9.h and Compat8.h automatically. After editing either, run `python CodeGeneration/Targets/GenerateCompat.py` to refresh the checked-in CompatSource.h used by the native Visual Studio project. Validate changes using the cross-version suite described in [Tests/README.md](../../Tests/README.md).
