################################################################################
#
# (c) Copyright IBM Corp. 1991, 2013
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
#    Multiple authors (IBM Corp.) - initial implementation and documentation
################################################################################

.global getTOC
.type getTOC@function

.section ".text"
.align 4

getTOC:
        addis 2,12,.TOC.-getTOC@ha
        addis 2,2,.TOC.-getTOC@l
.localentry getTOC,.-getTOC
        or 3,2,2
        blr
