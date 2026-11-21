#include "RuntimeFeatures.h"
#if !LVSERIAL_MINIMAL_RUNTIME || LV_USE_CALENDAR
#include "LVCalendar.h"
#if LVGL_VERSION_MAJOR >= 9 || defined(LV_RUNTIME_PORTABLE)
#include "WidgetRecords.h"
#include "Style.h"
#include <array>
#include <stdexcept>

namespace Serialization
{
    namespace
    {
        struct CalendarData
        {
            std::vector<std::string> days, months;
            std::array<const char*, 7> dayNames{};
            std::vector<lv_calendar_date_t> dates;
            lv_calendar_date_t emptyDate{};
            bool monthCallback = false;
        };

        lv_obj_t* Header(lv_obj_t* calendar)
        {
            for (uint32_t i = 0; i < lv_obj_get_child_count(calendar); ++i)
            {
                auto* child = lv_obj_get_child(calendar, i);
                if (lv_obj_get_class(child) == &lv_calendar_header_arrow_class) return child;
            }
            return nullptr;
        }

        void UpdateMonthTitle(lv_obj_t* calendar)
        {
            const auto& state = WidgetData<CalendarData>(calendar);
            auto* header = Header(calendar);
            const auto* shown = lv_calendar_get_showed_date(calendar);
            if (header && state.months.size() == 12 && shown->month >= 1 && shown->month <= 12)
                lv_label_set_text_fmt(lv_obj_get_child(header, 1), "%u %s", shown->year, state.months[shown->month - 1].c_str());
        }

        std::pair<lv_obj_t*, lv_style_selector_t> StyleTarget(lv_obj_t* object, const std::string& key)
        {
            auto* dates = lv_calendar_get_btnmatrix(object);
            auto* header = Header(object);
            if (key == "styleBG") return {object, LV_PART_MAIN};
            if (key == "styleHead") return {header, LV_PART_MAIN};
            if (key == "styleHeadPR") return {header, LV_PART_MAIN | LV_STATE_PRESSED};
            if (key == "styleDayName") return {dates, LV_PART_ITEMS};
            if (key == "styleHDay") return {dates, LV_PART_ITEMS | LV_STATE_CHECKED};
            if (key == "styleInaDay") return {dates, LV_PART_ITEMS | LV_STATE_DISABLED};
            if (key == "weekBox") return {dates, LV_PART_MAIN};
            if (key == "todayBox") return {dates, LV_PART_ITEMS | LV_STATE_FOCUSED};
            return {nullptr, LV_PART_MAIN};
        }
    }

    json LVCalendar::ToJSON(lv_obj_t* object)
    {
        auto record = ReadSection(object, "cal");
        const auto& state = WidgetData<CalendarData>(object);
        record["dayN"] = state.days.empty() ? std::vector<std::string>{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"} : state.days;
        record["monthN"] = state.months.empty() ? std::vector<std::string>{"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"} : state.months;
        record["hlDates"] = json::array();
        auto* dates = lv_calendar_get_highlighted_dates(object);
        for (std::size_t i = 0; dates && i < lv_calendar_get_highlighted_dates_num(object); ++i)
            record["hlDates"].push_back({{"year", dates[i].year}, {"month", dates[i].month}, {"day", dates[i].day}});
        const auto* shown = lv_calendar_get_showed_date(object);
        record["shown"] = {{"year", shown->year}, {"month", shown->month}};
        const auto* today = lv_calendar_get_today_date(object);
        record["today"] = {{"year", today->year}, {"month", today->month}, {"day", today->day}};
        for (const auto* name : {"styleBG", "styleHead", "styleHeadPR", "styleDayName", "styleHDay", "styleInaDay", "weekBox", "todayBox"})
        {
            const auto target = StyleTarget(object, name);
            if (target.first) record[name] = Style::Read(target.first, static_cast<lv_part_t>(target.second & LV_PART_ANY), record[name]);
        }
        return {{"base", LVObject::ToJSON(object)}, {"cal", record}};
    }

    lv_obj_t* LVCalendar::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;
        auto* object = LVObject::FromJSON(j["base"], lv_calendar_create(lv_screen_active()));
        lv_calendar_add_header_arrow(object);
        auto record = j["cal"];
        if (!record.is_object())
            return object;
        Apply(object, record, !HasNativeStyleSnapshot(j["base"]));
        return object;
    }

