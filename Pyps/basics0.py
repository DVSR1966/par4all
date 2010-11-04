from __future__ import with_statement # this is to work with python2.5
#!/usr/bin/env python

# import everything so that a session looks like tpips one
from pyps import workspace
import shutil,os


# a worspace ID is automagically created ... may be not a good feature
# the with statements ensure correct code cleaning
with workspace("basics0.c") as w:

	# you can get module object from the modules table
	foo=w.fun.foo
	bar=w.fun.bar
	malabar=w.fun.malabar
	mb=w["megablast"]
	
	# and apply transformation to modules
	foo.inlining(callers="bar",USE_INITIALIZATION_LIST=False)
	
	#the good old display, default to PRINTED_FILE, but you can give args
	foo.display()
	bar.display()
	malabar.display()
	bar.print_code()
	
	# you can also preform operations on loops
	mb.display("loops_file")
	for l in mb.loops():
	    l.unroll(rate=2)
	mb.display()
	
	# access all functions
	w.all.partial_eval()
	w.all.display()
	
	# recover a list of all labels in the source code ... without pipsing
	##
	import re # we are gonne use regular expression
	label_re = re.compile("^ *(\w+):")
	# code gives us a list of line view of modue's code
	lines=foo.code()
	labels=[]
	for line in lines:
		m = label_re.match(line)
		if m:
			for label in m.groups(1):
				labels.append(label);
	
	if labels:
		print "found labels:"
		for l in labels: print l
	##
	
	
	# new feature ! save the source code somewhere, so that it can be used after
	# the workspace is deleted
	a_out=w.compile(rep="basics0", link=False)


# tidy ..
shutil.rmtree("basics0")
os.remove("basics0.o")
