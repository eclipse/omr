###############################################################################
# Copyright (c) 2004, 2004 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
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

.file "omrrttime.s"
.text
    .align  8
.globl maxprec
    .type   maxprec,@function
maxprec:
.text
    larl    %r3,.LT0
    
    stckf 64(%r15)
    lg  %r1,64(%r15)
    sg %r1,.LC0-.LT0(%r3)
    srlg %r1,%r1,1
    lgr %r2,%r1
    br  %r14

    .align  8
.LT0:
.LC0:
    .quad   0x7D91048BCA000000
.Lfe1:
    .size   maxprec,.Lfe1-maxprec
    .ident  "maximum precision clock" 

