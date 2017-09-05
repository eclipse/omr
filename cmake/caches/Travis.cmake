###############################################################################
#
# (c) Copyright IBM Corp. 2017
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

set(OMR_DDR ON CACHE BOOL "Enable DDR")
set(OMR_EXAMPLE ON CACHE BOOL "")
set(OMR_JIT  ON CACHE BOOL "")
set(OMR_JITBUILDER ON CACHE BOOL "")
set(OMR_GC ON CACHE BOOL "")
set(OMR_PORT ON CACHE BOOL "")
set(OMR_THREAD ON CACHE BOOL "")
set(OMR_TEST_COMPILER ON CACHE BOOL "")
set(OMR_OMRSIG ON CACHE BOOL "")
set(OMR_FVTEST ON CACHE BOOL "")
set(OMR_GLUE ${CMAKE_SOURCE_DIR}/example/glue)
set(OMR_GC_SEGREGATED_HEAP ON CACHE BOOL "")
set(OMR_GC_MODRON_SCAVENGER ON CACHE BOOL "")
set(OMR_GC_MODRON_STANDARD ON CACHE BOOL "")
set(OMR_GC_MODRON_CONCURRENT_MARK ON CACHE BOOL "")
set(OMR_THR_CUSTOM_SPIN_OPTIONS ON CACHE BOOL "")
set(OMR_NOTIFY_POLICY_CONTROL ON CACHE BOOL "")
set(OMR_THR_SPIN_WAKE_CONTROL ON CACHE BOOL "")
set(OMR_THR_SPIN_CODE_REFACTOR ON CACHE BOOL "")
set(OMR_THR_FORK_SUPPORT ON CACHE BOOL "")
set(OMR_WARNINGS_AS_ERRORS OFF CACHE BOOL "")
