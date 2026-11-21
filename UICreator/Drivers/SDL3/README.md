# Native SDL3 editor adapter

These drivers adapt the MIT-licensed LVGL SDL driver to the native SDL3 API. See LICENCE.txt for the original LVGL license. They are application-owned sources; the vendored SDL3 sources are unchanged.

The editor renders LVGL software buffers through SDL3 streaming textures. Input includes UTF-8 text, keyboard shortcuts, mouse, touch and hover-target wheel scrolling. Window close is deferred until the editor has detached its history observer and can safely destroy LVGL objects.

For headless tests use SDL_VIDEO_DRIVER=dummy and SDL_RENDER_DRIVER=software. The editor links SDL3 directly and does not require an SDL2 compatibility library.
