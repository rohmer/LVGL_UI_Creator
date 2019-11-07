#pragma once
#include <sstream>
#include <string>

enum eResultType
{
    SUCCESS,
    WARN,
    ERROR
};

struct ResultItem
{
public:
    ResultItem(eResultType type, std::string objName, std::string message) :
        ResultType(type),
        ObjectName(objName),
        Message(message)
    {
    }

    std::string ToString()
    {
        std::stringstream ss;
        switch (ResultType)
        {
        case SUCCESS:
            ss << "SUCCESS: ";
            break;
        case WARN:
            ss << "WARN: ";
            break;
        case ERROR:
            ss << "ERROR: ";
            break;
        }
        ss << "Object: " << ObjectName << ", " << Message;
        return ss.str();
    }

    eResultType ResultType;
    std::string ObjectName, Message;


};
