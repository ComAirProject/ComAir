# Use PIN to find the hottest loops

This is to get the candidate loops for Production-run.

1. Rank the branches by their execution times.
2. Filter loop-related branches manually.

## 1. Build the branch counting plugin

`$ make`

You get branch.so in current dir.

## 2. Run PIN to find the loop result

1. unzip pin-3.7-97619-g0d0c92f4f-gcc-linux.zip
2. export PATH=`realpath pin-3.7-97619-g0d0c92f4f-gcc-linux/`:$PATH
3. pin -t branch.so -- $YOUR_PROGRAM $INPUT; result is in the output dir
4. objdump -Dl $YOUR_PROGRAM > objdump.log
5. ./br_rank.py output/thread.*.out objdump.log > your_result

Note: compile YOUR_PROGRAM with `-g`
