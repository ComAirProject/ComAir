#include "Common/InputFileParser.h"

#include <cstdio>
#include <fstream>
#include <string>
#include "optional.h"

/// isArray
/// i32 1
/// 413
///  %4 = load i32, i32* %arrayidx3, align 4, !dbg !109, !bb_id !106, !ins_id !112
///
/// isLinkedList
/// 1656
///  %3 = load %struct.stMapEntry*, %struct.stMapEntry** %next, align 8, !dbg !110, !bb_id !112, !ins_id !114
std::experimental::optional<ParsedLoopInfo> parseLoopInfo(const char *fileName) {
    std::fstream fs;
    fs.open(fileName, std::fstream::in);
    std::string loopType;
    fs >> loopType;
    if (loopType == "isArray") {
        std::string _stepType;
        int step = 0;
        unsigned instID = 0;
        fs >> _stepType >> step;
        fs >> instID;
        return ParsedLoopInfo { true, step, instID };
    } else if (loopType == "isLinkedList") {
        unsigned instID = 0;
        fs >> instID;
        return ParsedLoopInfo { false, 0, instID };
    } else {
        return {};
    }
}

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

