# $Id$
#
# Run validation in a directory and possibly its subdirectories
#
# relevant targets for the end-user:
# - validate-test: validate to "test" result files directly
# - unvalidate: cleanup before validate-test
# - validate-out: validate to "out" files
# - clean: clean directories
# - generate-test: generate missing "test" files
#
# relevant variables for the user:
# - DO_BUG: also validate on cases tagged as "bugs"
# - DO_LATER: idem with future "later" cases
# - DO_SLOW: idem for lengthy to validate cases
# - D.sub: subdirectories in which to possibly recurse, defaults to *.sub
#
# example:
#   sh> make DO_LATER=1 validate-test

# what special cases are included
DO_BUG	=
DO_LATER=
DO_SLOW	= 1

# pips exes
TPIPS	= tpips
PIPS	= pips

# 10 minutes default timeout
# use 0 for no timeout
TIMEOUT	= 600

# default output file
# this can be modified to generate separate files
# see "validate-out" and "validate-test" targets
TEST	= test

# is it a subversion working copy?
IS_SVN	= test -d .svn

# some parametric commands
CHECK	= $(IS_SVN)
DIFF	= svn diff
UNDO	= svn revert
LIST	= svn status

# prefix of tests to be run, default is all
PREFIX	=

# automatic sub directories,
# D.sub could be set explicitely to anywhere to recurse
D.sub	= $(wildcard *.sub)

# directory recursion
D.rec	= $(D.sub:%=%.rec)

# source files
F.c	= $(wildcard *.c)
F.f	= $(wildcard *.f)
F.F	= $(wildcard *.F)
F.f90	= $(wildcard *.f90)
F.f95	= $(wildcard *.f95)

# all source files
F.src	= $(F.c) $(F.f) $(F.F) $(F.f90) $(F.f95)

# all potential result directories
F.res	= \
	$(F.c:%.c=%.result) \
	$(F.f:%.f=%.result) \
	$(F.F:%.F=%.result) \
	$(F.f90:%.f90=%.result) \
	$(F.f95:%.f95=%.result)

# actual result directory to validate
F.result= $(wildcard $(PREFIX)*.result)

# various validation scripts
F.tpips	= $(wildcard *.tpips)
F.tpips2= $(wildcard *.tpips2)
F.test	= $(wildcard *.test)
F.py	= $(wildcard *.py)

F.exe	= $(F.tpips) $(F.tpips2) $(F.test) $(F.py)

# optimistic possible results for Ronan
F.future_result = \
	$(F.tpips:%.tpips=%.result) \
	$(F.tpips2:%.tpips2=%.result) \
	$(F.test:%.test=%.result) \
	$(F.py:%.py=%.result) \
	$(F.c:%.c=%.result) \
	$(F.f:%.f=%.result) \
	$(F.F:%.F=%.result) \
	$(F.f90:%.f90=%.result) \
	$(F.f95:%.f95=%.result)

# validation output
F.valid	= $(F.result:%=%/$(TEST))

# all base cases
F.list	= $(F.result:%.result=%)

# where are we?
SUBDIR	= $(notdir $(PWD))
here	:= $(shell pwd)

# get rid of absolute file names in output...
FLT	= sed -e 's,$(here),$$VDIR,g'

# where to store validation results
# this is the default, but may need to be overriden
RESULTS	= RESULTS

# shell environment to run validation scripts
# this is a requirement!
SHELL	= /bin/bash

# whether we are recurring in a specially "marked" directory
RECWHAT	=

# skip bug/later/slow cases depending on options
# a case requires a result directory, so a "bug/later/slow"
# tag is not counted if there is no corresponding result.
EXCEPT =  [ "$(RECWHAT)" ] && \
	    { echo "$(RECWHAT): $(SUBDIR)/$*" >> $(RESULTS) ; exit 0 ; } ; \
	  [ ! "$(DO_BUG)" -a -f $*.bug -a -d $*.result ] && \
	    { echo "bug: $(SUBDIR)/$*" >> $(RESULTS) ; exit 0 ; } ; \
	  [ ! "$(DO_LATER)" -a -f $*.later -a -d $*.result ] && \
	    { echo "later: $(SUBDIR)/$*" >> $(RESULTS) ; exit 0 ; } ; \
	  [ ! "$(DO_SLOW)" -a -f $*.slow -a -d $*.result ] && \
	    { echo "slow: $(SUBDIR)/$*" >> $(RESULTS) ; exit 0 ; }

