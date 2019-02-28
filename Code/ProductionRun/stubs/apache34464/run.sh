#!/usr/bin/env bash

PROJECT_DIR=../../
LOGGER=${PROJECT_DIR}cmake-build-debug/dumpmem/dumpmem
RESULT=./results/inner_loop.csv

export SAMPLE_RATE=100
#export SAMPLE_RATE=1
echo rms,cost > ${RESULT}

for i in inputs/*
do
	echo "./targets/target $i wxyz"
	./targets/target $i wxyz 1>/dev/null 2>&1
	${LOGGER} >> ${RESULT}
done
