# Work protection and workspace preferences

Interactive launches enable session storage under `%LOCALAPPDATA%\LVGL_UI_Creator\Session` on Windows. Set `UICREATOR_SESSION_DIR` to use another directory. Linux uses `$XDG_STATE_HOME/LVGL_UI_Creator`, or `~/.local/state/LVGL_UI_Creator` when that variable is unset. Headless launches leave session persistence disabled; integration tests explicitly enable an isolated directory.

Project edits mark the project title with `*`. New, Open, recent-project selection and Exit ask whether to save, discard or cancel when there are unsaved changes. Native window close and operating-system quit requests use the same prompt. Undoing back to the saved document clears its dirty state. View changes do not count as project edits. **File > Save project…** and Ctrl+S open the save dialog. Saving a project opened from Protobuf defaults to a JSON filename so a device file is not accidentally replaced with JSON data.

Every 30 seconds, and when the editor loses focus, a changed document is written to `recovery.json`. This is a separate snapshot; autosave never overwrites a project file. **View > Workspace preferences** adjusts the interval from 10 to 300 seconds. A successful project save clears its recovery snapshot. Discarding changes clears it as well. A snapshot from an interrupted session prompts to Recover, Discard snapshot, or Exit. Exit leaves it available for the next launch. Recovery restores an unsaved document for review and explicit saving. A malformed recovery file is kept on disk for inspection; the editor's Workspace preferences show the reported session error.

Files are written beside the destination using a temporary file followed by replacement. Windows replacement requests write-through. A process lock prevents two editors from writing the same session. If session storage is occupied or unavailable, unsaved-change prompts remain active, and project editing/saving still works. Autosave and remembered preferences are unavailable in that instance; Workspace preferences shows the cause.

The eight most recent successfully opened/saved project paths are available under **File > Recent projects** and in **Help > Welcome and examples**. Recent-menu entries show the parent folder beneath each filename. Workspace storage records panel visibility, panel widths, bottom-panel height, canvas zoom/Fit, grid visibility, snapping, guides and grid spacing. These preferences do not enter JSON/Protobuf project content or generated firmware.

Alt+F/E/V/A/L/X/D/H opens File/Edit/View/Assets/Language Models/Export/Debug/Help.
Use Up/Down to select menu commands, Right to enter a submenu, Left to return to
its parent, and Enter to activate the selection. Escape returns from an open
submenu first; pressing it again closes the top-level menu.

Arrow keys nudge selected widget roots by one project pixel; Shift+Arrow nudges by ten. Text fields and dropdowns retain their keyboard behavior. Position locks and constrained drag axes are respected. **Edit > Align** groups edge/center alignment commands. **Edit > Distribute > Horizontally / Vertically** spaces three or more unlocked sibling widgets evenly between the outer widgets. Each nudge or distribution is one undoable edit.

**View > Fit canvas**, **Actual size (100%)**, and the zoom commands control the canvas.
Use **View > Show panel** to reopen a hidden panel, or **Restore all panels** to
show every panel. **Workspace preferences…** controls autosave and panel sizes.

**Export > Device Protobuf (.pb)…** and **Export > LVGL source project…** retain the project path field and add target LVGL selection (9.6, 9.5, 9.3, 9.2, 9.1, 8.3), a compiler preflight report, and a success window with an output-folder action. Protobuf exports record their runtime target; the firmware runtime must match it. **Export > Create firmware project…** opens the board project wizard. Fonts, images and their converters are grouped under **Assets**.
