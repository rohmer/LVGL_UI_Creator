#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>

#include "../../../UICreator/ObjectCreation/Arc.h"
#include "../../../UICreator/ObjectCreation/Bar.h"
#include "../../../UICreator/ObjectCreation/Button.h"
#include "../../../UICreator/ObjectCreation/ButtonMatrix.h"
#include "../../../UICreator/ObjectCreation/Calendar.h"
#include "../../../UICreator/ObjectCreation/Canvas.h"
#include "../../../UICreator/ObjectCreation/Chart.h"
#include "../../../UICreator/ObjectCreation/CheckBox.h"
#include "../../../UICreator/ObjectCreation/Container.h"
#include "../../../UICreator/Utils/EditorEvents.h"
#include "../../../UICreator/Utils/EditorDrag.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/ObjectTools.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include <Serialization/LVArc.h>
#include <Serialization/LVLabel.h>

namespace
{
    int clicks = 0;
    lv_obj_t* lastTarget = nullptr;
    lv_indev_data_t pointerSample{};
    lv_obj_t* completedDrag = nullptr;

    void ReadPointer(lv_indev_t*, lv_indev_data_t* data)
    {
        data->point = pointerSample.point;
        data->state = pointerSample.state;
    }

    void RecordDrag(lv_obj_t* target)
    {
        completedDrag = target;
    }

    void UnexpectedClick(lv_obj_t*, EditorEventCode code)
    {
        if (code == LV_EVENT_CLICKED)
            clicks += 100;
    }

    void TrackClick(lv_obj_t* target, EditorEventCode code)
    {
        if (code == LV_EVENT_CLICKED)
        {
            lastTarget = target;
            ++clicks;
        }
    }

