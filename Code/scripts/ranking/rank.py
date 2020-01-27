#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys

complexity_path = sys.argv[1]
mem_head_path = sys.argv[2]

class Node: 
    def __init__(self, func_id, count):
        self.func_id = func_id
        self.count = count

    def __str__(self):
        return "{},{}".format(self.func_id, self.count)

    def __repr__(self):
        return "{},{}".format(self.func_id, self.count)


def insert_set(my_dict, key, value):
    if key not in my_dict:
        my_dict[key] = set()
    my_dict[key].add(value)


def insert_larger(my_dict, key, value):
    if key in my_dict:
        if my_dict[key] < value:
            my_dict[key] = value
    else:
        my_dict[key] = value


def parse_mem_result_head(lines, comp_func_ids):
    funcid_costs = {}
    funcid_counts = []

    for line in lines:
        fields = line.split(',')
        func_id = int(fields[0])
        cost = int(fields[2])
        count = int(fields[3])
        insert_larger(funcid_costs, func_id, cost)
        funcid_counts.append(Node(func_id, count))

    funcid_counts.reverse()

    stack = [funcid_counts[0]]
    # {caller: set(callees)}
    callgraph = {}

    funcid_counts.append(Node(0, -1))

    for node in funcid_counts[1:]:
        if len(stack) == 0:
            break
        while node.count <= stack[-1].count:
            callee = stack.pop()
            if len(stack) == 0:
                 break
            caller_id = stack[-1].func_id
            insert_set(callgraph, caller_id, callee.func_id)

#         if len(stack) == 0:
#             break
        stack.append(node)

    #callgraph = filter_callgraph_complex(callgraph)
    return funcid_costs, callgraph


def parse_complexity():
    comp_func_ids_99 = set()
    comp_func_ids_2 = set()
    comp_func_ids_1 = set()
    comp_func_ids_0 = set()
    with open(complexity_path) as infile:
        lines = infile.readlines()
        for line in lines:
            fields = line.split(",")
            func_id = int(fields[0])
            comp = int(fields[1])
            if comp == 99:
                comp_func_ids_99.add(func_id)
            elif comp == 2:
                comp_func_ids_2.add(func_id)
            elif comp == 1:
                comp_func_ids_1.add(func_id)
            else:
                comp_func_ids_0.add(func_id)
        if len(comp_func_ids_99) > 0:
            print("2^N")
            return comp_func_ids_99
        #elif len(comp_func_ids_2) > 0:
        #    print("N^2")
        #    return comp_func_ids_2
        #elif len(comp_func_ids_1) > 0:
        #    print("N")
        #    return comp_func_ids_1
        else:
            print("0")
            assert(len(comp_func_ids_0) > 0)
            return comp_func_ids_0


def dfs(graph, start):
    visited = set()
    stack = [start]
    while stack:
        vertex = stack.pop()
        if vertex not in visited:
            visited.add(vertex)
            if vertex in graph:
                stack.extend(graph[vertex] - visited)
    return visited - {start}


def cmp_call(x, y, fullgraph):

    if x in fullgraph:
        if y in fullgraph[x]:
            return -1

    if y in fullgraph:
        if x in fullgraph[y]:
            return 1

    return 0


funcid_costs = {}
callgraph = {}
comp_func_ids = parse_complexity()

lines = []

with open(mem_head_path) as infile:
    _ = infile.readline() # remove header
    lines = infile.readlines()


funcid_costs, callgraph = parse_mem_result_head(lines, comp_func_ids)


fullgraph = {}
func_ids = set()
for k,v in callgraph.items():
    fullgraph[k] = dfs(callgraph, k)
    func_ids.add(k)
    for i in v:
        func_ids.add(i)


func_ids_complex = set()
assert(len(comp_func_ids) > 0)
print(func_ids)
for i in func_ids:
    if i in comp_func_ids:
        func_ids_complex.add(i)

print("len of func_ids_complex:" + str(len(func_ids_complex)))


fullgraph_complex = {}
for k, v in fullgraph.items():
    if k in comp_func_ids:
        fullgraph_complex[k] = set()
        for i in v:
            if i in comp_func_ids:
                fullgraph_complex[k].add(i)


def filter_callgraph_complex(callgraph):
    fullgraph_complex = {}
    for k, v in fullgraph.items():
        if k in comp_func_ids:
            fullgraph_complex[k] = set()
            for i in v:
                if i in comp_func_ids:
                    fullgraph_complex[k].add(i)
    return fullgraph_complex


result = list(func_ids_complex)


def bubble_sort(arr, fullgraph, funcid_costs):
    n = len(arr)

    for i in range(n):
        for j in range(0, n-i-1):
            if funcid_costs[arr[j]] < funcid_costs[arr[j+1]]:
                arr[j], arr[j+1] = arr[j+1], arr[j]


bubble_sort(result, fullgraph_complex, funcid_costs)


def insert_sort(arr, fullgraph, funcid_costs):
    n = len(arr)
    new_arr = []
    new_arr.append(arr[0])
    #print(new_arr)
    for i in range(1, n):
        j = 0
        insert_flag = False
        while j < len(new_arr):
            if cmp_call(new_arr[j], arr[i], fullgraph) == -1:
                new_arr.insert(j, arr[i])
                insert_flag = True
                break
            j += 1
        if not insert_flag:
            new_arr.append(arr[i])
#        print(new_arr)
    return new_arr
    
final_result = insert_sort(result, fullgraph_complex, funcid_costs)


for i in final_result:
    print(i, funcid_costs[i])

#for k, v in fullgraph_complex.items():
#    print(k, v)
