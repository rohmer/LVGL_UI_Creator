#include "Variable.h"
#include <utility>

template <>
Variable<std::string>::Variable(std::string name,
	std::string defaultValue,
	uint16_t objectID,
	std::string propertyName,
	uint16_t history) :
	varName(std::move(name)),
	varDataType(eVarDataType::VAR_STRING),
	defaultValue(std::move(defaultValue)),
	oid(objectID),
	property(propertyName),
	history(history)
{
}

template <>
Variable<int>::Variable(std::string name,
	int defaultValue,
	uint16_t objectID,
	std::string propertyName,
	uint16_t history) :
	varName(std::move(name)),
	varDataType(eVarDataType::VAR_INT),
	defaultValue(defaultValue),
	oid(objectID),
	property(propertyName),
	history(history)
{
}

template <>
Variable<bool>::Variable(std::string name,
	bool defaultValue,
	uint16_t objectID,
	std::string propertyName,
	uint16_t history) :
	varName(std::move(name)),
	varDataType(eVarDataType::VAR_BOOL),
	defaultValue(defaultValue),
	oid(objectID),
	property(propertyName),
	history(history)
{
}

template <>
Variable<float>::Variable(std::string name,
	float defaultValue,
	uint16_t objectID,
	std::string propertyName,
	uint16_t history) :
	varName(std::move(name)),
	varDataType(eVarDataType::VAR_FLOAT),
	defaultValue(defaultValue),
	oid(objectID),
	property(propertyName),
	history(history)
{
}

template <>
Variable<std::time_t>::Variable(std::string name,
	std::time_t defaultValue,
	uint16_t objectID,
	std::string propertyName,
	uint16_t history) :
	varName(std::move(name)),
	varDataType(eVarDataType::VAR_DATE),
	defaultValue(defaultValue),
	oid(objectID),
	property(propertyName),
	history(history)
{
}

template <typename T>
Variable<T> Variable<T>::FromJSON(json j)
{
	std::string name=j["var"]["name"];
	eVarDataType dt=j["var"]["dataType"];
	uint16_t history=j["var"]["history"];
	uint16_t oid = j["var"]["oid"];
	std::string prop = j["var"]["property"];
	T dVal = j["var"]["default"];
	std::vector<T> hist;
	if(j["var"]["value"].is_array())
	{
		for(int i=0; i<j["var"]["value"].size(); i++)
		{
			hist.push_back(j["var"]["value"][i]);
		}
	}
	Variable v(name, dVal, oid, prop, history);
	v.SetValues(hist);
	return v;
}

template <typename T>
T Variable<T>::GetValue()
{
	if(values.empty())
	{
		return defaultValue;
	}
	return values[values.size() - 1];
}

template <typename T>
std::vector<T> Variable<T>::GetValues()
{
	return values;
}

template <typename T>
void Variable<T>::SetValue(T value)
{
	if (history != 0)
	{
		while (values.size() >= history-1)
			values.erase(values.begin());
	}
	values.push_back(value);
}

template <typename T>
void Variable<T>::SetValues(std::vector<T> newValues)
{
	values.clear();
	if(newValues.size()>history)
	{
		while (newValues.size() >= history - 1)
			newValues.erase(newValues.begin());
	}
	for (int i = 0; i < newValues.size(); i++)
		values.push_back(newValues[i]);
}

template <typename T>
void Variable<T>::ClearValues()
{
	values.clear();
}

template <typename T>
json Variable<T>::ToJSON(bool storeValues)
{
	json j;
	j["var"]["name"] = varName;
	j["var"]["dataType"] = varDataType;
	j["var"]["default"] = defaultValue;
	j["var"]["history"] = history;
	j["var"]["oid"] = oid;
	j["var"]["property"] = property;
	
	if(storeValues)
	{
		for(int i=0; i<values.size(); i++)
		{
			j["var"]["value"][i] = values[i];
			i++;
		}
	}
	return j;
}
