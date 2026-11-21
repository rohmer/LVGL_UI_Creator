#include "lvgl_runtime.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    FILE *file;
    long size;
    void *bytes;
    lvgl_runtime_t *project = NULL;
    char error[256];
    if (argc != 2)
        return 1;
    file = fopen(argv[1], "rb");
    if (!file)
        return 2;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0)
        return 3;
    bytes = malloc((size_t)size);
    if (!bytes)
        return 4;
    if (fread(bytes, 1, (size_t)size, file) != (size_t)size)
        return 5;
    fclose(file);
    lv_init();
#if LVGL_VERSION_MAJOR == 8
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);driver.hor_res=320;driver.ver_res=240;
    lv_disp_t *display=lv_disp_drv_register(&driver);
#else
    lv_display_t *display = lv_display_create(320, 240);
#endif
    if (!lvgl_runtime_load_memory(bytes, (size_t)size, NULL, &project, error, sizeof error))
    {
        fprintf(stderr, "%s\n", error);
        return 6;
    }
    free(bytes);
    if (lvgl_runtime_screen_count(project) != 1 || !lvgl_runtime_font(project, "font_1") ||
        !lvgl_runtime_image(project, "image_1"))
        return 7;
    lvgl_runtime_destroy(project);
#if LVGL_VERSION_MAJOR == 8
    lv_disp_remove(display);
#else
    lv_display_delete(display);
#endif
#if LVGL_VERSION_MAJOR >= 9
    lv_deinit();
#endif
    puts("Standalone C firmware runtime passed");
    return 0;
}
