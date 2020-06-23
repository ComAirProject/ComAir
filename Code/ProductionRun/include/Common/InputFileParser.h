#ifndef PRODUCTIONRUN_INPUTFILEPARSER_H
#define PRODUCTIONRUN_INPUTFILEPARSER_H

#include <map>

bool parseCallSites(const char *fileName, std::map<unsigned, std::map<unsigned, unsigned>>& mapCallSites);

#endif //PRODUCTIONRUN_INPUTFILEPARSER_H
