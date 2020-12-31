#ifndef PRODUCTIONRUN_INPUTFILEPARSER_H
#define PRODUCTIONRUN_INPUTFILEPARSER_H

#include <map>
#include "optional.h"

struct ParsedLoopInfo {
    bool isArray;
    int step;
    unsigned instID;
};

std::experimental::optional<ParsedLoopInfo> parseLoopInfo(const char *fileName);
bool parseCallSites(const char *fileName, std::map<unsigned, std::map<unsigned, unsigned>>& mapCallSites);

#endif //PRODUCTIONRUN_INPUTFILEPARSER_H
