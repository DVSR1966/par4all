# $Id$

FWD_DIRS	= src

install:
	$(MAKE) -C src phase1
	$(MAKE) -C src phase2
	$(MAKE) -C src phase3
