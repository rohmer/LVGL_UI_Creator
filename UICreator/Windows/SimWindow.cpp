#include "SimWindow.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorDrag.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorEvents.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <new>
#if LVGL_VERSION_MAJOR >= 9
#include <src/core/lv_obj_class_private.h>
#include <src/core/lv_obj_private.h>
#include <src/core/lv_obj_style_private.h>
#include <Serialization/NativeStyles.h>
#include <Serialization/ProjectKeyboard.h>
#include <Serialization/RuntimeFeatures.h>
#include "../Fonts/FontMemory.h"
#if LVSERIAL_USE_ADVANCED_KEYBOARD
#include <lv_advanced_keyboard.h>
#endif

struct RuntimeKeyboardMemory {
    std::string language;
    int width = 0, height = 0;
    const lv_font_t* inheritedFont = nullptr;
    nlohmann::json rawSettings, settings;
    size_t widgets = 0;
    std::vector<const lv_font_t*> fonts;
    bool inputKnown = false, attempted = false, available = false;
};
#endif

namespace {
#if LVGL_VERSION_MAJOR >= 9
bool TextWidget(lv_obj_t* object)
{
    return lv_obj_has_class(object,&lv_label_class)||lv_obj_has_class(object,&lv_roller_class)||
        lv_obj_has_class(object,&lv_dropdown_class)||lv_obj_has_class(object,&lv_buttonmatrix_class)||
        lv_obj_has_class(object,&lv_table_class)||lv_obj_has_class(object,&lv_scale_class);
}
bool FontStyle(const lv_obj_style_t& style)
{
    return !style.is_disabled&&!style.is_trans&&!Serialization::NativeStyles::IsTransientStyle(style.style);
}
const lv_font_t* StyleFont(lv_obj_t* object,uint32_t part,uint32_t state)
{
    const lv_font_t* found=nullptr;int weight=-1;
    // Use LVGL's style ordering and state specificity, without temporarily
    // changing an object's state or disturbing its style cache/animations.
    for(unsigned i=0;i<object->style_cnt;++i) {
        const auto& style=object->styles[i];
        const auto selectedPart=style.selector&0xff0000u,selectedState=style.selector&0xffffu;
        if(!FontStyle(style)||selectedPart!=part||(selectedState&~state)||int(selectedState)<=weight)continue;
        lv_style_value_t value{};
        if(lv_style_get_prop(style.style,LV_STYLE_TEXT_FONT,&value)!=LV_STYLE_RES_FOUND||!value.ptr)continue;
        found=static_cast<const lv_font_t*>(value.ptr);weight=selectedState;
        if(selectedState==state)return found;
    }
    return found;
}
template<class Fonts>
void AuthoredFonts(lv_obj_t* object,Fonts& fonts)
{
    for(unsigned i=0;i<object->style_cnt;++i) {
        const auto& style=object->styles[i];
        if(!FontStyle(style)||style.is_theme)continue;
        lv_style_value_t font{};
        if(lv_style_get_prop(style.style,LV_STYLE_TEXT_FONT,&font)==LV_STYLE_RES_FOUND&&
            font.ptr==StyleFont(object,style.selector&0xff0000u,style.selector&0xffffu))
            fonts.Include(static_cast<const lv_font_t*>(font.ptr));
    }
}
template<class Fonts>
void TextFonts(lv_obj_t* object,lv_obj_t* root,Fonts& fonts,uint32_t part)
{
    std::set<uint32_t> states{0u,static_cast<uint32_t>(lv_obj_get_state(object))};
    for(unsigned i=0;i<object->style_cnt;++i) {
        const auto& style=object->styles[i];lv_style_value_t value{};
        if(FontStyle(style)&&(style.selector&0xff0000u)==part&&lv_style_get_prop(style.style,LV_STYLE_TEXT_FONT,&value)==LV_STYLE_RES_FOUND)
            states.insert(style.selector&0xffffu);
    }
    bool inherit=false;
    for(const auto state:states) {if(const auto* font=StyleFont(object,part,state))fonts.Include(font);else inherit=true;}
    if(inherit) {
        if(part!=LV_PART_MAIN)TextFonts(object,root,fonts,LV_PART_MAIN);
        else if(object!=root&&lv_obj_get_parent(object))TextFonts(lv_obj_get_parent(object),root,fonts,LV_PART_MAIN);
        else fonts.Include(LV_FONT_DEFAULT);
    }
}
template<class Fonts>
size_t WidgetInstanceBytes(lv_obj_t* object,lv_obj_t* root,Fonts& fonts,bool skipLegacyKeyboard=false)
{
#if LVSERIAL_USE_ADVANCED_KEYBOARD
    if(skipLegacyKeyboard && lv_advanced_keyboard_is_instance(object))return 0;
#endif
    // LVGL allocates the first nonzero instance_size in the class chain.
    // That size includes the base object; inherited classes are not additive.
    const auto* widgetClass = lv_obj_get_class(object);
    while (widgetClass && !widgetClass->instance_size) widgetClass = widgetClass->base_class;
    size_t bytes = widgetClass ? widgetClass->instance_size : 0;
    // Authored fonts remain resources even when assigned to a currently hidden
    // object or a part/state that is not active in the preview.
    AuthoredFonts(object,fonts);
    if(TextWidget(object)) {
        if(lv_obj_has_class(object,&lv_label_class)||lv_obj_has_class(object,&lv_roller_class)||lv_obj_has_class(object,&lv_dropdown_class))
            TextFonts(object,root,fonts,LV_PART_MAIN);
        if(lv_obj_has_class(object,&lv_roller_class))TextFonts(object,root,fonts,LV_PART_SELECTED);
        if(lv_obj_has_class(object,&lv_table_class)||lv_obj_has_class(object,&lv_buttonmatrix_class)||lv_obj_has_class(object,&lv_scale_class))
            TextFonts(object,root,fonts,LV_PART_ITEMS);
        if(lv_obj_has_class(object,&lv_scale_class))TextFonts(object,root,fonts,LV_PART_INDICATOR);
    }
    const auto count = lv_obj_get_child_count(object);
    for (uint32_t index = 0; index < count; ++index)
        bytes += WidgetInstanceBytes(lv_obj_get_child(object, index),root,fonts,skipLegacyKeyboard);
    return bytes;
}

struct ProfileFonts {
    std::set<const lv_font_t*> fonts;
    void Include(const lv_font_t* font) { if(font)fonts.insert(font); }
};

// A probe never becomes the active screen or joins an editor input group.
// The canonical widget constructs its exact private LVGL tree for measurement;
// its temporary engine/workspace is destroyed before returning to the editor.
struct KeyboardMemoryProbe {
    lv_display_t* display = lv_display_get_default();
    lv_group_t* defaultGroup = lv_group_get_default();
    lv_group_t* editorGroup;
    lv_obj_t* defaultFocus = defaultGroup ? lv_group_get_focused(defaultGroup) : nullptr;
    lv_obj_t* editorFocus;
    bool defaultEditing = defaultGroup && lv_group_get_editing(defaultGroup);
    bool editorEditing;
    lv_obj_t* screen = nullptr;

