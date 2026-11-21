# Native generated Bootstrap consumers

After configuring the main project with `BUILD_TESTING=ON`, build and run:

```powershell
cmake --build build/release --target UICreatorBootstrapGenerated
ctest --test-dir build/release -R '^UICreatorBootstrapGenerated$' --output-on-failure
```

For a multi-configuration generator, add `--config Release` to the build and
`-C Release` to CTest.

The fixture target invokes `UICreatorBootstrapTests` to export every component
and template. `Prepare.cmake` reads each independent JSON fixture and generated
manifest to create expected object IDs, native classes, parent relationships,
and action checks. Each preset's `ui.c` and `ui_assets.c` compile as C11 in a
separate executable linked to the existing native LVGL library. LVGL is reused
rather than compiled separately for every fixture.

The common consumer verifies every exported object and its parent ancestry,
executes dismiss and collapse actions, renders the screen to a flush callback,
and repeats initialization and teardown three times. It also checks idempotent
initialization/teardown and rejects invalid object/screen lookups.

This validates generated native firmware behavior on the host. It does not
establish pixel equivalence to a browser, application-specific business actions,
hardware drivers, or device memory limits.
