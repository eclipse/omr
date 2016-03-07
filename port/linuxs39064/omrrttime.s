###############################################################################
#
# (c) Copyright IBM Corp. 2004
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
###############################################################################

.file "omrrttime.s"
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