    KeyboardMemoryProbe(lv_display_t* targetDisplay,lv_group_t* group) :
        editorGroup(group), editorFocus(group ? lv_group_get_focused(group) : nullptr),
        editorEditing(group && lv_group_get_editing(group))
    {
        lv_group_set_default(nullptr);
        lv_display_set_default(targetDisplay);
    }
    ~KeyboardMemoryProbe()
    {
        if(screen)lv_obj_delete(screen);
        lv_display_set_default(display);
        lv_group_set_default(defaultGroup);
        auto restore=[](lv_group_t* group,lv_obj_t* focus,bool editing) {
            if(!group)return;
            if(focus && lv_obj_is_valid(focus) && lv_group_get_focused(group) != focus)lv_group_focus_obj(focus);
            if(lv_group_get_editing(group) != editing)lv_group_set_editing(group,editing);
        };
        restore(defaultGroup,defaultFocus,defaultEditing);
        if(editorGroup != defaultGroup)restore(editorGroup,editorFocus,editorEditing);
    }
};

void MeasureRuntimeKeyboard(RuntimeKeyboardMemory& profile,lv_obj_t* canvas,lv_group_t* group,const nlohmann::json& settings)
{
#if LVSERIAL_USE_ADVANCED_KEYBOARD
    const auto* font=lv_obj_get_style_text_font(canvas,LV_PART_MAIN);
    const int width=lv_obj_get_width(canvas),screenHeight=lv_obj_get_height(canvas);
    const int height=(std::min)(settings.value("height",Serialization::ProjectKeyboard::DefaultHeight(screenHeight)),screenHeight);
    const auto& language=settings.at("language").get_ref<const std::string&>();
    if(profile.attempted && profile.language==language && profile.width==width && profile.height==height && profile.inheritedFont==font)return;
    profile.language=language;profile.width=width;profile.height=height;profile.inheritedFont=font;
    profile.attempted=true;profile.available=false;
    profile.widgets=0;profile.fonts.clear();
    KeyboardMemoryProbe probe(lv_obj_get_display(canvas),group);
    probe.screen=lv_obj_create(nullptr);
    if(!probe.screen)throw std::bad_alloc();
    lv_obj_set_hidden(probe.screen,true);
    lv_obj_set_style_text_font(probe.screen,font,LV_PART_MAIN);
    auto* keyboard=lv_advanced_keyboard_create(probe.screen);
    if(!keyboard)throw std::bad_alloc();
    lv_obj_set_size(keyboard,width,height);
    if(!lv_advanced_keyboard_set_language(keyboard,language.c_str()))
        throw std::invalid_argument("Could not measure the keyboard language");
    lv_advanced_keyboard_set_right_to_left(keyboard,settings.at("rightToLeft").get<bool>());
    ProfileFonts fonts;
    profile.widgets=WidgetInstanceBytes(keyboard,keyboard,fonts);
    profile.fonts.assign(fonts.fonts.begin(),fonts.fonts.end());
    profile.available=true;
#else
    (void)profile;(void)canvas;(void)group;(void)settings;
    throw std::invalid_argument("Advanced Keyboard is unavailable in this LVGL build");
#endif
}
struct MemoryHint {lv_obj_t *label=nullptr,*popup=nullptr;};
void HideMemoryHint(MemoryHint* hint)
{
    if(!hint->popup)return;
    auto* popup=hint->popup;hint->popup=nullptr;lv_obj_delete(popup);
}
void MemoryHintEvent(lv_event_t* event)
{
    auto* hint=static_cast<MemoryHint*>(lv_event_get_user_data(event));
    switch(lv_event_get_code(event)) {
    case LV_EVENT_DELETE:HideMemoryHint(hint);delete hint;break;
    case LV_EVENT_HOVER_LEAVE:
    case LV_EVENT_PRESSED:HideMemoryHint(hint);break;
    case LV_EVENT_HOVER_OVER: {
        if(hint->popup)break;
        auto* display=lv_obj_get_display(hint->label);
        hint->popup=lv_obj_create(lv_display_get_layer_sys(display));
        // The system layer can be cleared independently of the editor window.
        lv_obj_add_event_cb(hint->popup,[](lv_event_t* e){static_cast<MemoryHint*>(lv_event_get_user_data(e))->popup=nullptr;},LV_EVENT_DELETE,hint);
        lv_obj_set_width(hint->popup,(std::min)(440,lv_display_get_horizontal_resolution(display)-12));
        lv_obj_set_height(hint->popup,LV_SIZE_CONTENT);lv_obj_set_style_pad_all(hint->popup,8,0);
        lv_obj_set_style_bg_color(hint->popup,lv_color_hex(EditorStyle::Input),0);
        lv_obj_set_style_text_color(hint->popup,lv_color_hex(EditorStyle::Text),0);
        lv_obj_set_clickable(hint->popup,false);lv_obj_set_scrollable(hint->popup,false);
        auto* text=lv_label_create(hint->popup);lv_obj_set_width(text,LV_PCT(100));lv_label_set_long_mode(text,LV_LABEL_LONG_MODE_WRAP);
        lv_label_set_text(text,"LVGL widget instances plus unique used bitmap-font structures, glyphs, bitmap, Unicode and kerning tables. Includes the configured runtime keyboard and fallback fonts; shared fonts count once per screen. Excludes prediction tries/engine workspace, source TTFs, allocator overhead, text/style/image buffers, opaque external font data and temporary rendering memory. Sizes use the current native LVGL build.");
        lv_obj_update_layout(hint->popup);lv_area_t anchor{};lv_obj_get_coords(hint->label,&anchor);
        const int width=lv_obj_get_width(hint->popup),height=lv_obj_get_height(hint->popup);
        const int x=std::clamp(anchor.x1,4,(std::max)(4,lv_display_get_horizontal_resolution(display)-width-4));
        int y=anchor.y2+6;if(y+height>lv_display_get_vertical_resolution(display)-4)y=anchor.y1-height-6;
        lv_obj_set_pos(hint->popup,x,(std::max)(4,y));break;
    }
    default:break;
    }
}
#endif
bool ReadDimension(lv_obj_t* field, int& value)
{
    const char* text = lv_ta_get_text(field);
    const char* end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end && value > 0;
}
}

