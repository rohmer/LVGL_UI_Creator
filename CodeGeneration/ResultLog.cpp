#include "ResultsLog.h"

ResultsLog::ResultsLog()
{
}

void ResultsLog::AddResultItem(eResultType resultType, std::string objName, std::string message)
{
    resultItems.push_back(ResultItem(resultType, objName, message));
}

std::vector<std::string> ResultsLog::GetStrings()
{
    std::vector<std::string> results;
    for (int i = 0; i < resultItems.size(); i++)
        results.push_back(resultItems[i].ToString());
    return results;
}