#
# $Id$
# 
# $Log: config.makefile,v $
# Revision 1.62  1997/06/09 09:53:20  coelho
# separate hpfc pvm lib.
#
# Revision 1.61  1997/06/06 14:54:21  zory
# hpfc_communication -> hpfc_communication_pvm
#
# Revision 1.56  1997/05/29 13:31:49  zory
# _HPFC_DEBUG_ added
#
# Revision 1.54  1997/05/28 15:47:49  zory
# -ansi added to g77.
#
# Revision 1.53  1997/04/17 11:54:47  coelho
# better RCS headers.
#
#
# depends on 
# + PVM_ARCH 
# + PVM_ROOT
# + _HPFC_USE_PVMe_

RT_ARCH=$(PIPS_ARCH)/$(PVM_ARCH)

#
# additional defs for m4
ifeq ($(FC),g77)
M4FLAGS	+=	-D _HPFC_NO_BYTE1_ \
		-D _HPFC_NO_INTEGER2_
_HPFC_USE_GNU_ = 1
endif

PVM_ENCODING_OPTION =	PvmDataInPlace
HPFC_MAX_NPES	=	32

M4FLAGS+= -D _HPFC_DIMENSIONS_=3
M4FLAGS+= -D _HPFC_MAX_NPES_=$(HPFC_MAX_NPES)
M4FLAGS+= -D _HPFC_ENCODING_=$(PVM_ENCODING_OPTION)

M4FLAGS	+= -D _HPFC_DEMO_
M4FLAGS	+= -D _HPFC_DIRECT_
# M4FLAGS	+= -D _HPFC_DEBUG_

# the default on IBM is to use PVMe
ifeq ($(PVM_ARCH),RS6K)
_HPFC_USE_PVMe_ = 1
endif

ifdef _HPFC_USE_PVMe_
M4FLAGS	+= -D _HPFC_USE_PVMe_
endif


#############################################################################

SCRIPTS =	hpfc_llcmd \
		hpfc_qsub \
		hpfc_add_warning \
		hpfc_generate_h \
		hpfc_generate_init

#
# Default compilers and options

#
# others

ifeq ($(PVM_ARCH),CM5)
#
# Thinking Machine CM5
#
FFLAGS	+= -Nx1000

CMMD_INDIR	= /net/cm5/CMSW/CMMD/cmmd-3.2/include
CMMD_F77_H	= cmmd_fort.h
#
endif

ifeq ($(PVM_ARCH),SUN4)
#
# SUN - SOLARIS 1 (SUNOS 4)
#
FFLAGS		= -fast -u
#
endif

ifeq ($(PVM_ARCH),SUNMP)
#
# SUN - SOLARIS 2
#
CC	= cc
CFLAGS	= -O2
FC	= f77
FFLAGS	= -fast -u
#
endif

ifeq ($(PVM_ARCH),SUN4SOL2)
#
# SUN - SOLARIS 2
#
CC	= cc
CFLAGS	= -O2
FC	= f77
FFLAGS	= -fast -u
#
endif

ifeq ($(PVM_ARCH),RS6K)
#
# IBM compilers on RS6K/SPx...
#
FC	= xlf
# FFLAGS	= -O2 -u
FFLAGS	= -O2 -qarch=pwr2 -qhot -u
CC	= xlc
CFLAGS	= -O2
#
endif

ifeq ($(PVM_ARCH),ALPHA)
#
# DEC alpha
#
FC	= f77
FFLAGS	= -fast -u
CC	= cc
CFLAGS	= -O4
#
endif

# ??? this env. dependence is not very convincing...
ifdef _HPFC_USE_GNU_
#
# GNU Compilers
# if set, overwrites the architecture dependent defaults...
#
FC	= g77
# -Wall -pedantic
FFLAGS	= -O2 -pipe -ansi -Wall -Wimplicit 
CC	= gcc
CFLAGS	= -O2 -pipe -ansi -Wall -pedantic
CPPFLAGS= -D__USE_FIXED_PROTOTYPES__
#
endif

M4FLAGS += -D PVM_ARCH=$(PVM_ARCH) hpfc_lib_m4_macros

COPY	= cp
MOVE 	= mv

#
# I distinguish between PVM{3,e}_ROOT...