SimWindow::SimWindow(int iWidth, int iHeight) :
    width(iWidth),
    height(iHeight)
{
#if LVGL_VERSION_MAJOR >= 9
    fontMemory=std::make_unique<ScreenFontMemory>();
    runtimeKeyboardMemory=std::make_unique<RuntimeKeyboardMemory>();
#endif
    createObjects();
}
SimWindow::~SimWindow() = default;

int SimWindow::updateFrame()
{
    std::stringstream title;
    title << (width < 200 ? "Simulator\n[Size:" : "Simulator [Size:") << width << "x" << height << "]";
    EditorWindow::SetTitle(window, title.str().c_str());
#if LVGL_VERSION_MAJOR >= 9
    // Keep the byte count on its own line so it remains readable at TFT sizes.
    auto* header = EditorWindow::Header(window);
    const int lineHeight = lv_font_get_line_height(lv_obj_get_style_text_font(header, LV_PART_MAIN));
    const int titleHeight = std::max(28, lineHeight * (width < 200 ? 2 : 1));
    const int headerSize = 12 + titleHeight + 4 + lineHeight;
    lv_obj_set_height(header, headerSize);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return headerSize;
#else
    return EditorWindow::GetHeaderHeight(window);
#endif
}

void SimWindow::RefreshMemoryUsage()
{
#if LVGL_VERSION_MAJOR >= 9
    if (!memoryLabel) return;
    const nlohmann::json* settings=nullptr;
    bool configured=false,enabled=false,unavailable=false;
    try {
        const auto raw=runtimeKeyboardSettings ? runtimeKeyboardSettings() : nlohmann::json{};
        configured=!raw.is_null();
        if(configured) {
            if(!runtimeKeyboardMemory->inputKnown || runtimeKeyboardMemory->rawSettings!=raw) {
                auto normalized=Serialization::ProjectKeyboard::Normalize(raw);
                runtimeKeyboardMemory->rawSettings=raw;
                runtimeKeyboardMemory->settings=std::move(normalized);
                runtimeKeyboardMemory->inputKnown=true;
            }
            settings=&runtimeKeyboardMemory->settings;
            enabled=settings->at("enabled").get<bool>();
        }
    }catch(const std::exception&) {unavailable=true;}
    fontMemory->Begin();
    auto widgets=WidgetInstanceBytes(drawSurface,drawSurface,*fontMemory,configured);
    if(enabled) {
        try {
            MeasureRuntimeKeyboard(*runtimeKeyboardMemory,drawSurface,keyboardGroup,*settings);
            if(runtimeKeyboardMemory->available) {
                widgets+=runtimeKeyboardMemory->widgets;
                for(const auto* font:runtimeKeyboardMemory->fonts)fontMemory->Include(font);
            } else unavailable=true;
        }catch(const std::exception&) {unavailable=true;}
    } else if(!unavailable) {
        runtimeKeyboardMemory->attempted=false;
        runtimeKeyboardMemory->fonts.clear();
    }
    const auto bytes=widgets+fontMemory->End();
    if (bytes == memoryBytes && unavailable == memoryEstimateUnavailable) return;
    memoryBytes = bytes;
    memoryEstimateUnavailable=unavailable;
    lv_label_set_text_fmt(memoryLabel, unavailable ? "Screen Memory: %zu bytes (keyboard estimate unavailable)" : "Screen Memory: %zu bytes", bytes);
#endif
}

