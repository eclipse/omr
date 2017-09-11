###############################################################################
# Copyright (c) 2013, 2017 IBM Corp. and others
# 
# This program and the accompanying materials are made available 
# under the terms of the Eclipse Public License 2.0 which accompanies 
# this distribution and is available at  
# https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
# Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath 
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################
# This is a zLinux helper that stores the STFLE bits
# into the passed memory reference.
#
# The helper returns the number of valid double-words in the STFLE
# bits, minus 1.
#
# The stfle instruction is encoded as 0xb2b00000 in binary, leaving
# it in as such so that we can compile on any platform.
# ===================================================================

.file "j9loadfpc.s"

.text
    .align  8
.globl loadfpc
    .type   loadfpc,@function
loadfpc:
.text
	lgf %r2,1300
	aghi %r2,8212
    larl %r3,.LT0
    mvc 0(4,%r2),.LC0-.LT0(%r3)
    lfpc  .LC0-.LT0(%r3)
    br  %r14

    .align  8
.LT0:
.LC0:
    .long   0x00400000
 