pvminc	= $(PVM_ROOT)/include
pvmconf	= $(PVM_ROOT)/conf

ifdef _HPFC_USE_PVMe_
#
# if another PVM is used, I still need PVM 3 m4 macros...
# IBM puts includes in lib:-(
pvminc	= $(PVM_ROOT)/lib
pvmconf	= $(PVM3_ROOT)/conf
#
endif

ifeq ($(PVM_ARCH),CRAY)
#
# CRAY PVM is does not have pvm_version
# also no need to link to the pvm library
#
M4FLAGS	+= 	-D _HPFC_NO_PVM_VERSION_ \
		-D _HPFC_NO_BYTE1_ \
		-D _HPFC_NO_INTEGER2_ \
		-D _HPFC_NO_REAL4_ \
		-D _HPFC_NO_COMPLEX8_
PVM_ENCODING_OPTION =	PvmDataRaw
HPFC_MAX_NPES	= 512
#
# T3D:
# pvminc	= /usr/include/mpp
# HPFC_MAX_NPES =		128
endif

#
# pvm3 portability macros for Fortran calls to C functions:

M4COPT	+=	$(PVM_ARCH).m4

PVM_HEADERS  =	pvm3.h fpvm3.h
LIB_M4FFILES = 	hpfc_packing.m4f \
		hpfc_reductions.m4f \
		hpfc_rtsupport.m4f \
		hpfc_shift.m4f \
		hpfc_bufmgr.m4f \
		hpfc_broadcast.m4f \
		hpfc_communication_pvm.m4f

LIB_M4CFILES =	hpfc_misc.m4c
LIB_FFILES =	hpfc_check.f \
		hpfc_main.f \
		hpfc_main_host.f \
		hpfc_main_node.f

M4_HEADERS 	= hpfc_procs.m4h \
		  hpfc_buffers.m4h \
		  hpfc_parameters.m4h
CORE_HEADERS	= hpfc_commons.h \
		  hpfc_param.h \
		  hpfc_globs.h \
		  hpfc_misc.h

DDC_FFILES 	= $(LIB_M4FFILES:.m4f=.f)
DDC_CFILES	= $(LIB_M4CFILES:.m4c=.c)
DDC_HEADERS 	= $(LIB_M4FFILES:.m4f=.h) \
		  $(M4_HEADERS:.m4h=.h)\
		  hpfc_includes.h

$(DDC_FFILES) $(DDC_CFILES) $(DDC_HEADERS): $(PVM_ARCH).m4

LIB_HEADERS	= $(CORE_HEADERS) \
		  $(DDC_HEADERS)

#
# OBJECT files
#
LIB_OBJECTS:= $(addprefix $(RT_ARCH)/, $(DDC_FFILES:.f=.o) $(DDC_CFILES:.c=.o))

LIB_RTM_OBJECTS	= $(filter-out %pvm.o %mpi.o, $(LIB_OBJECTS))
LIB_PVM_OBJECTS	= $(filter %pvm.o, $(LIB_OBJECTS))
LIB_MPI_OBJECTS	= $(filter %mpi.o, $(LIB_OBJECTS))

#
#
# MISC files
M4_MACROS 	= hpfc_lib_m4_macros hpfc_architecture_m4_macros
HPFC_MAKEFILES 	= hpfc_Makefile_init 
DOCS		= hpfc_runtime_library.README

#
# the files to install

SOURCES = 	$(M4_MACROS) \
		$(M4_HEADERS) \
		$(LIB_FFILES) \
		$(LIB_M4FFILES) \
		$(LIB_M4CFILES) \
		$(HPFC_MAKEFILES) \
		$(CORE_HEADERS) \
		$(DOCS) \
		$(SCRIPTS)

#
# Targets to be built
#
LIB_TARGET 	= $(RT_ARCH)/libhpfcruntime.a
LIB_PVM_TARGET 	= $(RT_ARCH)/libhpfcpvm.a
LIB_MPI_TARGET 	= $(RT_ARCH)/libhpfcmpi.a
MKI_TARGET 	= $(RT_ARCH)/compilers.make

# $(LIB_OBJECTS) $(LIB_TARGET): $(RT_ARCH)

$(RT_ARCH): $(PIPS_ARCH) ; -test -d $@ || mkdir $@

#
# Installation:

