/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


/**
 * Description: An extensible class inheriting from another: 
 *      Using self() in the second class before the concrete 
 *      class is created should be a failure. 
 */
#define OMR_EXTENSIBLE __attribute__((annotate("OMR_Extensible")))

namespace TR { class Class1Ext; } 
namespace OMR
{
   class OMR_EXTENSIBLE Class1Ext {
      public:
      TR::Class1Ext* self(); 
   };
}
/* Export to TR Namespace */ 
namespace TR  { class OMR_EXTENSIBLE Class1Ext : public OMR::Class1Ext {}; }

// Declare self function. 
TR::Class1Ext* OMR::Class1Ext::self() { return static_cast<TR::Class1Ext*>(this);  }


namespace TR { class Class3Ext; } 
namespace OMR
{
   class OMR_EXTENSIBLE Class3Ext : public TR::Class1Ext {
      public:
//      TR::Class3Ext* self();  // Missed self, means below Self call is an upcast!
      bool foo() {
         // Assignment here to ensure the linter has to see through assignment VarDecls to find the caller.
         bool x = (self() == (void*)0x124);
         return x;
      } 
   };
}

namespace TR  { class OMR_EXTENSIBLE Class3Ext : public OMR::Class3Ext {}; }


