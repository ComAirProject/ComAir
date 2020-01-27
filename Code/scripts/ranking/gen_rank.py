#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys

def gen_callgraph(callgraph_file):
    callgraph = {}
    for line in callgraph_file.readlines():
        fields = line.split(':')
        caller = int(fields[0])
        callees_strs = fields[1].split(',')
        callees = set(map(lambda item: int(item), callees_strs[:-1]))
        callgraph[caller] = callees
    return callgraph 


def gen_funcIdCost(funcIdCost_file):
    funcIdCost = {}
    for line in funcIdCost_file.readlines():
        fields = line.split(',')
        funcId = int(fields[0])
        cost = int(fields[1])
        funcIdCost[funcId] = cost
    return funcIdCost


def gen_most_complexity(comp_file):
    comp_99_func_ids = set()
    comp_2_func_ids = set()
    comp_1_func_ids = set()
    for line in comp_file.readlines():
        fields = line.split(',')
        try:
            func_id = int(fields[0])
        except:
            continue
        comp = int(fields[1])
        if comp == 99:
            comp_99_func_ids.add(func_id)
        if comp == 2:
            comp_2_func_ids.add(func_id)
        if comp == 1:
            comp_1_func_ids.add(func_id)
    if len(comp_99_func_ids) > 10:
        print("2^N")
        return comp_99_func_ids
    elif len(comp_2_func_ids) > 0:
        print("N^2")
        return comp_2_func_ids
    else:
        print("N")
        return comp_1_func_ids


def gen_full_callgraph(callgraph):
    full_callgraph = {}
    for caller, callees in callgraph.items():
        worklist = []
        visited = set()
        for callee in callees:
            worklist.append(callee)
        while len(worklist) > 0:
            curr = worklist.pop()
            if curr in full_callgraph:
                for curr_full_callee in full_callgraph[curr]:
                    visited.add(curr_full_callee)
                continue
            if curr not in callgraph:
                continue
            for curr_callee in callgraph[curr]:
                if curr_callee not in visited:
                    worklist.append(curr_callee)
                    visited.add(curr_callee)
        full_callgraph[caller] = visited
    return full_callgraph


def insert_sort(arr, full_callgraph):
    n = len(arr)
    new_arr = []
    new_arr.append(arr[0])

    for i in range(1, n):
        j = 0
        insert_flag = False
        while j < len(new_arr):
            if new_arr[j] in full_callgraph \
                and arr[i] in full_callgraph[new_arr[j]]:
                new_arr.insert(j, arr[i])
                insert_flag = True
                break
            j += 1
        if not insert_flag:
            new_arr.append(arr[i])
    return new_arr


def gen_rank(callgraph, funcIdCost, compFuncIds):
    compFuncIdCost = dict(filter(lambda elem: elem[0] in compFuncIds, funcIdCost.items()))
    compFuncIdSortedByCost = sorted(compFuncIdCost, key=compFuncIdCost.get, reverse=True)
    # for funcId in compFuncIdSortedByCost:
    #     print(funcId, compFuncIdCost[funcId])
    full_callgraph = gen_full_callgraph(callgraph)
    #full_callgraph = callgraph
    rank = insert_sort(compFuncIdSortedByCost, full_callgraph)
    for funcId in rank:
        print(funcId, compFuncIdCost[funcId])
    #lower_bound = compFuncIdCost[rank[1]] * 0.5
    #top_rank = []
    #split_idx = 0
    #for idx, funcId in enumerate(rank):
    #    if compFuncIdCost[funcId] < lower_bound:
    #        split_idx = idx
    #        break
    #    top_rank.append(funcId)

    ## print("top rank:")
    ## for funcId in top_rank:
    ##     print(funcId, compFuncIdCost[funcId])
    #new_top_rank = sorted(top_rank, key=compFuncIdCost.get)
    ## print("rank:")
    #for funcId in new_top_rank:
    #    print(funcId, compFuncIdCost[funcId])
    #for funcId in rank[split_idx:]:
    #    print(funcId, compFuncIdCost[funcId])

def main():
    complexity_file_path = sys.argv[1]
    with open("callgraph.log") as infile:
        callgraph = gen_callgraph(infile)
    with open("funcIdCost.log") as infile:
        funcIdCost = gen_funcIdCost(infile)
    with open(complexity_file_path) as infile:
        compFuncIds = gen_most_complexity(infile)
    gen_rank(callgraph, funcIdCost, compFuncIds)


if __name__ == "__main__":
    main()
    