    void LVCalendar::Apply(lv_obj_t* object, json record, bool applyStyles)
    {
        const auto validateDate = [](const json& date) {
            const int year = date.value("year", 2026), month = date.value("month", 1), day = date.value("day", 1);
            if (year < 1 || year > 65535 || month < 1 || month > 12 || day < 1 || day > 31)
                throw std::invalid_argument("Use a valid year, month (1-12), and day (1-31)");
        };
        for (const auto* key : {"today", "shown"}) if (record.contains(key)) validateDate(record.at(key));
        if (record.contains("hlDates")) for (const auto& date : record.at("hlDates")) validateDate(date);
        PreserveSection(object, "cal", record);
        if (record.contains("today") && record["today"].is_object())
            lv_calendar_set_today_date(object, record["today"].value("year", 2026), record["today"].value("month", 1), record["today"].value("day", 1));
        if (record["dayN"].is_array())
            SetValue(object, "/cal/dayN", record["dayN"].get<std::vector<std::string>>());
        if (record["monthN"].is_array())
            SetValue(object, "/cal/monthN", record["monthN"].get<std::vector<std::string>>());
        std::vector<lv_calendar_date_t> dates;
        if (record["hlDates"].is_array())
            for (const auto& date : record["hlDates"])
                if (date.is_object() && date.contains("year") && date.contains("month") && date.contains("day")) {
                    lv_calendar_date_t value{};
                    value.year = date["year"].get<int>(); value.month = date["month"].get<int>(); value.day = date["day"].get<int>();
                    dates.push_back(value); }
        SetValue(object, "/cal/hlDates", std::move(dates));
        if (record["shown"].is_object())
            lv_calendar_set_month_shown(object, record["shown"].value("year", 2020), record["shown"].value("month", 1));
        for (const auto* name : {"styleBG", "styleHead", "styleHeadPR", "styleDayName",
                                 "styleHDay", "styleInaDay", "weekBox", "todayBox"})
        {
            const auto target = StyleTarget(object, name);
            // Internal calendar children have their own legacy style records;
            // only the calendar object's styles are covered by base.nativeStyles.
            if (target.first && (applyStyles || target.first != object))
                Style::Apply(target.first, record[name], target.second);
        }
        UpdateMonthTitle(object);
    }

    bool LVCalendar::SetValue(lv_obj_t* object, std::string key, std::vector<std::string> names)
    {
        auto& state = WidgetData<CalendarData>(object);
        if (key == "/cal/dayN" && names.size() == 7)
        {
            state.days = std::move(names);
            for (std::size_t i = 0; i < 7; ++i)
                state.dayNames[i] = state.days[i].c_str();
            lv_calendar_set_day_names(object, state.dayNames.data());
            return true;
        }
        if (key == "/cal/monthN" && names.size() == 12)
        {
            state.months = std::move(names);
            if (!state.monthCallback)
            {
                lv_obj_add_event_cb(object, [](lv_event_t* event) { UpdateMonthTitle(lv_event_get_target_obj(event)); }, LV_EVENT_VALUE_CHANGED, nullptr);
                state.monthCallback = true;
            }
            UpdateMonthTitle(object);
            return true;
        }
        return false;
    }

    bool LVCalendar::SetValue(lv_obj_t* object, std::string key, lv_style_t* style)
    {
        if (!style || key.compare(0, 5, "/cal/") != 0)
            return false;
        const auto target = StyleTarget(object, key.substr(5));
        if (!target.first)
            return false;
        lv_obj_add_style(target.first, style, target.second);
        return true;
    }

    bool LVCalendar::SetValue(lv_obj_t* object, std::string key, std::vector<lv_calendar_date_t> dates)
    {
        if (key != "/cal/hlDates")
            return false;
        auto& state = WidgetData<CalendarData>(object);
        state.dates = std::move(dates);
        lv_calendar_set_highlighted_dates(object, state.dates.empty() ? &state.emptyDate : state.dates.data(), state.dates.size());
        return true;
    }

    bool LVCalendar::SetValue(lv_obj_t*, std::string, int) { return false; }
}
#else
static std::vector<const char*> dstrings;
static std::vector<const char*> mstrings;

