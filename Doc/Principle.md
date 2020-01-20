#  ProductionRun

## Loop

### Input
-strFile ${FILENAME} -strFunc ${FUNCNAME} -noLine ${LINENO}

Top 5 loops, must include the InnerLoop

### Instrument
InnerLoop



### Opt
#### Opt inst sites
1. Dominance
For the same memory with only read/write (no addr_of to avoid alias), if a read/write A doms read/write B, then only only records A

2. Hoist
Hois the invar to preheader

#### Opt Cost
1. Algorithm

#### Opt Loop
1. Array
Only record first and last: Hoist&Sink

2. LinkedList
Only record Read

#### Sample

pass



## Workflow:

1. Find loop by location
2. If array, find indvar and stride.
3. Else If linkedlist, find all read.
4. Else, find invar (Hoist).
5. Find dominance of R/W.
6. Instrument Cost. (Opt cost)
