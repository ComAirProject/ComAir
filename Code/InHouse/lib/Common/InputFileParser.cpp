#include "Common/InputFileParser.h"

#include <cstdio>

namespace common {

    bool parseFuncListFile(const char *fileName, std::set<unsigned> &setFuncIDList) {

        FILE *fp = fopen(fileName, "r");
        if (!fp) {
            return false;
        }
        unsigned funcID = 0;

        while (fscanf(fp, "%u", &funcID) != EOF) {
            if (funcID == 0) {
                continue;
            }
            setFuncIDList.insert(funcID);
        }

        return true;
    }
}