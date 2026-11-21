The file-picker backend has no LVGL dependency. On Linux it launches Zenity with
an argv array, captures UTF-8 stdout and diagnostics independently, and returns
Selected, Cancelled, Unavailable or Failed. The UI adapter reports actual
failures and leaves cancellation quiet. Existing Win32 dialogs remain in their
original branches.

Run the isolated POSIX check without opening any visible dialog:

```sh
python3 Tests/Integration/NativeFileDialog/run.py --work build/native-file-dialog-tests
```

It compiles the production backend with C++17 and warnings as errors. Its only
dialog executable is a temporary fake Zenity placed on a process-local PATH.
Checks cover Unicode and spaces, literal shell metacharacters, filters, multiple
paths, save/overwrite-confirmation and folder flags, normal cancellation,
backend/display errors, empty output and missing backend. The child receives
the host's library lookup instead of the application's private LD_LIBRARY_PATH;
the caller's environment is preserved.

The desktop integrations include Advanced Keyboard trie loading, image and font
sources/outputs, corpus/model loading and generation, model comparison, component
import/export, performance data and firmware selection, device/toolchain paths,
and project destination folders. Zenity is a Linux DEB dependency and a system
prerequisite for the portable TGZ.
