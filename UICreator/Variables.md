# Variables and widget connections

Open the **Variables** tab in the **Tools** panel to connect application data to LVGL widgets. A variable
has one value and can connect to several widgets on the same screen or on different
screens. Connections are saved with JSON and Protobuf projects and included in C
source exports.

1. Add a variable with **+** and give it a unique **Name**.
2. Choose **Type**, **Default**, and **History**. DateTime values are Unix seconds.
   History `1` keeps the latest sample; `0` keeps all samples and can grow in memory.
3. Choose the exact **C type** of the application variable. For example, use
   `int32_t` for a 32-bit integer or `float` for a single-precision number.
4. Use **Add widget** to choose a screen, named widget, property, and direction.
   **Use selected** starts with the widget selected on the canvas or in the hierarchy.
5. Add more connections to display the same value in several places. **Reveal**
   selects the connected widget; **Edit** changes a connection; **Remove** removes
   that connection while keeping the variable and widget.
6. Test widget interaction with **Run** or **Debug screen**, then export through
   **Export > LVGL source project**.

The property menu shows properties supported by the widget and variable type.
Numeric values can also appear as text. A label's text, geometry, opacity, and
LED brightness are display properties. Editable text, checked state, selections,
and interactive numeric values can also send changes back to the variable.

| Direction | Behavior |
| --- | --- |
| Display variable | Variable changes update this widget. |
| Input to variable | User edits update the variable and its other output connections. |
| Read and write | Variable changes update this widget; user edits update the variable and its other output connections. |

Names must be unique. Renaming a variable updates its event actions and conditions
across screens. Deleting a variable also removes actions that depend on it; Undo
restores them. Deleting a widget removes its live connections while keeping the
variable. Screen reordering preserves connections; duplicating a screen adds its
explicit connections to the same shared variables. Older single-widget projects
remain supported.

## Connect a C or C++ variable

**C/C++ symbol** is an optional application-variable name for the wiring example;
it does not declare an external symbol or insert application code into `ui.c`.
**Copy wiring code** produces the exact generated function name, including any
legacy-name collision suffixes. When a symbol is entered, the example binds that
existing application variable. With the symbol left blank, it includes a sample
storage declaration. Include the exported `ui.h`, initialize LVGL and
the generated UI, and bind your existing variable by address:

```c
#include "ui.h"

static int32_t device_temperature = 22;

void start_ui(void)
{
    if (!ui_init()) return;
    if (!ui_bind_v_temperature(&device_temperature)) {
        /* Handle an invalid initial value or allocation failure. */
    }
}

void update_temperature_on_lvgl_thread(int32_t value)
{
    device_temperature = value;
    if (!ui_sync_variables()) {
        /* Handle a value that cannot be displayed or stored. */
    }
}
```

The name `ui_bind_v_temperature` assumes the editor variable is named
`temperature`. Use the example shown in the tab or exported `README.md` for the
exact name. The exported public header works with C and C++; C++ callers may pass
the address of a namespace variable or a member with the selected exact type.

Binding first imports the application's existing value. User input and generated
setters write back to the bound storage and update all output connections.
`ui_sync_variables()` imports later application changes; unchanged values do not
redraw widgets or add history samples. Per-variable `ui_sync_v_NAME()` and
`ui_unbind_v_NAME()` functions are also exported. Setters such as
`ui_set_v_temperature(value)` remain available without external storage.

Text uses a writable, NUL-terminated buffer with an explicit capacity:

```c
static char device_name[64] = "Controller";
/* After ui_init(), for an editor String variable named device_name: */
if (!ui_bind_v_device_name(device_name, sizeof device_name)) {
    /* Handle invalid text or allocation failure. */
}
```

Values that exceed the selected C type or text-buffer capacity are rejected
instead of truncated. Keep bound storage alive until it is unbound or
`ui_deinit()` is called, and bind it again after reinitializing the UI. All
generated UI calls run on the LVGL thread. Transfer sensor/task values to that
thread using the application's normal synchronization mechanism.