# setup running a case
PF	= @echo "processing $(SUBDIR)/$+" ; \
	  $(EXCEPT) ; \
	  set -o pipefail ; unset CDPATH ; \
	  export PIPS_MORE=cat PIPS_TIMEOUT=$(TIMEOUT) LC_ALL=C

# recursion into a subdirectory with target "FORWARD"
# a whole directory can be marked as bug/later/slow,
# in which case while recurring this mark take precedence about
# local information made available within the directory
%.rec: %
	recwhat= ; d=$* ; d=$${d%.sub} ; \
	[ ! "$(DO_BUG)" -a -f $$d.bug ] && recwhat=bug ; \
	[ ! "$(DO_LATER)" -a -f $$d.later ] && recwhat=later ; \
	[ ! "$(DO_SLOW)" -a -f $$d.slow ] && recwhat=slow ; \
	[ "$(RECWHAT)" ] && recwhat=$(RECWHAT) ; \
	$(MAKE) RECWHAT=$$recwhat RESULTS=../$(RESULTS) SUBDIR=$(SUBDIR)/$^ \
		-C $^ $(FORWARD)

# extract validation result for summary when the case was run
# four possible outcomes: passed, changed, failed, timeout
# 134 is for pips_internal_error, could allow to distinguish voluntary aborts.
OK	= status=$$? ; \
	  if [ "$$status" -eq 203 ] ; then \
	     echo "timeout: $(SUBDIR)/$* $$SECONDS" ; \
	  elif [ "$$status" != 0 ] ; then \
	     echo "failed: $(SUBDIR)/$* $$SECONDS" ; \
	  else \
	     $(DIFF) $*.result/test > $*.diff ; \
	     if [ -s $*.diff ] ; then \
	        echo "changed: $(SUBDIR)/$* $$SECONDS" ; \
	     else \
	        $(RM) $*.err $*.diff ; \
	        echo "passed: $(SUBDIR)/$* $$SECONDS" ; \
	     fi ; \
	  fi >> $(RESULTS)

# default target is to clean
.PHONY: clean
clean: rec-clean clean-validate
LOCAL_CLEAN	= clean-validate

.PHONY: clean-validate
clean-validate:
	$(RM) *~ *.o *.s *.tmp *.err *.diff *.result/out out err a.out RESULTS
	$(RM) -r *.database .PYPS*.tmp

.PHONY: rec-clean
rec-clean:
	[ "$(D.rec)" ] && $(MAKE) FORWARD=clean $(D.rec) || exit 0

.PHONY: validate
validate:
	# Parallel validation
	# run "make validate-test" to generate "test" files.
	# run "make validate-out" to generate usual "out" files.
	# run "make unvalidate" to revert test files to their initial status.

.PHONY: validate-dir
# the PARALLEL_VALIDATION macro tell whether it can run in parallel
ifdef PARALLEL_VALIDATION
validate-dir: $(LOCAL_CLEAN)
	$(RM) $(F.valid)
	$(MAKE) $(D.rec) $(F.valid)
	$(MAKE) sort-local-result

else # sequential validation, including subdir recursive forward
validate-dir: $(LOCAL_CLEAN)
	$(RM) $(F.valid)
	$(MAKE) $(D.rec) sequential-validate-dir
	$(MAKE) sort-local-result

# local target to parallelize the "sequential" local directory
# with test cases in its subdirectories
sequential-validate-dir:
	for f in $(F.valid) ; do $(MAKE) $$f ; done
endif

# how to summarize results to a human
SUMUP	= pips_validation_summary.pl

# on local validations only, sort result & show summary
.PHONY: sort-local-result
sort-local-result:
	@if [ $(RESULTS) = RESULTS -a -f RESULTS ] ; then \
	  mv RESULTS RESULTS.tmp ; \
	  sort -k 2 RESULTS.tmp > RESULTS ; \
	  $(RM) RESULTS.tmp ; \
	  $(SUMUP) RESULTS ; \
	fi

