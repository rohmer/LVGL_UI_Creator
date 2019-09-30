#pragma once

#include <iterator>
#include <string>
#include <sstream>
#include <vector>

#include <lvgl/lvgl.h>
#include "../../3rdParty/JSON/json.hpp"


class ObjectTools
{
public:
	static std::vector<lv_obj_t*> GetChildren(lv_obj_t* object);
	
	static std::vector<std::string> Split(const std::string& s, char delim) {
		std::vector<std::string> elems;
		Split(s, delim, std::back_inserter(elems));
		return elems;
	}

private:
	template<typename Out>
	static void Split(const std::string& s, char delim, Out result) {
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim)) {
			*(result++) = item;
		}
	}
};