void SimWindow::resize(int newWidth, int newHeight, bool repos, bool resize)
{
    EditorEdit::Scope edit;
    float xDelta = static_cast<float>(newWidth) / static_cast<float>(width);
    float yDelta = static_cast<float>(newHeight) / static_cast<float>(height);

    width = newWidth;
    height = newHeight;
    const int headerSize = updateFrame();
    lv_obj_set_size(window, width + 12, height + headerSize + 12);
    lv_obj_set_size(drawSurface, width, height);
    if (repos || resize)
    {
        std::vector<lv_obj_t*> children = ObjectTools::GetChildren(drawSurface);
        for (std::vector<lv_obj_t*>::iterator it = children.begin(); it != children.end(); ++it)
        {
            if (repos)
            {
                lv_obj_set_pos(*it,
                               round((lv_obj_get_x(*it) * xDelta)),
                               round((lv_obj_get_y(*it) * yDelta))
                );
            }
            if (resize)
            {
                lv_obj_set_size(*it,
                                round((lv_obj_get_width(*it) * xDelta)),
                                round((lv_obj_get_height(*it) * yDelta))
                );
            }
        }
    }
}

void SimWindow::createObjects()
{
    window = EditorWindow::Create(lv_scr_act(), false);
#if LVGL_VERSION_MAJOR >= 9
    EditorDrag::Attach(EditorWindow::Header(window), window);
#endif
    const int headerSize = updateFrame();

    lv_obj_set_size(window, width + 12, height + headerSize + 12);
    lv_area_t screenBounds;
    lv_obj_get_coords(lv_scr_act(), &screenBounds);
    int x = (screenBounds.x2 / 2) - (width / 2);
    int y = (screenBounds.y2 / 2) - (width / 2) - 50;
#if LVGL_VERSION_MAJOR >= 9
    const int workspaceLeft = 528;
    const int workspaceRight = screenBounds.x2 + 1 - 405;
    x = workspaceLeft + (workspaceRight - workspaceLeft - width - 12) / 2;
    y = 136;
#endif
    lv_obj_set_pos(window, x, y);
    EditorWindow::HideScrollbar(window);
    lv_obj_t* winBtn = EditorWindow::AddButton(window, LV_SYMBOL_SETTINGS, "settings");
    lv_obj_set_user_data(winBtn, this);
    SetEditorEventCallback<settings>(winBtn);
#if LVGL_VERSION_MAJOR >= 9
    memoryLabel = lv_label_create(EditorWindow::Header(window));
    lv_obj_set_ignore_layout(memoryLabel, true);
    lv_obj_set_width(memoryLabel, LV_PCT(100));
    lv_label_set_long_mode(memoryLabel, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(memoryLabel, lv_color_hex(EditorStyle::Muted), LV_PART_MAIN);
    lv_obj_align(memoryLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_clickable(memoryLabel,true);lv_obj_set_click_focusable(memoryLabel,false);
    auto* hint=new MemoryHint{memoryLabel};
    lv_obj_add_event_cb(memoryLabel,MemoryHintEvent,LV_EVENT_ALL,hint);
#endif
    drawSurface = lv_cont_create(EditorWindow::Content(window), nullptr);
    lv_obj_set_style(drawSurface, &lv_style_transp);
    lv_cont_set_layout(drawSurface, LV_LAYOUT_OFF);
#if LVGL_VERSION_MAJOR < 9
    lv_obj_set_auto_realign(drawSurface, false);
#endif
    lv_obj_set_size(drawSurface, width, height);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_pos(drawSurface, 5, 5);
#else
    lv_obj_set_x(drawSurface, lv_obj_get_x(drawSurface) - 3);
    lv_obj_set_y(drawSurface, lv_obj_get_y(drawSurface) - 6);
#endif
    RefreshMemoryUsage();
}

void SimWindow::settings(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    SimWindow* sw = static_cast<SimWindow*>(lv_obj_get_user_data(obj));

    if (sw->settingsWindow != nullptr)
    {
        SetEditorHidden(sw->settingsWindow, false);
        lv_obj_move_foreground(sw->settingsWindow);
        auto* controls = static_cast<cbStruct*>(lv_obj_get_user_data(sw->settingsWindow));
        focusDimension(controls, controls->width);
        return;
    }
    sw->settingsWindow = EditorWindow::Create(lv_scr_act());
    lv_obj_set_size(sw->settingsWindow, 400, 450);
    EditorWindow::SetTitle(sw->settingsWindow, "TFT Settings");
    lv_area_t screenBounds;
    lv_obj_get_coords(lv_scr_act(), &screenBounds);
    int x = (screenBounds.x2 / 2) - 150;
    int y = (screenBounds.y2 / 2) - 150;
    lv_obj_set_pos(sw->settingsWindow, x, y);
    lv_obj_t* close_btn = EditorWindow::AddButton(sw->settingsWindow, LV_SYMBOL_CLOSE);
    lv_obj_set_user_data(close_btn, sw->settingsWindow);
    SetEditorEventCallback<closeSettings>(close_btn);

    auto* content = EditorWindow::Content(sw->settingsWindow);
    lv_obj_t* wLabel = lv_label_create(content, nullptr);
    lv_obj_set_pos(wLabel, 56, 23);
    lv_label_set_text(wLabel, "TFT Width:");
    lv_obj_t* hLabel = lv_label_create(content, nullptr);
    lv_obj_set_pos(hLabel, 50, 73);
    lv_label_set_text(hLabel, "TFT Height:");

    lv_obj_t* wTA = lv_ta_create(content, nullptr);
    lv_ta_set_accepted_chars(wTA, "0123456789");
    lv_obj_set_pos(wTA, 150, 12);
    std::stringstream widthSS;
    lv_obj_set_width(wTA, 75);
    widthSS << sw->width;
    lv_ta_set_text(wTA, widthSS.str().c_str());
    lv_ta_set_one_line(wTA, true);

    lv_obj_t* hTA = lv_ta_create(content, nullptr);
    lv_ta_set_accepted_chars(hTA, "0123456789");
    lv_obj_set_pos(hTA, 150, 63);
    std::stringstream heightSS;
    heightSS << sw->height;
    lv_obj_set_width(hTA, 75);
    lv_ta_set_text(hTA, heightSS.str().c_str());
    lv_ta_set_one_line(hTA, true);

    lv_ta_set_cursor_type(wTA, LV_CURSOR_NONE);
    lv_ta_set_cursor_type(hTA, LV_CURSOR_NONE);

    lv_obj_t* kb = lv_kb_create(content, nullptr);
    lv_obj_set_y(kb, lv_obj_get_y(kb) - 10);
#if LVGL_VERSION_MAJOR >= 9
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
#else
    static const char* kb_map_num[] =
    {
        "7", "8", "9", "\n",
        "4", "5", "6", "\n",
        "1", "2", "3", "\n",
        LV_SYMBOL_LEFT, "0",LV_SYMBOL_RIGHT, "\n",
        "Bksp", ""
    };
    static const lv_btnm_ctrl_t kb_ctrl_num[] =
    {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1,
        LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 1, 1, LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 1,
        LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG | 2
    };
    lv_kb_set_map(kb, kb_map_num);
    lv_kb_set_ctrl_map(kb, kb_ctrl_num);
#endif

    SetEditorHidden(kb, true);

    lv_obj_t* pos = lv_cb_create(content, nullptr);
    lv_cb_set_text(pos, "Reposition objects");
    lv_cb_set_checked(pos, true);
    lv_obj_set_pos(pos, 20, 110);

    lv_obj_t* sze = lv_cb_create(content, nullptr);
    lv_cb_set_text(sze, "Resize objects");
    lv_cb_set_checked(sze, true);
    lv_obj_set_pos(sze, 240, 110);

    lv_obj_t* submitBtn = lv_btn_create(content, nullptr);
    lv_obj_t* btnLabel = lv_label_create(submitBtn, nullptr);
    lv_label_set_text(btnLabel, "Submit Changes");
    lv_obj_set_pos(submitBtn, 25, 150);
    lv_obj_set_size(submitBtn, 350, 30);
    SetEditorHidden(submitBtn, true);

    cbStruct* cb = new cbStruct();
    cb->sw = sw;
    cb->height = hTA;
    cb->width = wTA;
    cb->sze = sze;
    cb->pos = pos;
    cb->setBtn = submitBtn;
    cb->kb = kb;
    lv_obj_set_user_data(sw->settingsWindow, cb);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_add_event_cb(sw->settingsWindow, [](lv_event_t* event) {
        auto* controls = static_cast<cbStruct*>(lv_event_get_user_data(event));
        // LVGL destroys input devices before windows during shutdown. Remove
        // both fields directly while their callbacks still have live controls.
        lv_group_remove_obj(controls->width);
        lv_group_remove_obj(controls->height);
        lv_kb_set_ta(controls->kb, nullptr);
        controls->sw->settingsWindow = nullptr;
        delete controls;
    }, LV_EVENT_DELETE, cb);
#endif

    lv_obj_set_user_data(wTA, cb);
    lv_obj_set_user_data(hTA, cb);
    lv_obj_set_user_data(submitBtn, cb);
    SetEditorEventCallback<submitClicked>(submitBtn);

    SetEditorEventCallback<dimensionChanged>(wTA);
    SetEditorEventCallback<dimensionChanged>(hTA);
    focusDimension(cb, wTA);
}

void SimWindow::closeSettings(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    lv_obj_t* win = static_cast<lv_obj_t*>(lv_obj_get_user_data(obj));
#if LVGL_VERSION_MAJOR >= 9
    EditorWindow::ReleaseFocus(win);
#endif
    auto* controls = static_cast<cbStruct*>(lv_obj_get_user_data(win));
    lv_kb_set_ta(controls->kb, nullptr);
    SetEditorHidden(win, true);
}

void SimWindow::focusDimension(cbStruct* controls, lv_obj_t* field)
{
#if LVGL_VERSION_MAJOR >= 9
    if (auto* group = controls->sw->keyboardGroup) {
        FocusEditorControl(group, field);
        // Keep both dimensions in the group so Tab can switch between them.
        // FOCUSED events only retarget the keypad; they must not add fields
        // back while ReleaseFocus is removing a closing dialog's controls.
        auto* peer = field == controls->width ? controls->height : controls->width;
        if (lv_obj_get_group(peer) != group) lv_group_add_obj(group, peer);
        lv_group_set_editing(group, true);
    }
#endif
    SetEditorHidden(controls->kb, false);
    lv_ta_set_cursor_type(controls->width, field == controls->width ? LV_CURSOR_LINE : LV_CURSOR_NONE);
    lv_ta_set_cursor_type(controls->height, field == controls->height ? LV_CURSOR_LINE : LV_CURSOR_NONE);
    lv_kb_set_ta(controls->kb, field);
}

void SimWindow::dimensionChanged(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED && event != LV_EVENT_FOCUSED &&
        event != LV_EVENT_VALUE_CHANGED && event != LV_EVENT_READY)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    int width = 0, height = 0;
    const bool changed = ReadDimension(cb->width, width) && ReadDimension(cb->height, height) &&
        (width != cb->sw->width || height != cb->sw->height);
    SetEditorHidden(cb->setBtn, !changed);
    if (event == LV_EVENT_CLICKED) focusDimension(cb, obj);
    else if (event == LV_EVENT_FOCUSED) {
        SetEditorHidden(cb->kb, false);
        lv_ta_set_cursor_type(cb->width, obj == cb->width ? LV_CURSOR_LINE : LV_CURSOR_NONE);
        lv_ta_set_cursor_type(cb->height, obj == cb->height ? LV_CURSOR_LINE : LV_CURSOR_NONE);
        lv_kb_set_ta(cb->kb, obj);
    }
    else if (event == LV_EVENT_READY && changed) submitClicked(cb->setBtn, LV_EVENT_CLICKED);
}

void SimWindow::submitClicked(lv_obj_t* obj, EditorEventCode event)
{
    if (event != LV_EVENT_CLICKED)
        return;
    cbStruct* cb = static_cast<cbStruct*>(lv_obj_get_user_data(obj));
    int width = 0, height = 0;
    if (!ReadDimension(cb->width, width) || !ReadDimension(cb->height, height))
        return;
    cb->sw->resize(width, height, lv_cb_is_checked(cb->pos), lv_cb_is_checked(cb->sze));
#if LVGL_VERSION_MAJOR >= 9
    EditorWindow::ReleaseFocus(cb->sw->settingsWindow);
#endif
    lv_kb_set_ta(cb->kb, nullptr);
    SetEditorHidden(cb->sw->settingsWindow, true);
}
