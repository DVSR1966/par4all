#! /bin/sh
#
# $Id$
#
# Find out actual *pips for the current $PIPS_ARCH...
# It is always better to use the executable directly. 
#
# This shell script is expected to be executer as pips/tpips or wpips.
# This can be achieve by providing such links in the Share directory.
#

what=`basename $0 .sh`

error()
{
  status=$1
  shift
  echo "error: $@" >&2
  exit ${status}
}

[ "${PIPS_ARCH}" ] || error 1 "\$PIPS_ARCH is undefined!"
[ "${PIPS_ROOT}" ] || error 2 "\$PIPS_ROOT is undefined!"

PATH=./${PIPS_ARCH}:${PIPS_ROOT}/bin/${PIPS_ARCH}:${PATH}

# how to avoid a recursion of no actual binary is found:
PATH=./${PIPS_ARCH}:${PIPS_ROOT}/bin/${PIPS_ARCH} \
    type ${what} > /dev/null || error 3 "no ${what} binary found!"

exec "${what}" "$@"
