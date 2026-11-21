#pragma once
#include <Serialization/Variable.h>
using json = nlohmann::json;
using Serialization::eVarDataType;
using Serialization::VAR_STRING;
using Serialization::VAR_INT;
using Serialization::VAR_BOOL;
using Serialization::VAR_FLOAT;
using Serialization::VAR_DATE;
template <typename T> using Variable = Serialization::Variable<T>;