    void Check(bool passed, const char* description)
    {
        if (!passed)
        {
            std::fprintf(stderr, "%s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    SdlSimulator simulator;
    Check(simulator.Initialize(1200, 800), "SDL initialization failed");
    Check(lv_display_get_horizontal_resolution(nullptr) == 1200 &&
          lv_display_get_vertical_resolution(nullptr) == 800, "Unexpected display size");
    Check(lv_indev_get_type(simulator.GetKeyboard()) == LV_INDEV_TYPE_KEYPAD, "SDL keypad missing");

    auto* screen = lv_screen_active();
    auto* arc = Arc::Create(screen, 10, 20);
    const auto created = Serialization::LVArc::ToJSON(arc);
    Check(created["arc"]["angle"]["start"] == 90 && created["arc"]["angle"]["end"] == 270,
          "Created arc has unexpected angles");

    const auto legacy = json::parse(R"({
        "base": {
            "name": "Progress arc",
            "coords": {"x": 12, "y": 24, "width": 100, "height": 120},
            "click": true, "hidden": false, "parevent": true,
            "dragDir": 1, "opascale": 180, "opascaleen": true,
            "style": {"body": {"main_color": 4278190335}}
        },
        "arc": {
            "angle": {"start": 25, "end": 125},
            "style": {"line.rounded": true, "line.width": 8, "line.color": 4278190335, "line.opa": 255}
        }
    })");
    auto* loaded = Serialization::LVArc::FromJSON(legacy);
    Check(loaded != nullptr, "Legacy arc could not be loaded");
    const auto saved = Serialization::LVArc::ToJSON(loaded);
    for (const auto& property : legacy["base"].items())
        Check(saved["base"][property.key()] == property.value(), "Legacy arc base property changed after round trip");
    for (const auto& property : legacy["arc"].items())
        Check(saved["arc"][property.key()] == property.value(), "Legacy arc properties changed after round trip");
    Check(lv_color_to_u32(lv_obj_get_style_bg_color(loaded, LV_PART_MAIN)) == 4278190335u,
          "Legacy background color was not applied");
    Check(ObjectTools::GetChildren(screen) == std::vector<lv_obj_t*>{loaded, arc},
          "Child traversal changed editor order");

    auto legacyLabel = legacy;
    legacyLabel.erase("arc");
    legacyLabel["text"] = "Status";
    legacyLabel["longmode"] = 2;
    legacyLabel["textalign"] = 1;
    legacyLabel["drawback"] = false;
    auto* label = Serialization::LVLabel::FromJSON(legacyLabel);
    Check(label && lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_DOTS &&
          lv_obj_get_style_text_align(label, LV_PART_MAIN) == LV_TEXT_ALIGN_CENTER,
          "Legacy label properties failed to restore");
    const auto savedLabel = Serialization::LVLabel::ToJSON(label);
    for (const auto* key : {"text", "longmode", "textalign", "drawback"})
        Check(savedLabel[key] == legacyLabel[key], "Legacy label properties changed after round trip");

    auto* container = Container::Create(screen, 150, 30);
    auto* bar = Bar::Create(container, 0, 0);
    auto* button = Button::Create(container, 0, 0);
    auto* matrix = ButtonMatrix::Create(container, 0, 0);
    auto* calendar = Calendar::Create(container, 0, 0);
    auto* canvas = Canvas::Create(container, 0, 0);
    auto* chart = Chart::Create(container, 0, 0);
    auto* checkbox = CheckBox::Create(container, 0, 0);
    Check(lv_bar_get_value(bar) == 50 &&
          lv_obj_check_type(button, &lv_button_class) &&
          lv_obj_check_type(matrix, &lv_buttonmatrix_class) &&
          lv_obj_check_type(calendar, &lv_calendar_class) &&
          lv_obj_check_type(canvas, &lv_canvas_class) &&
          lv_chart_get_point_count(chart) == 10 &&
          lv_obj_has_state(checkbox, LV_STATE_CHECKED),
          "Widget factories created unexpected widget types or state");

    auto* window = EditorWindow::Create(screen);
    lv_obj_set_pos(window, 400, 300);
    lv_obj_set_size(window, 230, 180);
    EditorWindow::SetTitle(window, "Drag test");
    auto* close = EditorWindow::AddButton(window, LV_SYMBOL_CLOSE);
    auto* header = EditorWindow::Header(window);
    Check(lv_obj_get_parent(close) == header &&
          lv_obj_get_parent(EditorWindow::Content(window)) == window, "Editor window structure is invalid");
    EditorDrag::Attach(header, window, RecordDrag);
    lv_obj_update_layout(window);
    lv_area_t bounds;
    lv_obj_get_coords(header, &bounds);
    auto* input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, ReadPointer);
    pointerSample.point = {bounds.x1 + 10, bounds.y1 + 8};
    pointerSample.state = LV_INDEV_STATE_PRESSED;
    lv_indev_read(input);
    pointerSample.point.x += 25;
    pointerSample.point.y += 15;
    lv_indev_read(input);
    pointerSample.state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(input);
    lv_obj_update_layout(window);
    Check(lv_obj_get_x(window) == 425 && lv_obj_get_y(window) == 315 &&
          completedDrag == window, "Editor window drag did not move or report completion");
    lv_indev_delete(input);

    SetEditorEventCallback<UnexpectedClick>(screen);
    SetEditorEventCallback<TrackClick>(screen);
    lv_obj_send_event(loaded, LV_EVENT_CLICKED, nullptr);
    Check(clicks == 1 && lastTarget == screen, "Replacement or bubbling of editor events failed");
    Check(simulator.ProcessEvents(), "SDL event loop stopped before close");
    lv_refr_now(nullptr);

    SDL_Event quit = {};
    quit.type = SDL_EVENT_QUIT;
    Check(SDL_PushEvent(&quit) == 1, "SDL quit event could not be queued");
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        if (!simulator.ProcessEvents())
            return EXIT_SUCCESS;
    }
    Check(false, "SDL quit did not stop the event loop");
}
