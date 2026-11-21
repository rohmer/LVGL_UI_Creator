#pragma once
#include <lvgl/lvgl.h>
#include <lv_advanced_keyboard_prediction.h>
#include <string>
#include <vector>
class PropertyWindow;
namespace ModelPlayground {
struct Evaluation {
    std::string prefix, previous, older, correction;
    std::vector<std::string> suggestions;
    double confidence = 0;
    bool accepted = false;
};
Evaluation Evaluate(lv_obj_t* textarea, lv_advanced_keyboard_predictor_t* predictor, double threshold);
void Open(PropertyWindow* properties);
void Close();
void Shutdown();
lv_obj_t* ActiveWindow();
}
