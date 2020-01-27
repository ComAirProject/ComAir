#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <map>
#include <set>

#define BUFFERSIZE 1UL << 38
#define APROF_MEM_LOG "/media/boqin/New Volume/aprof_log.log"

using FuncIdTy = unsigned;

struct Node {
    FuncIdTy funcId;
    unsigned ts;
    long rms;
    unsigned long cost;
};

std::vector<FuncIdTy> stack;

std::map<FuncIdTy, std::set<FuncIdTy>> callgraph;
std::map<FuncIdTy, unsigned long> funcIdCost;

void gen_callgraph(Node &x, Node &y) {
    if (x.ts > y.ts) {
        // y calls x and nodes in stack
        callgraph[y.funcId].insert(x.funcId);
        for (auto funcId : stack) {
            callgraph[y.funcId].insert(funcId);
        }
        stack.clear();
    } else {
        // push nodes in stack
        stack.push_back(x.funcId);
    }
}

void gen_funcIdCost(Node &x) {
    if (funcIdCost.find(x.funcId) == funcIdCost.end()) {
        funcIdCost[x.funcId] = x.cost;
    } else {
        if (funcIdCost[x.funcId] < x.cost) {
            funcIdCost[x.funcId] = x.cost;
        }
    }
}

void output_callgraph() {
    FILE *f = fopen("callgraph.log", "w");
    assert(f);
    for (auto &callerCallees : callgraph) {
        fprintf(f, "%u:", callerCallees.first);
        for (auto &callee : callerCallees.second) {
            fprintf(f, "%u,", callee);
        }
        fprintf(f, "\n");
    }
}

void output_funcIdCost() {
    FILE *f = fopen("funcIdCost.log", "w");
    assert(f);
    for (auto &kv : funcIdCost) {
        fprintf(f, "%u,%lu\n", kv.first, kv.second);
    }
}

int main() {
    int fd = open(APROF_MEM_LOG, O_RDWR | O_CREAT, 0777);

    if (fd < 0) {
        fd = open(APROF_MEM_LOG, O_RDWR, 0777);
    }

    assert(fd >= 0);

    assert(ftruncate(fd, BUFFERSIZE) == 0);

    char *ptr = (char *)mmap(NULL, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);

    struct Node x;
    struct Node y;
    std::size_t nodeSize = sizeof(Node);

    puts("start reading data....");
    memcpy(&x, ptr, nodeSize);
    if (x.funcId <= 0) {
        return 0;
    }
    gen_funcIdCost(x);

    ptr += nodeSize;
    memcpy(&y, ptr, nodeSize);

    while (y.funcId > 0) {
        gen_funcIdCost(y);
        gen_callgraph(x, y);

        x = y;
        ptr += nodeSize;
        memcpy(&y, ptr, nodeSize);
    }

    puts("read over");
    close(fd);    

    puts("output callgraph");
    output_callgraph();

    puts("output funcIdCost");
    output_funcIdCost();

    return 0;
}