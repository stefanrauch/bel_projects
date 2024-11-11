#!/bin/sh
#set -x

# Absolute path to this script (with resolved symlinks)
SCRIPT=$(readlink -f "$0")
# Absolute path to script base directory  (with resolved symlinks)
SCRIPTDIR=$(dirname "${SCRIPT}")
# We assume that the script resides in bin/ folder inside the base folder, so going one level up
BASEDIR=$(cd "${SCRIPTDIR}/.." >/dev/null; pwd)

# script for starting the unichop system viewer on INT
export DIM_DNS_NODE=asl105
export LD_LIBRARY_PATH=${BASEDIR}/lib
xterm -T 'unilac chopper monitor' -fa monaco -fs 10 -geometry 164x24 -e ${BASEDIR}/bin/unichop-client-mon int
