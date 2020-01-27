#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys

def gen_funcIdCost(infile):
    with open("funcIdCost.log", "w") as outfile:
        for line in infile.readlines():
            fields = line.split(",")
            funcId = int(fields[0])
            cost = int(fields[2])
            outfile.write("{},{}\n".format(funcId, cost))


def main():
    with open(sys.argv[1]) as infile:
        infile.readline()
        gen_funcIdCost(infile)


if __name__ == "__main__":
    main()
