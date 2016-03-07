###############################################################################
#
# (c) Copyright IBM Corp. 2013, 2014
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

# This is a zLinux helper that stores the STFLE bits
# into the passed memory reference.
#
# The helper returns the number of valid double-words in the STFLE
# bits, minus 1.
#
# The stfle instruction is encoded as 0xb2b00000 in binary, leaving
# it in as such so that we can compile on any platform.

.file "omrgetstfle.s"

.text
    .align  8
.globl getstfle
    .type   getstfle,@function
getstfle:
.text

  lr   %r0,%r2
.long 0xb2b03000
  lr   %r2,%r0
  br  %r14
