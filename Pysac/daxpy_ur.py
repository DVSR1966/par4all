from __future__ import with_statement # this is to work with python2.5
from pyps import workspace
from sac import workspace as sac_workspace
from os import remove
filename="daxpy_ur"
with workspace(filename+".c", parents=[sac_workspace], driver="sse", deleteOnClose=True) as w:
	m=w[filename]
	m.display()
	m.sac()
	m.display()
	w.goingToRunWith(w.save(rep="save-dp"),"save-dp")

