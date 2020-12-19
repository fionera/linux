#!/usr/bin/python
import os, sys, re, getopt, mimetypes, struct

inputfile = open('CFG_low_latency.h','rb')

b = inputfile.read()

a = b.split(",")

print 'size = %d' % len(a)

fcnt =0
wcnt = 0

fname = "CFG_low_latency_%d.h" % fcnt
outfile = open(fname, 'w+')


outfile.write("const unsigned char CFG_12b_0_low_latency_%d[109727] = {\n" % fcnt)

for cnt in range(0,len(a)):

	if(cnt !=0 and cnt%109727 ==0):
		print "write file name = %s, cnt = %d" % (fname, cnt)
		fcnt+=1
		wcnt =0

		outfile.close()
		fname = "CFG_low_latency_%d.h" % fcnt


		outfile = open(fname, 'w+')

		outfile.write("const unsigned char CFG_12b_0_low_latency_%d[109727] = {\n" % fcnt)

	if(cnt !=0 and (cnt+1)%109727 ==0):
		outfile.write("%s};" % a[cnt].strip())
	else:
		outfile.write("%s," % a[cnt].strip())
	wcnt+=1
	if(wcnt%16 ==0):
		outfile.write("\n")



outfile.close()

inputfile.close()