# restore all initial "test" result files if you are unhappy with a validate
.PHONY: unvalidate
unvalidate: do-unvalidate rec-unvalidate

.PHONY: do-unvalidate
do-unvalidate:: check-vc
	-$(CHECK) && [ $(TEST) = 'test' ] && $(UNDO) $(F.valid)

.PHONY: rec-unvalidate
rec-unvalidate::
	[ "$(D.rec)" ] && $(MAKE) FORWARD=unvalidate $(D.rec) || exit 0

# generate "out" files
.PHONY: validate-out
validate-out:
	$(MAKE) TEST=out DIFF=pips_validation_diff_out.sh \
		FORWARD=$@ LIST=: UNDO=: validate-dir

# generate "test" files: svn diff show the diffs!
.PHONY: validate-test
validate-test: check-vc
	$(MAKE) FORWARD=$@ TEST=test validate-dir

# hack: validate depending on prefix, without forwarding?
validate-%:
	$(MAKE) F.result="$(wildcard $**.result)" validate-dir

# generate missing "test" files
.PHONY: generate-test
generate-test: $(F.valid)

# generate empty result directories, for Ronan
# beware that this is a magick guess from the contents of the directory
# you then have to generate the corresponding "test" file
# and commit everything on the svn
.PHONY: generate-result
generate-result: $(F.future_result)

# generate an empty result directory
%.result:
	@echo "creating: $@" ; mkdir $@ ; touch $@/test

# (shell) script
%.result/$(TEST): %.test
	$(PF) ; ./$< 2> $*.err | $(FLT) > $@ ; $(OK)

# tpips scripts
%.result/$(TEST): %.tpips
	$(PF) ; $(TPIPS) $< 2> $*.err | $(FLT) > $@ ; $(OK)

%.result/$(TEST): %.tpips2
	$(PF) ; $(TPIPS) $< 2>&1 | $(FLT) > $@ ; $(OK)

# python scripts
ifdef PIPS_VALIDATION_NO_PYPS
%.result/$(TEST): %.py
	$(EXCEPT) ; echo "keptout: $(SUBDIR)/$*" >> $(RESULTS)
else # else we have pyps
%.result/$(TEST): %.py
	$(PF) ; python $< 2> $*.err | $(FLT) > $@ ; $(OK)
endif # PIPS_VALIDATION_NO_PYPS

# default_tpips
# FILE could be $<
# VDIR could be avoided if running in local directory?
DFTPIPS	= default_tpips
%.result/$(TEST): %.c $(DFTPIPS)
	$(PF) ; WSPACE=$* FILE=$(here)/$< VDIR=$(here) $(TPIPS) $(DFTPIPS) \
	2> $*.err | $(FLT) > $@ ; $(OK)

%.result/$(TEST): %.f $(DFTPIPS)
	$(PF) ; WSPACE=$* FILE=$(here)/$< VDIR=$(here) $(TPIPS) $(DFTPIPS) \
	2> $*.err | $(FLT) > $@ ; $(OK)

%.result/$(TEST): %.F $(DFTPIPS)
	$(PF) ; WSPACE=$* FILE=$(here)/$< VDIR=$(here) $(TPIPS) $(DFTPIPS) \
	2> $*.err | $(FLT) > $@ ; $(OK)

# default_test relies on FILE WSPACE NAME
# warning: Semantics & Regions create local "properties.rc":-(
DEFTEST	= default_test
%.result/$(TEST): %.c $(DEFTEST)
	$(PF) ; WSPACE=$* FILE=$(here)/$< sh $(DEFTEST) \
	2> $*.err | $(FLT) > $@ ; $(OK)

%.result/$(TEST): %.f $(DEFTEST)
	$(PF) ; WSPACE=$* FILE=$(here)/$< sh $(DEFTEST) \
	2> $*.err | $(FLT) > $@ ; $(OK)

