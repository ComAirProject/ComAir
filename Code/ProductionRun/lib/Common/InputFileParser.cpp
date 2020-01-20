#include "Common/InputFileParser.h"

#include <cstdio>

bool parseCallSites(const char *fileName, std::map<unsigned, std::map<unsigned, unsigned>>& mapCallSites) {
    FILE *fp = fopen(fileName, "r");
    if (!fp) {
        return false;
    }
    unsigned caller = 0;
    unsigned callInst = 0;
    unsigned callee = 0;

    while (fscanf(fp, "%u,%u,%u\n", &caller, &callInst, &callee) != EOF) {
        if (mapCallSites.find(caller) == mapCallSites.end()) {
            mapCallSites[caller] = std::map<unsigned, unsigned>();
        }
        if (mapCallSites[caller].find(callInst) == mapCallSites[caller].end()) {
            mapCallSites[caller][callInst] = callee;
        } else {
            fprintf(stderr, "Orig: %u,%u,%u\n", caller, callInst, mapCallSites[caller][callInst]);
            fprintf(stderr, "Dup: %u,%u,%u\n", caller, callInst, callee);
            return false;
        }
    }
//        for (auto &kv : mapCallSites) {
//            for (auto &kv2 : kv.second) {
//                errs() << kv.first << "," << kv2.first << "," << kv2.second << "\n";
//            }
//        }
    return true;
}

