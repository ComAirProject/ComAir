#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Rank functions in oprofile result.

The second parameter is to filter the bug name (same as the bin name) in oprofile result.
This is to rule out functions from systems.

Usage: ./parse_operf_result.py operf_result_file bug_name 

Output:
"rank:function_name"

"""

import sys

def parse_operf_result(lines):
    cnt = 1
    for line in lines:
        if line[0].isdigit() and sys.argv[2] in line:
            line = line.strip()
            fields = line.split()
            print("{}:{}".format(cnt, fields[3]))
            cnt += 1

def main():
    if len(sys.argv) < 3:
        print("Two arguments: operf_result_file bug_name")
        return
    with open(sys.argv[1]) as infile:
        lines = infile.readlines()
        parse_operf_result(lines)

if __name__ == "__main__":
    main()
