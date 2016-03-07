#!/bin/sh
###############################################################################
#
# (c) Copyright IBM Corp. 2015
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
###############################################################################

OMRDIR=$1
echo "\
${OMRDIR}/lib/libomrvmstartup.a \
${OMRDIR}/lib/libj9omr.a \
${OMRDIR}/lib/libomrgcstartup.a \
${OMRDIR}/lib/libomrgcstandard.a \
${OMRDIR}/lib/libomrgcverbose.a \
${OMRDIR}/lib/libomrgcverbosehandlerstandard.a \
${OMRDIR}/lib/libomrgcbase.a \
${OMRDIR}/lib/libomrgcstats.a \
${OMRDIR}/lib/libomrgcstructs.a \
${OMRDIR}/lib/libj9hookstatic.a \
${OMRDIR}/lib/libj9thrstatic.a \
${OMRDIR}/lib/libj9prtstatic.a \
${OMRDIR}/lib/libomrtrace.a \
${OMRDIR}/lib/libj9hashtable.a \
${OMRDIR}/lib/libj9avl.a \
${OMRDIR}/lib/libj9pool.a \
${OMRDIR}/lib/libomrutil.a"
