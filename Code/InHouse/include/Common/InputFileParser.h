#ifndef INHOUSE_INPUTFILEPARSER_H
#define INHOUSE_INPUTFILEPARSER_H

#include <set>

namespace common {

    bool parseFuncListFile(const char *fileName, std::set<unsigned> &setFuncIDList);
}

#endif //INHOUSE_INPUTFILEPARSER_H
