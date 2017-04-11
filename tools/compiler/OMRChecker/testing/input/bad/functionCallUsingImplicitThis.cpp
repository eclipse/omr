/*******************************************************************************
*
* (c) Copyright IBM Corp. 2016, 2016
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

/*
 * Description: Calls an extensible class member function using the
 *    implicit `this` pointer, which is not allowed.
*/

#define OMR_EXTENSIBLE __attribute__((annotate("OMR_Extensible")))

namespace OMR
{

class OMR_EXTENSIBLE ExtClass
   {
   public:
   void functionCalled();   // function to be called
   void callingFunction();  // function that will make call
                            //    with implicit `this 
   };

} // namespace OMR

namespace TR
{ 
   
class OMR_EXTENSIBLE ExtClass : public OMR::ExtClass
   {


   };

}

void OMR::ExtClass::functionCalled() {}

void OMR::ExtClass::callingFunction() { functionCalled(); }
