#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Rank functions >O(N) by cost.

This is the method used by aprof.

Usage: ./rank_by_cost.py complexity.csv inhouse_results.csv

Output:
The number of functions >O(N).
The list of functions ordered by cost: "rank:function_id:cost".
"""

import sys

def parse_complexity_csv(complexity_csv):
    complexity_results = {}
    with open(complexity_csv) as infile:
        for line in infile.readlines():
            line = line.strip()
            fields = line.split(',')
            if len(fields) < 2:
                continue
            if not fields[0].isnumeric() or not fields[1].isnumeric():
                continue
            func_id = int(fields[0])
            complexity = int(fields[1])
            complexity_results[func_id] = complexity
    return complexity_results

def filter_out_omega_n(complexity_results):
    #return complexity_results
    return dict(filter(lambda elem: elem[1] > 1, complexity_results.items()))

def parse_inhouse_largest_csv(inhouse_csv):
    cost_results = {}
    with open(inhouse_csv) as infile:
        for line in infile.readlines():
            line = line.strip()
            fields = line.split(',')
            if len(fields) < 3:
                print("fields < 3")
                continue
            if not fields[0].isnumeric() or not fields[1].isnumeric() or not fields[2].isnumeric():
                print("fields not number")
                continue
            func_id = int(fields[0])
            cost = int(fields[2])
            if func_id not in cost_results:
                cost_results[func_id] = cost
            else:
                cost_results[func_id] = max(cost_results[func_id], cost)
    return cost_results

def sort_cost_results(cost_results):
    return dict(sorted(cost_results.items(), key=lambda item: item[1], reverse=True))

def rank_by_cost(complexity_csv, inhouse_csv):
    complexity_results = parse_complexity_csv(complexity_csv)
    gt_omega_n = filter_out_omega_n(complexity_results)
    print(">O(N):{}".format(len(gt_omega_n)))
    cost_results = parse_inhouse_largest_csv(inhouse_csv)
    sorted_cost_results = sort_cost_results(cost_results)
    cnt = 1
    for func_id, cost in sorted_cost_results.items():
        if func_id in gt_omega_n:
            print("{}:{}:{}".format(cnt, func_id, cost))
            cnt += 1

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Two arguments: complexity_csv inhouse_csv")
    else:
        rank_by_cost(sys.argv[1], sys.argv[2])
