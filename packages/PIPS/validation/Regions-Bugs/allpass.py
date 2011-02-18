from __future__ import with_statement
from pyps import *
import openmp

class mycompiler(gccCompiler,ompCompiler):
	pass

with workspace("allpass.c","tap.c", "cdelay.c", "wrap.c","tools.c") as w:
	w.all_functions.openmp(verbose=True)
	w.compile(compiler=mycompiler(LDFLAGS="-lm"))
