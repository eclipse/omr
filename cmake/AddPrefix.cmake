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

# Given a prefix, and a list of arguments, prefix the list of arguments and
# assign to out: ie, add_prefix(out "-I" "a;b;c") should set out to
# "-Ia;-Ib;-Ic".
function(add_prefix out prefix) 
   set(ret "")
   foreach(var IN ITEMS ${ARGN}) 
      list(APPEND ret "${prefix}${var}")
   endforeach()
   set(${out} ${ret} PARENT_SCOPE)
endfunction(add_prefix) 

