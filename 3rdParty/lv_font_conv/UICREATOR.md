# UICreator integration

Upstream: https://github.com/lvgl/lv_font_conv

Pinned commit: `24b94aefc751539f5c476b6f22e357500d1ab2c7`, package version 1.5.3.
The upstream CLI/library and FreeType rendering module are unchanged. The MIT
license is in LICENSE. Production npm dependencies use the upstream lockfile;
install with `npm ci --omit=dev --ignore-scripts` (no package lifecycle scripts).

UICreator adds its bridge in `UICreator/Fonts/font-converter-bridge.js`, a native
file-picker/options window, and a worker that launches Node directly without a
command shell. The build copies this converter, its production dependencies,
the bridge and a Node runtime into `bin/font-converter`. Distribute that folder
alongside the executable. No npm/network access is needed at application runtime.

The upstream development/runtime pin is Node 24.11.1 and npm 11.6.2. CMake's
`UICREATOR_FONT_NODE` cache option selects the runtime to bundle; it prefers a
local `build/font-tools/node-v24.11.1-win-x64` distribution when available.
Its LICENSE is copied as NODE_LICENSE. Other production dependency licenses
remain in their respective node_modules directories. Keep all licenses when
redistributing the application.
