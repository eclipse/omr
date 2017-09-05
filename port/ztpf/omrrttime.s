###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code is also Distributed under one or more Secondary Licenses,
# as those terms are defined by the Eclipse Public License, v. 2.0: GNU
# General Public License, version 2 with the GNU Classpath Exception [1]
# and GNU General Public License, version 2 with the OpenJDK Assembly
# Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# Contributors:
#   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
#   Multiple authors (IBM Corp.) - z/TPF platform initial port to OMR environment
###############################################################################
.file "j9rttime.s"
.text
    .align  8
.globl maxprec
    .type   maxprec,@function
maxprec:
.text
    larl    %r3,.LT0
    
    stck 64(%r15)
    lg  %r1,64(%r15)
    sg %r1,.LC0-.LT0(%r3)
    srlg %r1,%r1,9
    lgr %r2,%r1
    br  %r14

    .align  8
.LT0:
.LC0:
    .quad   0x7D91048BCA000000
.Lfe1:
    .size   maxprec,.Lfe1-maxprec
    .ident  "maximum precision clock" 

