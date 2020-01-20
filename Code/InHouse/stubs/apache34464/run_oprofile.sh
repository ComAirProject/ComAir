#!/usr/bin/env bash

PROJECT_DIR=../../
LOGGER=${PROJECT_DIR}runtime/AprofLogger/AprofLogger
RESULT=./results/outer_func.csv

let x=1
for i in inputs/*
do
	echo "./targets/target $i wxyz"
	operf ./targets/target $i wxyz 1>/dev/null 2>&1
	opreport --callgraph 1> ${x}.csv 2>&1
	let x++
	#${LOGGER}
done

#echo func_id,rms,cost,chains > ${RESULT}
#cat aprof_logger_* | grep ^10, >> ${RESULT}
#rm aprof_logger_*