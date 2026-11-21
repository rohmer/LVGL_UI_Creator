#pragma once
#include <lvgl/lvgl.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// Editor overlays never become project objects or serialized styles.
namespace EditorGrid
{
struct State
{
    lv_obj_t* root = nullptr;
    lv_obj_t* moving = nullptr;
    std::vector<lv_obj_t*> ignored;
    bool visible = true, snapping = false, guides = true;
    int spacing = 8;
    bool vertical = false, horizontal = false;
    int guideX = 0, guideY = 0;
};
inline State* active = nullptr;
inline bool Contains(const State& state, lv_obj_t* object)
{
    for(auto* parent = lv_obj_get_parent(object); parent; parent = lv_obj_get_parent(parent))
        if(parent == state.root)return true;
    return false;
}
inline void Clear(lv_obj_t* object = nullptr)
{
    if(!active || (object && active->moving != object))return;
    active->ignored.clear();
    active->moving = nullptr;active->vertical = active->horizontal = false;
    if(active->root)lv_obj_invalidate(active->root);
}
inline int Round(int value, int spacing)
{
    return (int)std::lround((double)value / spacing) * spacing;
}
inline void Move(lv_obj_t* object, int& x, int& y, bool allowX, bool allowY)
{
    if(!active || !Contains(*active, object))return;
    auto& state = *active;
    auto* parent = lv_obj_get_parent(object);
    state.moving = object;state.vertical = state.horizontal = false;
    lv_area_t root, area;lv_obj_get_content_coords(state.root, &root);lv_obj_get_content_coords(parent, &area);
    const int originX = area.x1 - lv_obj_get_scroll_x(parent), originY = area.y1 - lv_obj_get_scroll_y(parent);
    if(state.snapping) {
        if(allowX)x = Round(originX + x - root.x1, state.spacing) - originX + root.x1;
        if(allowY)y = Round(originY + y - root.y1, state.spacing) - originY + root.y1;
    }
    if(state.guides) {
        double zoomX = 1, zoomY = 1;
        for(auto* ancestor = parent; ancestor; ancestor = lv_obj_get_parent(ancestor)) {
            zoomX *= lv_obj_get_style_transform_scale_x(ancestor, LV_PART_MAIN) / 256.0;
            zoomY *= lv_obj_get_style_transform_scale_y(ancestor, LV_PART_MAIN) / 256.0;
        }
        const double toleranceX = state.snapping ? 6 / (std::max)(0.01, zoomX) : 0.5;
        const double toleranceY = state.snapping ? 6 / (std::max)(0.01, zoomY) : 0.5;
        double bestX = toleranceX + 1, bestY = toleranceY + 1;
        int adjustX = 0, adjustY = 0;
        const int width = lv_obj_get_width(object), height = lv_obj_get_height(object);
        auto consider = [&](int left, int top, int right, int bottom) {
            const int xs[] = {left, (left + right + 1) / 2, right + 1};
            const int ys[] = {top, (top + bottom + 1) / 2, bottom + 1};
            const int offsetsX[] = {0, width / 2, width};
            const int offsetsY[] = {0, height / 2, height};
            for(int anchor : xs)for(int offset : offsetsX) {
                int delta = anchor - (originX + x + offset);
                if(allowX && std::abs(delta) <= toleranceX && std::abs(delta) < bestX) {
                    bestX = std::abs(delta);adjustX = delta;state.guideX = anchor;state.vertical = true;
                }
            }
            for(int anchor : ys)for(int offset : offsetsY) {
                int delta = anchor - (originY + y + offset);
                if(allowY && std::abs(delta) <= toleranceY && std::abs(delta) < bestY) {
                    bestY = std::abs(delta);adjustY = delta;state.guideY = anchor;state.horizontal = true;
                }
            }
        };
        consider(area.x1, area.y1, area.x2, area.y2);
        for(uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
            auto* sibling = lv_obj_get_child(parent, i);
            if(sibling == object || lv_obj_is_hidden(sibling) || std::find(state.ignored.begin(),state.ignored.end(),sibling)!=state.ignored.end())continue;
            lv_area_t bounds;lv_obj_get_coords(sibling, &bounds);
            consider(bounds.x1, bounds.y1, bounds.x2, bounds.y2);
        }
        if(state.snapping) { x += adjustX;y += adjustY; }
    }
    lv_obj_invalidate(state.root);
}
inline void Draw(lv_event_t* event)
{
    auto& state = *static_cast<State*>(lv_event_get_user_data(event));
    const auto code = lv_event_get_code(event);
    if(code != LV_EVENT_DRAW_MAIN_END && code != LV_EVENT_DRAW_POST)return;
    lv_area_t area;lv_obj_get_content_coords(state.root, &area);
    auto* layer = lv_event_get_layer(event);
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);
    line.width = 1;
    auto draw = [&](int x1, int y1, int x2, int y2) {
        line.p1.x = x1;line.p1.y = y1;line.p2.x = x2;line.p2.y = y2;lv_draw_line(layer, &line);
    };
    if(code == LV_EVENT_DRAW_MAIN_END && state.visible) {
        line.color = lv_color_hex(0x8295a5);line.opa = LV_OPA_20;
        // Bound drawing work for unusually large project screens.
        int step = state.spacing;
        while((area.x2 - area.x1 + area.y2 - area.y1) / step > 2048)step *= 2;
        for(int x = area.x1; x <= area.x2; x += step)draw(x, area.y1, x, area.y2);
        for(int y = area.y1; y <= area.y2; y += step)draw(area.x1, y, area.x2, y);
    }
    if(code == LV_EVENT_DRAW_POST && state.moving && state.guides) {
        line.color = lv_color_hex(0x36c9f1);line.opa = LV_OPA_COVER;line.dash_width = 5;line.dash_gap = 3;
        if(state.vertical)draw(state.guideX, area.y1, state.guideX, area.y2);
        if(state.horizontal)draw(area.x1, state.guideY, area.x2, state.guideY);
    }
}
}

