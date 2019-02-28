#!/usr/bin/env bash

PROJECT_DIR=../../
LOGGER=${PROJECT_DIR}runtime/AprofLogger/AprofLogger
RESULT=./results/outer_func.csv

for i in inputs/*
do
	echo "./targets/target $i wxyz"
	./targets/target $i wxyz 1>/dev/null 2>&1
	${LOGGER}
done

echo func_id,rms,cost,chains > ${RESULT}
cat aprof_logger_* | grep ^10, >> ${RESULT}
rm aprof_logger_*
