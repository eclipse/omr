###############################################################################
#
# (c) Copyright IBM Corp. 2015, 2016
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

CONFIGURE_ARGS += \
  --enable-OMR_EXAMPLE \
  --enable-OMR_GC \
  --enable-OMR_PORT \
  --enable-OMR_THREAD \
  --enable-OMR_OMRSIG \
  --enable-fvtest \
  --enable-OMR_GC_SEGREGATED_HEAP \
  --enable-OMR_GC_MODRON_SCAVENGER \
  --enable-OMR_GC_MODRON_CONCURRENT_MARK \
  --enable-OMR_THR_CUSTOM_SPIN_OPTIONS \
  --enable-OMR_NOTIFY_POLICY_CONTROL
