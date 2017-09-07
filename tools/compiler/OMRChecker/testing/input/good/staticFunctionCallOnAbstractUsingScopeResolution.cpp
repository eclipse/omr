/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/


/**
 * Description: Calls an extensible class static member function on
 *    an abstract class using scope resolution, which is allowed.
 */

#define OMR_EXTENSIBLE __attribute__((annotate("OMR_Extensible")))

namespace TR  { class ExtClass; }         // forward declaration required to declared `self()`

namespace OMR
{

class OMR_EXTENSIBLE ExtClass
   {
   public:
   TR::ExtClass * self();         // declaration of down cast function

   static void functionCalled();  // function to be called
   void callingFunction();        // function that will make call
                                  //   using scope resolution
   };

} // namespace OMR

namespace TR { class OMR_EXTENSIBLE ExtClass : public OMR::ExtClass {}; }

TR::ExtClass * OMR::ExtClass::self() { return static_cast<TR::ExtClass *>(this); }

void OMR::ExtClass::functionCalled() {}

void OMR::ExtClass::callingFunction() { OMR::ExtClass::functionCalled(); }
