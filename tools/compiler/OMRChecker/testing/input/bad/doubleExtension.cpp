/*******************************************************************************
*
* (c) Copyright IBM Corp. 2017, 2017
*
*  This program and the accompanying materials are made available
*  under the terms of the Eclipse Public License v1.0 and
*  Apache License v2.0 which accompanies this distribution.
*
*      The Eclipse Public License is available at
*      http://www.eclipse.org/legal/epl-v10.html
*
*      The Apache License v2.0 is available at
*      http://www.opensource.org/licenses/apache2.0.php
*
* Contributors:
*    Multiple authors (IBM Corp.) - initial implementation and documentation
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


