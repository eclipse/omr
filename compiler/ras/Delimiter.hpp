/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef TR_DELIMITER_INCL
#define TR_DELIMITER_INCL

#include <string.h>
#include "infra/Assert.hpp"

namespace TR { class Compilation; }

namespace TR
{

class Delimiter
   {
   public:

   Delimiter(
         TR::Compilation * comp,
         bool trace,
         char * tag,
         char * comment0 = NULL,
         char * comment1 = NULL,
         char * comment2 = NULL) :
      _tag(tag),
      _comp(comp),
      _trace(trace)
      {
      TR_ASSERT(strlen(tag) < tagsize-1, "tag is too long");

      if (_trace)
         {
         if (!comment0)
            traceMsg(_comp,"<%s>\n",_tag);
         else
            {
            if (!comment1)
               traceMsg(_comp, "<%s %s>\n", _tag, comment0);
            else
               {
               traceMsg(_comp, "<%s\n", _tag);
               traceMsg(_comp, "\t%s\n", comment0);
               traceMsg(_comp, "\t%s", comment1);
               if (comment2)
                  traceMsg(_comp, "\n\t%s>\n", comment2);
               else
                  traceMsg(_comp, ">\n");
               }
            }
         }
      }

   ~Delimiter()
      {
      if (_trace)
         traceMsg(_comp,"</%s>\n",_tag);
      }

   protected:

   static const int tagsize = 32;

   // pin an address is good enough
   // char _buffer[tagsize];
   char * _tag;

   TR::Compilation *_comp;

   bool _trace;
   };

}

#endif
