!
! COMMON /HPFC PARAM/
!
! $RCSfile: hpfc_param.h,v $ ($Date: 1996/09/07 16:24:12 $, )
! version $Revision$
! got on %D%, %T%
! $Id$
!
! the following files has to be included:
!     include 'parameters.h'
!
      common /HPFC PARAM/
     $     ATOT(MAX NB OF ARRAYS), 
     $     TTOP(MAX NB OF TEMPLATES), 
     $     NODIMA(MAX NB OF ARRAYS),
     $     NODIMT(MAX NB OF TEMPLATES),
     $     NODIMP(MAX NB OF PROCESSORS), 
     $     RANGEA(MAX NB OF ARRAYS, 7, 10),
     $     RANGET(MAX NB OF TEMPLATES, 7, 3),
     $     RANGEP(MAX NB OF PROCESSORS, 7, 3), 
     $     ALIGN(MAX NB OF ARRAYS, 7, 3),
     $     DIST(MAX NB OF TEMPLATES, 7, 2),
     $     MSTATUS(MAX NB OF ARRAYS),
     $     LIVE MAPPING(MAX NB OF ARRAYS)
!
      integer 
     $     ATOT, TTOP, NODIMA, NODIMT, NODIMP, 
     $     RANGEA, RANGET, RANGEP, ALIGN, DIST, MSTATUS
      logical LIVE MAPPING
!