namespace Serialization
{
    json LVCalendar::ToJSON(lv_obj_t* cal)
    {
        json j;
        j["base"] = LVObject::ToJSON(cal);
        const char** dayN = lv_calendar_get_day_names(cal);
        int ctr = 0;
        if (dayN != nullptr)
        {
            const char** ma = dayN;
            int col = 0;
            while (ctr < 7)
            {
                const char* p = *ma;
                if (p != nullptr)
                    while (strlen(p) > 0)
                    {
                        j["cal"]["dayN"][ctr] = p;
                        p += strlen(p);
                        ctr++;
                    }
                ma++;
            }
        }
        uint16_t hDateNum = lv_calendar_get_highlighted_dates_num(cal);
        lv_calendar_date_t* hDates = lv_calendar_get_highlighted_dates(cal);
        for (int i = 0; i < hDateNum; i++)
        {
            j["cal"]["hlDates"][i]["day"] = hDates[i].day;
            j["cal"]["hlDates"][i]["month"] = hDates[i].month;
            j["cal"]["hlDates"][i]["year"] = hDates[i].year;
        }
        const char** monthN = lv_calendar_get_month_names(cal);
        ctr = 0;
        if (monthN != nullptr)
        {
            const char** ma = monthN;
            int col = 0;
            while (ctr < 12)
            {
                const char* p = *ma;
                if (p != nullptr)
                    while (strlen(p) > 0)
                    {
                        j["cal"]["monthN"][ctr] = p;
                        p += strlen(p);
                        ctr++;
                    }
                ma++;
            }
        }


        lv_style_t* styleBG = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_BG);
        json jBG = Style::ToJSON(*styleBG);
        j["cal"]["styleBG"]["body"] = jBG["body"];
        j["cal"]["styleBG"]["text"] = jBG["text"];

