#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import random

def gen_input_file():

    inputs = []
    for x in range(10):
        inputs.append(random.randint(500,10500))

    for i in inputs:

        CASE_NAME = './inputs/input_case_{0}.txt'
        CONSTANT_SONG = 'wxyz'

        file_name = CASE_NAME.format(i)
        with open(file_name, 'w') as f:
            context = ('a' * i) + CONSTANT_SONG
            f.write(context)


if __name__ == "__main__":
	os.mkdir("./inputs")
	gen_input_file()
