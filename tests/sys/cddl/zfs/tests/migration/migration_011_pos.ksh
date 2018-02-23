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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)migration_011_pos.ksh	1.2	07/01/09 SMI"
#

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/migration/migration.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: migration_011_pos
#
# DESCRIPTION:
# Migrating test file from ZFS fs to UFS fs using cp
#
# STRATEGY:
# 1. Calculate chksum of testfile
# 2. CP up test file and place on a ZFS filesystem
# 3. Extract cp contents to a UFS file system
# 4. Calculate chksum of extracted file
# 5. Compare old and new chksums.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2005-07-04)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "both"

function cleanup
{
	$RM -rf $NONZFS_TESTDIR/cp${TESTCASE_ID}.cp
	$RM -rf $TESTDIR/$BNAME
}

log_assert "Migrating test file from ZFS fs to UFS fs using cp"

log_onexit cleanup

prepare $DNAME "$CP $BNAME $TESTDIR/cp${TESTCASE_ID}.cp"
(( $? != 0 )) && log_fail "Unable to create src archive"

migrate $NONZFS_TESTDIR $SUMA $SUMB "$CP $TESTDIR/cp${TESTCASE_ID}.cp $BNAME"
(( $? != 0 )) && log_fail "Uable to successfully migrate test file from" \
    "ZFS fs to UFS fs"

log_pass "Successully migrated test file from ZFS fs to UFS fs".