        lv_style_t* styleHead = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_HEADER);
        json jHead = Style::ToJSON(*styleHead);
        j["cal"]["styleHead"]["bodt"] = jHead["body"];
        j["cal"]["styleHead"]["text"] = jHead["text"];

        lv_style_t* styleHeadPR = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_HEADER_PR);
        json jHeadPR = Style::ToJSON(*styleHeadPR);
        j["cal"]["styleHeadPR"]["body"] = jHeadPR["body"];
        j["cal"]["styleHeadPR"]["text"] = jHeadPR["text"];

        lv_style_t* styleDayN = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_DAY_NAMES);
        json jDN = Style::ToJSON(*styleDayN);
        j["cal"]["styleDayName"]["body"] = jDN["body"];
        j["cal"]["styleDayName"]["text"] = jDN["text"];

        lv_style_t* styleHDay = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_HIGHLIGHTED_DAYS);
        json jHD = Style::ToJSON(*styleHDay);
        j["cal"]["styleHDay"]["text"] = jHD["text"];

        lv_style_t* styleIDay = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_INACTIVE_DAYS);
        json jID = Style::ToJSON(*styleIDay);
        j["cal"]["styleInaDay"]["text"] = jID["text"];

        lv_style_t* styleWeekBox = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_WEEK_BOX);
        json jWB = Style::ToJSON(*styleWeekBox);
        j["cal"]["styleWeekBox"]["body"] = jWB["body"];

        lv_style_t* styleTodayBox = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_TODAY_BOX);
        json jTB = Style::ToJSON(*styleTodayBox);
        j["cal"]["styleTodayBox"]["body"] = jTB["body"];
        j["cal"]["styleTodayBox"]["text"] = jTB["text"];

        return j;
    }


    lv_obj_t* LVCalendar::FromJSON(json j)
    {
        if (!j["base"].is_object())
            return nullptr;

        lv_obj_t* cal = lv_calendar_create(lv_scr_act(), nullptr);
        cal = LVObject::FromJSON(j["base"], cal);

        if (j["cal"].is_object())
        {
            if (j["cal"]["dayN"].is_array())
            {
                static std::vector<std::string> dayNames;
                for (json::iterator it = j["cal"]["dayN"].begin();
                     it != j["cal"]["dayN"].end();
                     ++it)
                {
                    dayNames.push_back(*it);
                }
                lv_calendar_ext_t* ext = static_cast<lv_calendar_ext_t*>(lv_obj_get_ext_attr(cal));
                if (ext->day_names == nullptr)
                    ext->day_names = new const char* [7];
                for (int i = 0; i < 7; i++)
                    ext->day_names[i] = dayNames[i].c_str();
                /*for (size_t i = 0; i < dayNames.size(); ++i)
                    ext->day_names[0]=dayNames[i].c_str();				*/
            }

            if (j["cal"]["hlDates"].is_array())
            {
                static std::vector<lv_calendar_date_t> hlDates;
                int ctr = 0;
                while (j["cal"]["hlDates"][ctr].is_object())
                {
                    lv_calendar_date_t calDate;
                    calDate.day = j["cal"]["hlDates"][ctr]["day"];
                    calDate.month = j["cal"]["hlDates"][ctr]["month"];
                    calDate.year = j["cal"]["hlDates"][ctr]["year"];
                    hlDates.push_back(calDate);
                    ctr++;
                }
                if (!hlDates.empty())
                {
                    lv_calendar_set_highlighted_dates(cal, &hlDates[0], hlDates.size());
                }
            }

            if (j["cal"]["monthN"].is_array())
            {
                static std::vector<std::string> monthNames;
                int ctr = 0;
                while (j["cal"]["monthN"][ctr].is_string())
                {
                    monthNames.push_back(j["cal"]["monthN"][ctr]);
                    ctr++;
                }

                if (!monthNames.empty())
                {
                    lv_calendar_ext_t* ext = static_cast<lv_calendar_ext_t*>(lv_obj_get_ext_attr(cal));
                    if (ext->month_names == nullptr)
                        ext->month_names = new const char* [7];

                    for (size_t i = 0; i < monthNames.size(); ++i)
                        ext->month_names[i] = monthNames[i].c_str();
                }
            }

            if (j["cal"]["styleBG"].is_object())
            {
                static lv_style_t bgStyle = Style::FromJSON(j["cal"]["styleBG"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_BG, &bgStyle);
            }
            if (j["cal"]["styleHead"].is_object())
            {
                static lv_style_t headStyle = Style::FromJSON(j["cal"]["styleHead"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_HEADER, &headStyle);
            }
            if (j["cal"]["styleHeadPR"].is_object())
            {
                static lv_style_t headPRStyle = Style::FromJSON(j["cal"]["styleHeadPR"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_HEADER_PR, &headPRStyle);
            }
            if (j["cal"]["styleDayName"].is_object())
            {
                static lv_style_t dayNameStyle = Style::FromJSON(j["cal"]["styleDayName"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_DAY_NAMES, &dayNameStyle);
            }
            if (j["cal"]["styleHDay"].is_object())
            {
                static lv_style_t hDayStyle = Style::FromJSON(j["cal"]["styleHDay"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_HIGHLIGHTED_DAYS, &hDayStyle);
            }
            if (j["cal"]["styleInaDay"].is_object())
            {
                static lv_style_t inDay = Style::FromJSON(j["cal"]["styleInaDay"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_INACTIVE_DAYS, &inDay);
            }
            if (j["cal"]["styleWeekBox"].is_object())
            {
                static lv_style_t weekBox = Style::FromJSON(j["cal"]["styleWeekBox"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_WEEK_BOX, &weekBox);
            }
            if (j["cal"]["styleTodayBox"].is_object())
            {
                static lv_style_t todayBox = Style::FromJSON(j["cal"]["styleTodayBox"]);
                lv_calendar_set_style(cal, LV_CALENDAR_STYLE_WEEK_BOX, &todayBox);
            }
        }

        return cal;
    }

    bool LVCalendar::SetValue(lv_obj_t* obj, std::string key, std::vector<std::string> value)
    {
        if (key == "/cal/dayN")
        {
            std::vector<const char*> dayNames;
            int i = 0;
            for (int i = 0; i < value.size(); i++)
                dayNames.push_back(value[i].c_str());
            lv_calendar_set_day_names(obj, &dayNames[0]);
            return true;
        }
        if (key == "/cal/monthN")
        {
            std::vector<const char*> monthNames;
            int i = 0;
            for (int i = 0; i < value.size(); i++)
                monthNames.push_back(value[i].c_str());
            lv_calendar_set_day_names(obj, &monthNames[0]);
            return true;
        }

        return false;
    }

    bool LVCalendar::SetValue(lv_obj_t* obj, std::string key, lv_style_t* style)
    {
        if (key == "/cal/styleBG")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_BG, style);
            return true;
        }
        if (key == "/cal/styleBG")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_BG, style);
            return true;
        }
        if (key == "/cal/styleHead")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_HEADER, style);
            return true;
        }
        if (key == "/cal/styleHeadPR")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_HEADER_PR, style);
            return true;
        }
        if (key == "/cal/styleDayName")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_DAY_NAMES, style);
            return true;
        }
        if (key == "/cal/styleHDay")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_HIGHLIGHTED_DAYS, style);
            return true;
        }
        if (key == "/cal/styleInaDay")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_INACTIVE_DAYS, style);
            return true;
        }
        if (key == "/cal/styleWeekBox")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_WEEK_BOX, style);
            return true;
        }
        if (key == "/cal/styleTodayBox")
        {
            lv_calendar_set_style(obj, LV_CALENDAR_STYLE_TODAY_BOX, style);
            return true;
        }

        return false;
    }

    bool LVCalendar::SetValue(lv_obj_t* obj, std::string key, std::vector<lv_calendar_date_t> value)
    {
        if (key == "/cal/hlDates")
        {
            lv_calendar_set_highlighted_dates(obj, &value[0], value.size());
            return true;
        }
        return false;
    }

    bool LVCalendar::SetValue(lv_obj_t* obj, std::string key, int value)
    {
        return false;
    }
}
#endif

#endif // selected widget
