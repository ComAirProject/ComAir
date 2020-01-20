#ifndef NEWCOMAIR_ARRAYDUMP_PARSERECORD_H
#define NEWCOMAIR_ARRAYDUMP_PARSERECORD_H

#include <unordered_map>

enum OneLoopRecordFlag : unsigned {
    Uninitialized = 0,
    FirstLoad,
    FirstStore
};

struct struct_stMemRecord {
    unsigned long address;
    unsigned length;
    int id;
};

typedef std::unordered_map<unsigned long, OneLoopRecordFlag> OneLoopRecordTy;

void parseRecord(char *pcBuffer, unsigned stride);

#endif //NEWCOMAIR_ARRAYDUMP_PARSERECORD_H
