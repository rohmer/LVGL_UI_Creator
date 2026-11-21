#include "lvgl_runtime.h"
#include <string.h>
int main(void)
{
    lvgl_runtime_t *runtime = NULL;
    char error[128];
    if (strcmp(lvgl_runtime_backend(), "9.6"))
        return 1;
    if (lvgl_runtime_load_memory(NULL, 0, NULL, &runtime, error, sizeof error))
        return 2;
    if (runtime || !error[0])
        return 3;
    lvgl_runtime_destroy(NULL);
    return 0;
}
