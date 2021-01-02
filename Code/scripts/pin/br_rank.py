#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
#def readfile(filename):
#fname='./output/thread12283-00.out'
fname = sys.argv[1]
file = open(fname)
content_raw = [line.rstrip('\n') for line in file]

row_num = len(content_raw)
column_num = 4

content = []
for row in range(row_num):
	content.append([])
	content[row] = content_raw[row].split() 

br_uniq = []
time_uniq = []
row_br = 0
for row in range(row_num):
	if content[row][0] not in br_uniq :
		br_uniq.append(content[row][0])
		time_uniq.append(int(content[row][3])+int(content[row+1][3]))
		row_br = row_br + 1

BrTime = []
for row in range(row_br):
	BrTime.append([])
	BrTime[row].append(br_uniq[row])
	BrTime[row].append(time_uniq[row])

BrTime = sorted(BrTime,key = lambda k: k[1],reverse = True)


#load the objdump.log file:

#fname2='./output/objdump.log'
fname2 = sys.argv[2]
file2 = open(fname2)
content_raw2 = [line.rstrip('\n') for line in file2]

row_num2 = len(content_raw2)

content2 = []
for row in range(row_num2):
	content2.append([])
	content2[row] = content_raw2[row].split()


#scan for the position of each row in BrTime
row2_lasttime = 10
count = 0
for row in range(row_br):
	if count > 400:
		location = 'skipped'
		BrTime[row].append(location)
		continue
	address_f1 = BrTime[row][0][-6:]
	flag_quickfind = 0
	for row2 in range(row2_lasttime+1,row2_lasttime+100):
		if not content2[row2] :
			continue
		if content2[row2][0][0] == '/':
			break
		address_f2 = content2[row2][0].replace(' ','')
		address_f2 = address_f2.replace(':','')
		if address_f2 == address_f1 :
			flag_quickfind = 1
			BrTime[row].append(BrTime[row-1][2])
			break
	if flag_quickfind == 1:
		continue
	for row2 in range(row_num2):
		if not content2[row2] :
			continue
		address_f2 = content2[row2][0].replace(' ','')
		address_f2 = address_f2.replace(':','')
		if address_f2 == address_f1 :
			flag_haslocation = 1
			while 1 :
				if not content2[row2]:
					row2 = row2 - 1
					continue
				if content2[row2][0][0] == '/':
					row2_lasttime = row2
					break
				if row2 <= 1 :
					flag_haslocation = 0
					break
				row2 = row2 - 1
			if flag_haslocation == 0:
				location = 'not_found'
			else:
				location = content2[row2][0]
				count = count + 1
			break
	BrTime[row].append(location)

location_list = []
for row in range(row_br):
	if BrTime[row][2] in location_list and BrTime[row][2] != 'skipped' and BrTime[row][2] != 'not_found':
		BrTime[row][2] = 'duplicated'
	else:
		location_list.append(BrTime[row][2])

for row in range(row_br):
	if BrTime[row][2] == 'duplicated' or BrTime[row][2] == 'skipped':
		continue
	else:
		print(BrTime[row])

