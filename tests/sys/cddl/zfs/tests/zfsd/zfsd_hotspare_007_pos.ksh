#!/usr/local/bin/ksh93 -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

# $FreeBSD$

#
# Copyright 2012 Spectra Logic.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)hotspare_replace_006_pos.ksh	1.0	12/08/10 SL"
#
. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib
. $STF_SUITE/include/libgnop.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zfs_hotspare_007_pos
#
# DESCRIPTION: 
#	If a vdev gets removed from a pool with a spare while zfsd is shut
#	down, then the spare will be activated when zfsd restarts
#       
#
# STRATEGY:
#	1. Create 1 storage pools with hot spares.
#	2. Turn off zfsd
#	3. Remove one vdev
#	4. Restart zfsd
#	5. Verify that the spare is in use.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2014-09-17)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

log_assert "zfsd will spare missing drives on startup"


function verify_assertion # spare_dev
{
	typeset spare_dev=$1
	stop_zfsd

	log_must destroy_gnop $REMOVAL_DISK

	# Check to make sure ZFS sees the disk as removed
	wait_for_pool_removal 20

	restart_zfsd

	# Check that the spare was activated
	wait_for_pool_dev_state_change 20 $spare_dev INUSE

	# Re-enable the  missing disk
	log_must create_gnop $REMOVAL_DISK $PHYSPATH
}

typeset PHYSPATH="some_physical_path"
typeset REMOVAL_DISK=$DISK0
typeset REMOVAL_NOP=${DISK0}.nop
typeset SPARE_DISK=$DISK4
typeset SPARE_NOP=${DISK4}.nop
typeset OTHER_DISKS="${DISK1} ${DISK2} ${DISK3}"
typeset OTHER_NOPS=${OTHER_DISKS//~(E)([[:space:]]+|$)/.nop\1}
set -A MY_KEYWORDS "mirror" "raidz1" "raidz2"
ensure_zfsd_running
log_must create_gnops $OTHER_DISKS $SPARE_DISK
log_must create_gnop $REMOVAL_DISK $PHYSPATH
for keyword in "${MY_KEYWORDS[@]}" ; do
	log_must create_pool $TESTPOOL $keyword $REMOVAL_NOP $OTHER_NOPS spare $SPARE_NOP
	log_must $ZPOOL set autoreplace=on $TESTPOOL
	iterate_over_hotspares verify_assertion $SPARE_NOP

	destroy_pool "$TESTPOOL"
done

log_pass
