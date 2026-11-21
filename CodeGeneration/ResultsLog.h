#pragma once

#include <string>
#include <vector>

#include "ResultItem.h"

class ResultsLog
{
public:
    ResultsLog();

    void AddResultItem(eResultType resultType, std::string objName, std::string message);

    std::vector<std::string> GetStrings();


private:
    std::vector<ResultItem> resultItems;
};
