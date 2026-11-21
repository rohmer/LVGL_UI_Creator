/* Native Bootstrap-inspired recipes. Project-local extension, not upstream Bootstrap. */
#include "../../../include/lvgl/widgets/lv_bootstrap.h"
#include <stdio.h>

typedef struct {
    bool dark;
    lv_bootstrap_visit_cb_t visit;
    void * context;
} recipe_t;

static const char * names[] = {
    "Accordion", "Alert", "Badge", "Breadcrumb", "Primary button", "Button group",
    "Card", "Carousel", "Close button", "Collapse", "Dropdown", "List group",
    "Modal", "Navbar", "Navs and tabs", "Offcanvas", "Pagination", "Placeholder",
    "Popover", "Progress", "Scrollspy", "Spinner", "Toast", "Tooltip",
    "Form controls", "Input group", "Floating label", "Checks and radios", "Range",
    "Validation", "Data table", "Sidebar", "Statistic card", "Line chart", "Bar chart",
    "Activity feed", "Admin dashboard", "Analytics dashboard", "Commerce dashboard",
    "User management", "Settings page", "Sign in", "Checkout", "Pricing", "Album",
    "Blog", "Product", "Cover", "Component gallery"
};

const char * lv_bootstrap_preset_name(lv_bootstrap_preset_t preset)
{
    return (unsigned)preset < LV_BOOTSTRAP_PRESET_COUNT ? names[preset] : "Unknown preset";
}

#if LV_USE_LABEL && LV_USE_BUTTON && LV_USE_CHECKBOX && LV_USE_SWITCH && LV_USE_DROPDOWN && \
    LV_USE_TEXTAREA && LV_USE_TABLE && LV_USE_CHART && LV_USE_TABVIEW && LV_USE_SPINNER && \
    LV_USE_SLIDER && LV_USE_BAR && LV_USE_FLEX

static lv_color_t ink(recipe_t * r) { return lv_color_hex(r->dark ? 0xdee2e6 : 0x212529); }
static lv_color_t surface(recipe_t * r) { return lv_color_hex(r->dark ? 0x212529 : 0xffffff); }
static lv_color_t muted(recipe_t * r) { return lv_color_hex(r->dark ? 0xadb5bd : 0x6c757d); }
static lv_obj_t * own(recipe_t * r, lv_obj_t * object, const char * name)
{
    if(r->visit) r->visit(object, name, r->context);
    return object;
}

