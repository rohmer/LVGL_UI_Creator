#pragma once
#include <ctime>
#include <string>
#include <vector>

#include "../../3rdParty/JSON/json.hpp"
using json = nlohmann::json;

typedef enum
{
    VAR_STRING,
    VAR_INT,
    VAR_BOOL,
    VAR_FLOAT,
    VAR_DATE
} eVarDataType;


/**
 * \brief
 * The variable class abstracts away data for the UI.
 * Generally if you are planning on having static data, you will set it in the UI Edtior,
 * but in the case of dynamic data, use variables.
 * Example:
 * A chart getting data from the database.  You would define the series to point to a variable
 * and then that variable would override the variable base class and modify get/set data to
 * get data from the db.
 * \tparam T 
 */
template <typename T>
class Variable
{
public:
    Variable(std::string name,
             T defaultValue,
             uint16_t objectID,
             std::string propertyName,
             uint16_t history = 1);

    virtual T GetValue();
    virtual void SetValue(T value);
    virtual std::vector<T> GetValues();
    virtual void SetValues(std::vector<T> newValues);

    json ToJSON(bool storeValues);
    static Variable FromJSON(json j);
    void ClearValues();

private:
    uint16_t oid;
    std::string property;
    std::string varName;
    eVarDataType varDataType;
    T defaultValue;
    uint16_t history;
    std::vector<T> values;
};
