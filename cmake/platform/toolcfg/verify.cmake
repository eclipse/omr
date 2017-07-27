###############################################################################
#
# (c) Copyright IBM Corp. 2017, 2017
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

# Verify that all the definitions required from a toolcfg exist. 

# Error if a variable isn't defined. 
macro(omr_assert_defined var) 
   if(NOT DEFINED ${var}) 
      message(FATAL_ERROR "Missing variable definition ${var} from toolconfig ${OMR_TOOLCONFIG}")
   endif()
endmacro(omr_assert_defined)

# Warn if a variable isn't defined
macro(omr_warn_if_not_defined var) 
   if(NOT DEFINED ${var}) 
      message(WARNING "Missing variable definition ${var} from toolconfig ${OMR_TOOLCONFIG}")
   endif()
endmacro(omr_warn_if_not_defined)

omr_warn_if_not_defined("OMR_WARNING_AS_ERROR_FLAG")