static void panel_style(recipe_t * r, lv_obj_t * object)
{
    lv_obj_set_style_bg_color(object, surface(r), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(object, ink(r), 0);
    lv_obj_set_style_border_color(object, lv_color_hex(r->dark ? 0x495057 : 0xdee2e6), 0);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_radius(object, 6, 0);
    lv_obj_set_style_pad_all(object, 12, 0);
    lv_obj_set_style_pad_row(object, 10, 0);
    lv_obj_set_style_pad_column(object, 10, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
}

static lv_obj_t * box(recipe_t * r, lv_obj_t * parent, const char * name, int width, int height, bool row)
{
    lv_obj_t * object = own(r, lv_obj_create(parent), name);
    panel_style(r, object);
    lv_obj_set_size(object, width, height);
    lv_obj_set_flex_flow(object, row ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
    return object;
}

static lv_obj_t * text(recipe_t * r, lv_obj_t * parent, const char * caption)
{
    lv_obj_t * label = own(r, lv_label_create(parent), "Text");
    lv_label_set_text(label, caption);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_color(label, ink(r), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    return label;
}

static lv_obj_t * button(recipe_t * r, lv_obj_t * parent, const char * caption, bool primary)
{
    lv_obj_t * object = own(r, lv_button_create(parent), caption);
    panel_style(r, object);
    lv_obj_set_size(object, LV_SIZE_CONTENT, 38);
    lv_obj_set_style_bg_color(object, lv_color_hex(primary ? 0x0d6efd : (r->dark ? 0x343a40 : 0xf8f9fa)), 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(primary ? 0x0b5ed7 : 0x6c757d), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(object, lv_color_hex(primary ? 0x0d6efd : 0xadb5bd), 0);
    lv_obj_set_style_pad_ver(object, 7, 0);
    lv_obj_t * label = text(r, object, caption);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(label, primary ? lv_color_white() : ink(r), 0);
    lv_obj_center(label);
    return object;
}

static lv_obj_t * input(recipe_t * r, lv_obj_t * parent, const char * placeholder, bool password)
{
    lv_obj_t * object = own(r, lv_textarea_create(parent), placeholder);
    panel_style(r, object);
    lv_obj_set_size(object, LV_PCT(100), 42);
    lv_textarea_set_one_line(object, true);
    lv_textarea_set_placeholder_text(object, placeholder);
    lv_textarea_set_password_mode(object, password);
    return object;
}

static void close_parent(lv_event_t * event)
{
    /* Hiding preserves document identity and allows editor undo. */
    lv_obj_set_hidden(lv_obj_get_parent(lv_event_get_current_target_obj(event)), true);
}
static lv_obj_t * dismiss(recipe_t * r, lv_obj_t * parent)
{
    lv_obj_t * object = button(r, parent, LV_SYMBOL_CLOSE, false);
    lv_obj_add_event_cb(object, close_parent, LV_EVENT_CLICKED, NULL);
    return object;
}
static void collapse(lv_event_t * event)
{
    lv_obj_t * parent = lv_obj_get_parent(lv_event_get_current_target_obj(event));
    lv_obj_t * body = lv_obj_get_child(parent, 1);
    if(body) lv_obj_set_hidden(body, !lv_obj_is_hidden(body));
}

const char * lv_bootstrap_get_action(lv_obj_t * object)
{
    for(uint32_t i = 0; i < lv_obj_get_event_count(object); ++i) {
        lv_event_cb_t callback = lv_event_dsc_get_cb(lv_obj_get_event_dsc(object, i));
        if(callback == collapse) return "collapse";
        if(callback == close_parent) return "dismiss";
    }
    return NULL;
}

static lv_obj_t * native_table(recipe_t * r, lv_obj_t * parent)
{
    lv_obj_t * table = own(r, lv_table_create(parent), "Records");
    panel_style(r, table);
    lv_obj_set_size(table, LV_PCT(100), 190);
    lv_table_set_column_count(table, 3);
    const char * cells[5][3] = {{"Name", "Status", "Value"}, {"Alex Morgan", "Active", "$1,240"},
        {"Sam Rivera", "Pending", "$860"}, {"Jordan Lee", "Active", "$2,180"}, {"Taylor Chen", "Review", "$430"}};
    lv_obj_update_layout(parent);
    int width = lv_obj_get_content_width(parent);
    for(unsigned col = 0; col < 3; ++col) lv_table_set_column_width(table, col, (width > 0 ? width : 360) / 3);
    for(unsigned row = 0; row < 5; ++row)
        for(unsigned col = 0; col < 3; ++col) lv_table_set_cell_value(table, row, col, cells[row][col]);
    lv_obj_set_style_bg_color(table, lv_color_hex(r->dark ? 0x343a40 : 0xf8f9fa), LV_PART_ITEMS | LV_STATE_PRESSED);
    return table;
}

static lv_obj_t * chart(recipe_t * r, lv_obj_t * parent, bool bars)
{
    lv_obj_t * object = own(r, lv_chart_create(parent), bars ? "Revenue bars" : "Revenue trend");
    panel_style(r, object);
    lv_obj_set_size(object, LV_PCT(100), 160);
    lv_chart_set_type(object, bars ? LV_CHART_TYPE_BAR : LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(object, 8);
    lv_chart_series_t * series = lv_chart_add_series(object, lv_color_hex(0x0d6efd), LV_CHART_AXIS_PRIMARY_Y);
    const int values[] = {18, 30, 24, 48, 41, 65, 58, 86};
    for(unsigned i = 0; i < 8; ++i) lv_chart_set_value_by_id(object, series, i, values[i]);
    return object;
}

static lv_obj_t * component(recipe_t * r, lv_obj_t * parent, lv_bootstrap_preset_t preset)
{
    lv_obj_t * root = NULL;
    const char * name = lv_bootstrap_preset_name(preset);
    if(preset == LV_BOOTSTRAP_BUTTON || preset == LV_BOOTSTRAP_CLOSE_BUTTON)
        return button(r, parent, preset == LV_BOOTSTRAP_BUTTON ? "Primary action" : LV_SYMBOL_CLOSE, true);
    if(preset == LV_BOOTSTRAP_DROPDOWN) {
        root = own(r, lv_dropdown_create(parent), name);
        panel_style(r, root); lv_obj_set_width(root, 220);
        lv_dropdown_set_options(root, "Choose an action\nView details\nEdit record\nArchive record");
        return root;
    }
    if(preset == LV_BOOTSTRAP_SPINNER) {
        root = own(r, lv_spinner_create(parent), name); lv_obj_set_size(root, 48, 48);
        lv_obj_set_style_arc_color(root, lv_color_hex(0x0d6efd), LV_PART_INDICATOR); return root;
    }
    if(preset == LV_BOOTSTRAP_RANGE) {
        root = own(r, lv_slider_create(parent), name); lv_obj_set_size(root, 240, 16);
        lv_slider_set_value(root, 45, LV_ANIM_OFF); return root;
    }
    if(preset == LV_BOOTSTRAP_TABLE) return native_table(r, parent);
    if(preset == LV_BOOTSTRAP_LINE_CHART || preset == LV_BOOTSTRAP_BAR_CHART)
        return chart(r, parent, preset == LV_BOOTSTRAP_BAR_CHART);
    if(preset == LV_BOOTSTRAP_TABS || preset == LV_BOOTSTRAP_CAROUSEL || preset == LV_BOOTSTRAP_SCROLLSPY) {
        root = own(r, lv_tabview_create(parent), name); panel_style(r, root);
        lv_obj_set_size(root, 280, 190); lv_tabview_set_tab_bar_size(root, 38);
        if(preset == LV_BOOTSTRAP_SCROLLSPY) lv_tabview_set_tab_bar_position(root, LV_DIR_LEFT);
        const char * tabs[] = {"Overview", "Details", "Activity"};
        for(unsigned i = 0; i < 3; ++i) {
            lv_obj_t * page = lv_tabview_add_tab(root, tabs[i]);
            panel_style(r, page);
            text(r, page, preset == LV_BOOTSTRAP_CAROUSEL ? "Swipe to explore the next slide" : tabs[i]);
        }
        return root;
    }
    root = box(r, parent, name, 280, LV_SIZE_CONTENT, false);
    switch(preset) {
        case LV_BOOTSTRAP_BADGE:
            lv_obj_set_width(root, LV_SIZE_CONTENT); lv_obj_set_style_bg_color(root, lv_color_hex(0x0d6efd), 0);
            lv_obj_set_style_pad_all(root, 6, 0); text(r, root, "New  4");
            lv_obj_set_width(lv_obj_get_child(root, 0), LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(lv_obj_get_child(root, 0), lv_color_white(), 0); break;
        case LV_BOOTSTRAP_BREADCRUMB:
            text(r, root, "Home  /  Workspace  /  Overview"); break;
        case LV_BOOTSTRAP_ALERT:
            lv_obj_set_style_bg_color(root, lv_color_hex(r->dark ? 0x052c65 : 0xcfe2ff), 0);
            text(r, root, "Your changes were saved successfully."); dismiss(r, root); break;
        case LV_BOOTSTRAP_CARD:
            text(r, root, "Project overview"); text(r, root, "A flexible native card with editable content and actions.");
            button(r, root, "View project", true); break;
        case LV_BOOTSTRAP_STAT_CARD:
            text(r, root, "TOTAL REVENUE"); text(r, root, "$24,580");
            lv_obj_set_style_text_color(text(r, root, "+12.8% this month"), lv_color_hex(0x198754), 0); break;
        case LV_BOOTSTRAP_BUTTON_GROUP:
        case LV_BOOTSTRAP_PAGINATION:
            lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW_WRAP);
            button(r, root, preset == LV_BOOTSTRAP_PAGINATION ? "<" : "Left", false);
            button(r, root, preset == LV_BOOTSTRAP_PAGINATION ? "1" : "Middle", true);
            button(r, root, preset == LV_BOOTSTRAP_PAGINATION ? "2" : "Right", false);
            if(preset == LV_BOOTSTRAP_PAGINATION) button(r, root, ">", false);
            for(uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
                lv_obj_t * item = lv_obj_get_child(root, i);
                lv_obj_set_checkable(item, true); lv_obj_set_radio_button(item, true);
                lv_obj_set_style_bg_color(item, lv_color_hex(0x0d6efd), LV_STATE_CHECKED);
            } break;
        case LV_BOOTSTRAP_ACCORDION:
        case LV_BOOTSTRAP_COLLAPSE: {
            unsigned count = preset == LV_BOOTSTRAP_ACCORDION ? 3 : 1;
            for(unsigned i = 0; i < count; ++i) {
                lv_obj_t * section = box(r, root, "Section", LV_PCT(100), LV_SIZE_CONTENT, false);
                lv_obj_t * trigger = button(r, section, i == 0 ? "Overview  +" : i == 1 ? "Details  +" : "More information  +", false);
                lv_obj_set_width(trigger, LV_PCT(100));
                lv_obj_t * body = text(r, section, "Expandable content. Edit this text or add widgets to this section.");
                if(i) lv_obj_set_hidden(body, true);
                lv_obj_add_event_cb(trigger, collapse, LV_EVENT_CLICKED, NULL);
            } break;
        }
        case LV_BOOTSTRAP_NAVBAR:
            text(r, root, "NORTHSTAR");
            lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW_WRAP);
            button(r, root, "Overview", true); button(r, root, "Projects", false); input(r, root, "Search", false); break;
        case LV_BOOTSTRAP_SIDEBAR:
        case LV_BOOTSTRAP_LIST_GROUP:
            button(r, root, "Dashboard", true); button(r, root, "Projects", false);
            button(r, root, "Team", false); button(r, root, "Settings", false); break;
        case LV_BOOTSTRAP_OFFCANVAS:
        case LV_BOOTSTRAP_MODAL: {
            lv_obj_t * trigger = button(r, root, preset == LV_BOOTSTRAP_MODAL ? "Open dialog" : "Open navigation", true);
            lv_obj_t * body = box(r, root, preset == LV_BOOTSTRAP_MODAL ? "Dialog" : "Drawer", LV_PCT(100), LV_SIZE_CONTENT, false);
            text(r, body, preset == LV_BOOTSTRAP_MODAL ? "Confirm changes" : "Workspace navigation");
            if(preset == LV_BOOTSTRAP_MODAL) {
                text(r, body, "Review your changes before continuing."); button(r, body, "Save changes", true);
            } else {
                button(r, body, "Dashboard", true); button(r, body, "Projects", false); button(r, body, "Settings", false);
            }
            dismiss(r, body); lv_obj_add_event_cb(trigger, collapse, LV_EVENT_CLICKED, NULL); break;
        }
        case LV_BOOTSTRAP_PLACEHOLDER:
            for(unsigned i = 0; i < 3; ++i) {
                lv_obj_t * line = box(r, root, "Loading placeholder", LV_PCT(i == 2 ? 60 : 100), 14, false);
                lv_obj_set_style_bg_color(line, muted(r), 0); lv_obj_set_style_bg_opa(line, LV_OPA_40, 0);
                lv_obj_set_style_border_width(line, 0, 0);
            } break;
        case LV_BOOTSTRAP_POPOVER:
        case LV_BOOTSTRAP_TOOLTIP: {
            lv_obj_t * trigger = button(r, root, preset == LV_BOOTSTRAP_POPOVER ? "Toggle popover" : "Show hint", false);
            lv_obj_t * hint = box(r, root, "Hint content", LV_PCT(100), LV_SIZE_CONTENT, false);
            text(r, hint, "Helpful information"); text(r, hint, "Native hint content, available on touch and pointer devices.");
            lv_obj_set_hidden(hint, true); lv_obj_add_event_cb(trigger, collapse, LV_EVENT_CLICKED, NULL); break;
        }
        case LV_BOOTSTRAP_PROGRESS: {
            text(r, root, "Project completion  68%");
            lv_obj_t * bar = own(r, lv_bar_create(root), "Completion");
            lv_obj_set_size(bar, LV_PCT(100), 16); lv_bar_set_value(bar, 68, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0x0d6efd), LV_PART_INDICATOR); break;
        }
        case LV_BOOTSTRAP_TOAST:
            text(r, root, "Notification  -  just now"); text(r, root, "A new report is ready to view."); dismiss(r, root); break;
        case LV_BOOTSTRAP_INPUT_GROUP: {
            text(r, root, "Website address"); input(r, root, "https://example.com", false); button(r, root, "Apply", true); break;
        }
        case LV_BOOTSTRAP_FLOATING_LABEL:
            lv_obj_set_style_text_color(text(r, root, "Email address"), muted(r), 0);
            input(r, root, "name@example.com", false); break;
        case LV_BOOTSTRAP_CHECKS_RADIOS: {
            lv_obj_t * check = own(r, lv_checkbox_create(root), "Enable notifications");
            lv_checkbox_set_text(check, "Enable notifications");
            lv_obj_t * toggle = own(r, lv_switch_create(root), "Dark mode");
            lv_obj_set_checked(toggle, true);
            text(r, root, "Notification frequency");
            lv_obj_t * group = box(r, root, "Radio group", LV_PCT(100), LV_SIZE_CONTENT, false);
            const char * options[] = {"Immediately", "Daily digest", "Weekly digest"};
            for(unsigned i = 0; i < 3; ++i) {
                lv_obj_t * radio = own(r, lv_checkbox_create(group), options[i]);
                lv_checkbox_set_text(radio, options[i]); lv_obj_set_radio_button(radio, true);
                lv_obj_set_style_radius(radio, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
                if(!i) lv_obj_set_checked(radio, true);
            } break;
        }
        case LV_BOOTSTRAP_VALIDATION:
        case LV_BOOTSTRAP_FORM:
            text(r, root, "Account details"); input(r, root, "Full name", false); input(r, root, "Email address", false);
            if(preset == LV_BOOTSTRAP_VALIDATION) {
                lv_obj_set_style_text_color(text(r, root, "Enter a valid email address to continue."), lv_color_hex(0xdc3545), 0);
                lv_obj_set_state(button(r, root, "Submit", true), LV_STATE_DISABLED, true);
            } else button(r, root, "Save details", true); break;
        case LV_BOOTSTRAP_ACTIVITY:
            text(r, root, "Recent activity"); text(r, root, "09:42  Report generated\n09:15  Team member invited\n08:50  Project updated"); break;
        default: break;
    }
    return root;
}

static lv_obj_t * template_create(recipe_t * r, lv_obj_t * parent, lv_bootstrap_preset_t preset)
{
    int width = lv_obj_get_content_width(parent);
    if(width < 280) width = 280;
    int height = lv_obj_get_content_height(parent);
    lv_obj_t * root = box(r, parent, lv_bootstrap_preset_name(preset), width, height > 0 ? height : 240, false);
    lv_obj_set_style_bg_color(root, lv_color_hex(r->dark ? 0x15191d : 0xf8f9fa), 0);
    text(r, root, "NORTHSTAR  /  WORKSPACE");
    text(r, root, lv_bootstrap_preset_name(preset));
    if(preset == LV_BOOTSTRAP_COMPONENT_GALLERY) {
        for(unsigned item = 0; item < LV_BOOTSTRAP_COMPONENT_COUNT; ++item) {
            text(r, root, lv_bootstrap_preset_name((lv_bootstrap_preset_t)item));
            component(r, root, (lv_bootstrap_preset_t)item);
        }
    } else if(preset <= LV_BOOTSTRAP_SETTINGS) {
        lv_obj_t * nav = box(r, root, "Admin navigation", LV_PCT(100), LV_SIZE_CONTENT, true);
        button(r, nav, "Overview", true); button(r, nav, "Reports", false); button(r, nav, "Settings", false);
        if(preset == LV_BOOTSTRAP_SETTINGS) {
            component(r, root, LV_BOOTSTRAP_FORM); component(r, root, LV_BOOTSTRAP_CHECKS_RADIOS);
        } else if(preset == LV_BOOTSTRAP_USERS) {
            input(r, root, "Search team members", false); button(r, root, "Invite member", true); native_table(r, root);
        } else {
            lv_obj_t * stats = box(r, root, "Key metrics", LV_PCT(100), LV_SIZE_CONTENT, true);
            lv_obj_t * metric = component(r, stats, LV_BOOTSTRAP_STAT_CARD); lv_obj_set_width(metric, width > 650 ? LV_PCT(31) : LV_PCT(100));
            metric = box(r, stats, "Orders", width > 650 ? LV_PCT(31) : LV_PCT(100), LV_SIZE_CONTENT, false);
            text(r, metric, "ORDERS"); text(r, metric, "1,428"); text(r, metric, "+8.2% this month");
            metric = box(r, stats, "Active users", width > 650 ? LV_PCT(31) : LV_PCT(100), LV_SIZE_CONTENT, false);
            text(r, metric, "ACTIVE USERS"); text(r, metric, "3,842"); text(r, metric, "+18.4% this month");
            text(r, root, preset == LV_BOOTSTRAP_COMMERCE ? "Sales performance" : "Activity over time");
            chart(r, root, preset == LV_BOOTSTRAP_COMMERCE);
            if(preset == LV_BOOTSTRAP_ANALYTICS) component(r, root, LV_BOOTSTRAP_PROGRESS);
            native_table(r, root); component(r, root, LV_BOOTSTRAP_ACTIVITY);
        }
    } else if(preset == LV_BOOTSTRAP_SIGN_IN) {
        text(r, root, "Welcome back"); input(r, root, "Email address", false); input(r, root, "Password", true);
        lv_obj_t * check = own(r, lv_checkbox_create(root), "Remember me"); lv_checkbox_set_text(check, "Remember me");
        button(r, root, "Sign in", true);
    } else if(preset == LV_BOOTSTRAP_CHECKOUT) {
        text(r, root, "Billing address"); input(r, root, "Full name", false); input(r, root, "Street address", false);
        input(r, root, "City / Postal code", false); text(r, root, "Order total  $149.00"); button(r, root, "Continue to payment", true);
    } else if(preset == LV_BOOTSTRAP_PRICING) {
        const char * plans[] = {"Free  /  $0", "Professional  /  $19", "Team  /  $49"};
        for(unsigned i = 0; i < 3; ++i) {
            lv_obj_t * plan = box(r, root, "Pricing plan", LV_PCT(100), LV_SIZE_CONTENT, false);
            text(r, plan, plans[i]); text(r, plan, "Projects, collaboration and support"); button(r, plan, "Choose plan", i == 1);
        }
    } else if(preset == LV_BOOTSTRAP_ALBUM) {
        for(unsigned i = 0; i < 3; ++i) {
            lv_obj_t * card = component(r, root, LV_BOOTSTRAP_CARD); lv_obj_set_width(card, LV_PCT(100));
            text(r, card, LV_SYMBOL_IMAGE "  Media placeholder");
        }
    } else if(preset == LV_BOOTSTRAP_BLOG) {
        text(r, root, "Ideas for a better workspace"); text(r, root, "DESIGN  /  NOVEMBER 2026");
        text(r, root, "A clear interface helps people focus on their work. Replace this sample article with your own content.");
        component(r, root, LV_BOOTSTRAP_CARD);
    } else {
        text(r, root, preset == LV_BOOTSTRAP_PRODUCT ? "Build something exceptional" : "Make room for your next idea");
        text(r, root, "A native LVGL starter with a clear message, useful content, and a focused next step.");
        button(r, root, "Get started", true);
        if(preset == LV_BOOTSTRAP_PRODUCT) component(r, root, LV_BOOTSTRAP_CARD);
    }
    return root;
}

lv_obj_t * lv_bootstrap_create(lv_obj_t * parent, lv_bootstrap_preset_t preset,
                              bool dark, lv_bootstrap_visit_cb_t visit, void * context)
{
    if(!parent || (unsigned)preset >= LV_BOOTSTRAP_PRESET_COUNT) return NULL;
    recipe_t recipe = {dark, visit, context};
    lv_obj_update_layout(parent);
    return preset >= LV_BOOTSTRAP_COMPONENT_COUNT ? template_create(&recipe, parent, preset) : component(&recipe, parent, preset);
}
#else
/* Keep builds with a minimal firmware configuration valid. Recipes require
 * their native control dependencies; name discovery remains available. */
const char * lv_bootstrap_get_action(lv_obj_t * object) { LV_UNUSED(object); return NULL; }
lv_obj_t * lv_bootstrap_create(lv_obj_t * parent, lv_bootstrap_preset_t preset,
                              bool dark, lv_bootstrap_visit_cb_t visit, void * context)
{
    LV_UNUSED(parent); LV_UNUSED(preset); LV_UNUSED(dark); LV_UNUSED(visit); LV_UNUSED(context);
    return NULL;
}
#endif
