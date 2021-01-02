# ComAir

ComAir can effectively conduct algorithmic profiling under both production-run and in-house settings.

System requirements: Ubuntu 16.04, 64G memory, 2T disk.

Software requirements: LLVM-5.0, Python3.5.2, Matlab.

The experiment results: https://docs.google.com/spreadsheets/d/14GMzctx2RbaD_BLiGbhk-lBFjaCvEzWjDWd3KDGuA6c/edit?usp=sharing

VM Image Download link: https://psu.box.com/s/drf06kfzakww07qgn6udl602h1xozmk2

The VM Image can be run in VirtualBox 6.1.16 r140961 (Qt5.6.2) on Windows 10.0.19041 Version 19041.

The OS in VM is Ubuntu 16.04 and it requires 64G memory and 2T disk.

VM Username: boqin
VM Password: comair

The code is under
```
/home/boqin/Projects/ComAir
```
cd ```build``` under InHouse or ProductionRun

```cmake ..; make``` to get the corresponding binary files.

The binary files are linked to /home/boqin/Env/ to simplify usage.

All the bugs are under
```
/home/boqin/Projects/stubs
```
Some of them are compressed to save disk.

e.g., under ```Projects/stubs/adrian-icse/apache53637```

- Makefile.ih: to build InHouse version

- Makefile.pr: to build ProductionRun version

- run_mem_ih.py: to get InHouse logs

- run_mem_pr.py: to get ProductionRun logs

The directory ```scripts```:

1. /home/boqin/Projects/ComAir/scripts/fitting contains Matlab code to parse the logs and output the complexity for each function

2. /home/boqin/Projects/ComAir/scripts/ranking contains python scripts to parse InHouse logs & complexity and output InHouse ranking

3. /home/boqin/Projects/ComAir/scripts/oprofile contains python scripts to parse oprofile ranking results

4. /home/boqin/Projects/ComAir/scripts/aprof contains python scripts to parse InHouse logs & complexity and output Aprof ranking

5. /home/boqin/Projects/ComAir/scripts/pin contains code to get top loops' locations for ProductionRun

6. /home/boqin/Projects/ComAir/scripts/time contains python scripts to parse time results

Note that some experiments can only be run on a physical machine.

Thee VM Image is more suitable for demonstration than experiments.