%.result/$(TEST): %.F $(DEFTEST)
	$(PF) ; WSPACE=$* FILE=$(here)/$< sh $(DEFTEST) \
	2> $*.err | $(FLT) > $@ ; $(OK)


# default_pyps relies on FILE & WSPACE
PYTHON	= python
DEFPYPS	= default_pyps.py
ifdef PIPS_VALIDATION_NO_PYPS
%.result/$(TEST): %.c $(DEFPYPS)
	$(EXCEPT) ; echo "keptout: $(SUBDIR)/$*" >> $(RESULTS)

%.result/$(TEST): %.f $(DEFPYPS)
	$(EXCEPT) ; echo "keptout: $(SUBDIR)/$*" >> $(RESULTS)

%.result/$(TEST): %.F $(DEFPYPS)
	$(EXCEPT) ; echo "keptout: $(SUBDIR)/$*" >> $(RESULTS)
else # with pyps
%.result/$(TEST): %.c $(DEFPYPS)
	$(PF) ; WSPACE=$* FILE=$(here)/$< $(PYTHON) $(DEFPYPS) \
	2> $*.err | $(FLT) > $@ ; $(OK)

%.result/$(TEST): %.f $(DEFPYPS)
	$(PF) ; WSPACE=$* FILE=$(here)/$< $(PYTHON) $(DEFPYPS) \
	2> $*.err | $(FLT) > $@ ; $(OK)

%.result/$(TEST): %.F $(DEFPYPS)
	$(PF) ; WSPACE=$* FILE=$(here)/$< $(PYTHON) $(DEFPYPS) \
	2> $*.err | $(FLT) > $@ ; $(OK)
endif # PIPS_VALIDATION_NO_PYPS

# detect skipped stuff
.PHONY: skipped
skipped:
	for base in $(sort $(basename $(F.src) $(F.exe))) ; do \
	  if ! test -d $$base.result ; \
	  then \
	    echo "skipped: $(SUBDIR)/$$base" ; \
	  elif ! [ -f $$base.result/test -o -f $$base.result/test.$(ARCH) ] ; \
	  then \
	    echo "missing: $(SUBDIR)/$$base" ; \
	  fi ; \
	done >> $(RESULTS)

# test RESULT directory without any script
.PHONY: orphan
orphan:
	for base in $(sort $(F.list)) ; do \
	  test -f $$base.tpips -o \
	       -f $$base.tpips2 -o \
	       -f $$base.test -o \
	       -f $$base.py -o \
	       -f default_tpips -o \
	       -f default_pyps.py -o \
	       -f default_test || \
	  echo "orphan: $(SUBDIR)/$$base" ; \
	done >> $(RESULTS)

# test case with multiple scripts... one is randomly (?) chosen
.PHONY: multi-script
multi-script:
	for base in $$(echo $(basename $(F.exe))|tr ' ' '\012'|sort|uniq -d); \
	do \
	  echo "multi-script: $(SUBDIR)/$$base" ; \
	done >> $(RESULTS)

# test case with multiple sources (c/f/F...)
.PHONY: multi-source
multi-source:
	for base in $$(echo $(basename $(F.src))|tr ' ' '\012'|sort|uniq -d); \
	do \
	  echo "multi-source: $(SUBDIR)/$$base" ; \
	done >> $(RESULTS)

# all possible inconsistencies
.PHONY: inconsistencies
inconsistencies: skipped orphan multi-source multi-script

# what about nothing?
# source files without corresponding result directory
.PHONY: missing
missing:
	@echo "# checking for missing (?) result directories"
	@ n=0; \
	for res in $(F.res) ; do \
	  if [ ! -d $$res ] ; then \
	     echo "missing: $$res" ; \
	     let n++; \
	  fi ; \
	done ; \
	echo "# $$n missing result(s)"

.PHONY: missing-vc
missing-vc:
	@echo "# result directories not under version control"
	@$(LIST) | grep '\.result'

# check that we are in a working copy
.PHONY: check-vc
check-vc:
	@$(CHECK) || { \
	  echo "error: validation must be a working copy" >&2 ; \
	  exit 1 ; \
	}

.PHONY: count
count:
	@echo "number of validations:" `echo $(F.result) | wc -w`