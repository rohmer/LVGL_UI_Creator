#include "catch.hpp"
#include "../3rdParty/LVGL/lvgl/lvgl.h"
#include <Serialization/LVCalendar.h>

TEST_CASE("Serialize Calendar")
{
	lv_obj_t* cal = lv_calendar_create(lv_scr_act(), nullptr);
	const char* dayNames[7];
	dayNames[0] = "Sun";
	dayNames[1] = "Mon";
	dayNames[2] = "Tue";
	dayNames[3] = "Wed";
	dayNames[4] = "Thu";
	dayNames[5] = "Fri";
	dayNames[6] = "Sat";
	const char* monNames[12] = { "Jan", "Feb", "Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
	lv_calendar_set_day_names(cal, dayNames);
	lv_calendar_set_month_names(cal, monNames);
	json j = Serialization::LVCalendar::ToJSON(cal);

	REQUIRE(j["cal"]["dayN"][0] == "Sun");
	REQUIRE(j["cal"]["dayN"][6] == "Sat");
	REQUIRE(j["cal"]["monthN"][4] == "May");
	REQUIRE(j["cal"]["monthN"][10] == "Nov");
	lv_obj_del(cal);	
}

TEST_CASE("Deserialize Calendar")
{
	lv_obj_t* cal = lv_calendar_create(lv_scr_act(), nullptr);
	const char* dayNames[7];
	dayNames[0] = "Sun";
	dayNames[1] = "Mon";
	dayNames[2] = "Tue";
	dayNames[3] = "Wed";
	dayNames[4] = "Thu";
	dayNames[5] = "Fri";
	dayNames[6] = "Sat";
	const char* monNames[12] = { "Jan", "Feb", "Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
	lv_calendar_set_day_names(cal, dayNames);
	lv_calendar_set_month_names(cal, monNames);
	json j = Serialization::LVCalendar::ToJSON(cal);

	lv_obj_t* cal2 = Serialization::LVCalendar::FromJSON(j);
	const char** dayNames1 = lv_calendar_get_day_names(cal);
	const char** dayNames2 = lv_calendar_get_day_names(cal2);
	for(int i=0; i<7; i++)
	{
		REQUIRE(strcmp(dayNames1[i],dayNames2[i])==0);
		REQUIRE(strcmp(dayNames[i],dayNames1[i])==0);
	}
	const char** mn1 = lv_calendar_get_month_names(cal);
	const char** mn2 = lv_calendar_get_month_names(cal2);
	for (int i = 0; i < 12; i++)
	{
		REQUIRE(strcmp(mn1[i], mn2[i]) == 0);
		REQUIRE(strcmp(mn1[i], monNames[i]) == 0);
	}

}