/** Native LVGL component recipes inspired by Bootstrap 5.3 (no HTML/CSS runtime).
 * The returned trees contain ordinary LVGL objects and remain editable.
 * Application code owns data, navigation and device actions.
 */
#ifndef LV_BOOTSTRAP_H
#define LV_BOOTSTRAP_H
#include "../lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LV_BOOTSTRAP_ACCORDION, LV_BOOTSTRAP_ALERT, LV_BOOTSTRAP_BADGE,
    LV_BOOTSTRAP_BREADCRUMB, LV_BOOTSTRAP_BUTTON, LV_BOOTSTRAP_BUTTON_GROUP,
    LV_BOOTSTRAP_CARD, LV_BOOTSTRAP_CAROUSEL, LV_BOOTSTRAP_CLOSE_BUTTON,
    LV_BOOTSTRAP_COLLAPSE, LV_BOOTSTRAP_DROPDOWN, LV_BOOTSTRAP_LIST_GROUP,
    LV_BOOTSTRAP_MODAL, LV_BOOTSTRAP_NAVBAR, LV_BOOTSTRAP_TABS,
    LV_BOOTSTRAP_OFFCANVAS, LV_BOOTSTRAP_PAGINATION, LV_BOOTSTRAP_PLACEHOLDER,
    LV_BOOTSTRAP_POPOVER, LV_BOOTSTRAP_PROGRESS, LV_BOOTSTRAP_SCROLLSPY,
    LV_BOOTSTRAP_SPINNER, LV_BOOTSTRAP_TOAST, LV_BOOTSTRAP_TOOLTIP,
    LV_BOOTSTRAP_FORM, LV_BOOTSTRAP_INPUT_GROUP, LV_BOOTSTRAP_FLOATING_LABEL,
    LV_BOOTSTRAP_CHECKS_RADIOS, LV_BOOTSTRAP_RANGE, LV_BOOTSTRAP_VALIDATION,
    LV_BOOTSTRAP_TABLE, LV_BOOTSTRAP_SIDEBAR, LV_BOOTSTRAP_STAT_CARD,
    LV_BOOTSTRAP_LINE_CHART, LV_BOOTSTRAP_BAR_CHART, LV_BOOTSTRAP_ACTIVITY,
    LV_BOOTSTRAP_COMPONENT_COUNT,
    LV_BOOTSTRAP_DASHBOARD = LV_BOOTSTRAP_COMPONENT_COUNT,
    LV_BOOTSTRAP_ANALYTICS, LV_BOOTSTRAP_COMMERCE, LV_BOOTSTRAP_USERS,
    LV_BOOTSTRAP_SETTINGS, LV_BOOTSTRAP_SIGN_IN, LV_BOOTSTRAP_CHECKOUT,
    LV_BOOTSTRAP_PRICING, LV_BOOTSTRAP_ALBUM, LV_BOOTSTRAP_BLOG,
    LV_BOOTSTRAP_PRODUCT, LV_BOOTSTRAP_COVER, LV_BOOTSTRAP_COMPONENT_GALLERY,
    LV_BOOTSTRAP_PRESET_COUNT
} lv_bootstrap_preset_t;

/** Called for each recipe-owned object, excluding native widget internals.
 * Names are static strings. The callback must not delete/reparent the object.
 */
typedef void (*lv_bootstrap_visit_cb_t)(lv_obj_t * object, const char * name, void * context);
const char * lv_bootstrap_preset_name(lv_bootstrap_preset_t preset);
/** Returns "collapse" or "dismiss" for recipe behavior, otherwise NULL. */
const char * lv_bootstrap_get_action(lv_obj_t * object);
lv_obj_t * lv_bootstrap_create(lv_obj_t * parent, lv_bootstrap_preset_t preset,
                              bool dark, lv_bootstrap_visit_cb_t visit, void * context);
#ifdef __cplusplus
}
#endif
#endif
