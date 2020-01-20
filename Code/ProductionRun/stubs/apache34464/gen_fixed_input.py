#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

def gen_input_file():

    for i in range(500, 10500, 1000):

        CASE_NAME = './inputs/input_case_{0}.txt'
        CONSTANT_SONG = 'wxyz'

        file_name = CASE_NAME.format(i)
        with open(file_name, 'w') as f:
            context = ('a' * i) + CONSTANT_SONG
            f.write(context)


if __name__ == "__main__":
	os.mkdir("./inputs")
	gen_input_file()