INSTALL_INC_DIR:=$(INSTALL_RTM_DIR)/hpfc
INSTALL_LIB_DIR:=$(INSTALL_RTM_DIR)/hpfc/$(RT_ARCH)

INSTALL_INC =	$(CORE_HEADERS) \
		$(DDC_HEADERS) \
		$(HPFC_MAKEFILES) \
		$(M4_MACROS) \
		$(SCRIPTS) \
		$(LIB_FFILES) 

INSTALL_LIB=	$(LIB_TARGET) \
		$(LIB_PVM_TARGET) \
		$(LIB_MPI_TARGET) \
		$(MKI_TARGET)

recompile: quick-install

#
# rules
#

ifeq ($(PVM_ARCH),CM5)
all: $(CMMD_F77_H) 
#
$(CMMD_F77_H):	$(CMMD_INDIR)/cm/$(CMMD_F77_H)
	$(COPY) $(CMMD_INDIR)/cm/$(CMMD_F77_H) .
endif

all: $(RT_ARCH) $(PVM_HEADERS) $(DDC_HEADERS) $(DDC_CFILES) $(DDC_FFILES) \
		$(LIB_OBJECTS) $(INSTALL_LIB)

#
# get pvm headers
#

pvm3.h:	$(pvminc)/pvm3.h; $(COPY) $< $@
fpvm3.h:$(pvminc)/fpvm3.h; $(COPY) $< $@
$(PVM_ARCH).m4:; $(COPY) $(pvmconf)/$(PVM_ARCH).m4 $@

#
# HPFC RUNTIME
#
$(LIB_TARGET):	$(LIB_HEADERS) $(LIB_RTM_OBJECTS) 
	$(RM) $@ ; \
	$(ARCHIVE) $@ $(LIB_RTM_OBJECTS) ; \
	$(RANLIB) $@

#
# HPFC PVM
#
$(LIB_PVM_TARGET): $(PVM_HEADERS) $(LIB_HEADERS) $(LIB_PVM_OBJECTS) 
	$(RM) $@ ; \
	$(ARCHIVE) $@ $(LIB_PVM_OBJECTS) ; \
	$(RANLIB) $@

#
# HPFC MPI
#
$(LIB_MPI_TARGET): $(PVM_HEADERS) $(LIB_HEADERS) $(LIB_MPI_OBJECTS) 
	$(RM) $@ ; \
	$(ARCHIVE) $@ $(LIB_MPI_OBJECTS) ; \
	$(RANLIB) $@

#
#
#
%.h: %.f
	# building $@ from $<
	sh ./hpfc_generate_h < $< > $@ ; \
	sh ./hpfc_add_warning $@

$(RT_ARCH)/%.o: %.c
	$(COMPILE) $< -o $@

ifeq ($(PVM_ARCH),CRAY)
# the cray compilers do not like combining -c and -o
$(RT_ARCH)/%.o: %.f; $(F77COMPILE) $< ; mv $*.o $@
else
$(RT_ARCH)/%.o: %.f; $(F77COMPILE) $< -o $@
endif

$(RT_ARCH)/compilers.make:
	#
	# building $@
	#
	{ echo "FC=$(FC)"; \
	  echo "FFLAGS=$(FFLAGS)";\
	  echo "FOPT=$(FOPT)";\
	  echo "CC=$(CC)";\
	  echo "CFLAGS=$(CFLAGS)";\
	  echo "CPPFLAGS=$(CPPFLAGS)";\
	  echo "M4=$(M4)";\
	  echo "_HPFC_USE_PVMe_=$(_HPFC_USE_PVMe_)";} > $@

hpfc_includes.h: $(LIB_M4FFILES:.m4f=.h) 
	#
	# building $@
	#
	{ for i in $(LIB_M4FFILES:.m4f=.h) ; do \
	  echo "      include \"$$i\"" ; done; } > $@
	sh ./hpfc_add_warning $@

clean: local-clean
local-clean: 
	$(RM) *~ $(LIB_OBJECTS) $(PVM_HEADERS) \
		$(DDC_HEADERS) 	$(DDC_FFILES) $(DDC_CFILES) $(INSTALL_LIB)
	test ! -d $(RT_ARCH) || rmdir $(RT_ARCH)

# that is all
#
