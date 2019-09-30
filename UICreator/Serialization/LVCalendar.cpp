#include "LVCalendar.h"

namespace Serialization
{
	json LVCalendar::ToJSON(lv_obj_t* cal)
	{
		json j;
		j["base"] = LVObject::ToJSON(cal);
		const char** dayN = lv_calendar_get_day_names(cal);
		int ctr = 0;
		while (*dayN)
		{
			const char* p = *dayN;
			while (strlen(p) > 0)
			{
				j["cal"]["dayN"][ctr] = p;
				p++;
				ctr++;
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
		while (*monthN)
		{
			const char* p = *monthN;
			while (strlen(p) > 0)
			{
				j["cal"]["monthN"][ctr] = p;
				p++;
				ctr++;
			}
		}

		lv_style_t* styleBG = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_BG);
		json jBG = Serialization::Style::Serialize(*styleBG);
		j["cal"]["styleBG"]["body"] = jBG["body"];
		j["cal"]["styleBG"]["text"] = jBG["text"];

		lv_style_t* styleHead = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_HEADER);
		json jHead = Serialization::Style::Serialize(*styleHead);
		j["cal"]["styleHead"]["bodt"] = jHead["body"];
		j["cal"]["styleHead"]["text"] = jHead["text"];

		lv_style_t* styleHeadPR = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_HEADER_PR);
		json jHeadPR = Serialization::Style::Serialize(*styleHeadPR);
		j["cal"]["styleHeadPR"]["body"] = jHeadPR["body"];
		j["cal"]["styleHeadPR"]["text"] = jHeadPR["text"];

		lv_style_t* styleDayN = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_DAY_NAMES);
		json jDN = Serialization::Style::Serialize(*styleDayN);
		j["cal"]["styleDayName"]["body"] = jDN["body"];
		j["cal"]["styleDayName"]["text"] = jDN["text"];

		lv_style_t* styleHDay = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_HIGHLIGHTED_DAYS);
		json jHD = Serialization::Style::Serialize(*styleHDay);
		j["cal"]["styleHDay"]["text"] = jHD["text"];

		lv_style_t* styleIDay = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_INACTIVE_DAYS);
		json jID = Serialization::Style::Serialize(*styleIDay);
		j["cal"]["styleInaDay"]["text"] = jID["text"];

		lv_style_t* styleWeekBox = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_WEEK_BOX);
		json jWB = Serialization::Style::Serialize(*styleWeekBox);
		j["cal"]["styleWeekBox"]["body"] = jWB["body"];

		lv_style_t* styleTodayBox = (lv_style_t*)lv_calendar_get_style(cal, LV_CALENDAR_STYLE_TODAY_BOX);
		json jTB = Serialization::Style::Serialize(*styleTodayBox);
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
				std::vector<std::string> dayNames;
				for (json::iterator it = j["cal"]["dayN"].begin();
					it != j["cal"]["dayN"].end();
					++it)
				{
					dayNames.push_back(*it);
				}
				std::vector<const char*> cstrings;
				cstrings.reserve(dayNames.size());
				for (size_t i = 0; i < dayNames.size(); ++i)
					cstrings.push_back(const_cast<char*>(dayNames[i].c_str()));
				lv_calendar_set_day_names(cal, &cstrings[0]);
			}

			if (j["cal"]["hlDates"].is_array())
			{
				std::vector<lv_calendar_date_t> hlDates;
				int ctr = 0;;
				while (j["cal"]["hlDates"][ctr].is_object())
				{
					lv_calendar_date_t calDate;
					calDate.day = j["cal"]["hlDates"][ctr]["day"];
					calDate.month = j["cal"]["hlDates"][ctr]["month"];
					calDate.year = j["cal"]["hlDates"][ctr]["year"];
					hlDates.push_back(calDate);
				}
				if (!hlDates.empty())
				{
					lv_calendar_set_highlighted_dates(cal, &hlDates[0], hlDates.size());
				}
			}

			if (j["cal"]["monthN"].is_array())
			{
				std::vector<std::string> monthNames;
				int ctr = 0;
				while (j["cal"]["monthN"][ctr].is_object())
				{
					monthNames.push_back(j["cal"]["monthN"][ctr]);
				}
				std::vector<const char*> cstrings;
				cstrings.reserve(monthNames.size());
				for (size_t i = 0; i < monthNames.size(); ++i)
					cstrings.push_back(const_cast<char*>(monthNames[i].c_str()));

				lv_calendar_set_month_names(cal, &cstrings[0]);
			}

			if (j["cal"]["styleBG"].is_object())
			{
				static lv_style_t bgStyle=Style::Deserialize(j["cal"]["styleBG"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_BG, &bgStyle);
			}
			if (j["cal"]["styleHead"].is_object())
			{
				static lv_style_t headStyle = Style::Deserialize(j["cal"]["styleHead"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_HEADER, &headStyle);
			}
			if (j["cal"]["styleHeadPR"].is_object())
			{
				static lv_style_t headPRStyle = Style::Deserialize(j["cal"]["styleHeadPR"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_HEADER_PR, &headPRStyle);
			}
			if (j["cal"]["styleDayName"].is_object())
			{
				static lv_style_t dayNameStyle = Style::Deserialize(j["cal"]["styleDayName"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_DAY_NAMES, &dayNameStyle);
			}
			if (j["cal"]["styleHDay"].is_object())
			{
				static lv_style_t hDayStyle = Style::Deserialize(j["cal"]["styleHDay"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_HIGHLIGHTED_DAYS, &hDayStyle);
			}
			if (j["cal"]["styleInaDay"].is_object())
			{
				static lv_style_t inDay = Style::Deserialize(j["cal"]["styleInaDay"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_INACTIVE_DAYS, &inDay);
			}
			if (j["cal"]["styleWeekBox"].is_object())
			{
				static lv_style_t weekBox = Style::Deserialize(j["cal"]["styleWeekBox"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_WEEK_BOX, &weekBox);
			}
			if (j["cal"]["styleTodayBox"].is_object())
			{
				static lv_style_t todayBox = Style::Deserialize(j["cal"]["styleTodayBox"]);
				lv_calendar_set_style(cal, LV_CALENDAR_STYLE_WEEK_BOX, &todayBox);
			}
		}

		return cal;
	}
	
	bool LVCalendar::SetValue(lv_obj_t* obj, std::string key, std::vector<std::string> value)
	{
		if(key=="/cal/dayN")
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
		if(key=="/cal/hlDates")
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