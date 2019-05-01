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

#ifndef ANALYSER_INCL
#define ANALYSER_INCL

#define NUM_ACTIONS 64

#include <stddef.h>
#include <stdint.h>

#define Clob2 0x01
#define Mem2  0x02
#define Reg2  0x04
#define Clob1 0x08
#define Mem1  0x10
#define Reg1  0x20

namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class Register; }

class TR_Analyser
   {
   uint8_t _inputs;

   public:

   TR_Analyser() : _inputs(0) {};

   void setInputs(TR::Node     *firstChild,
                  TR::Register *firstRegister,
                  TR::Node     *secondChild,
                  TR::Register *secondRegister,
                  bool         nonClobberingDestination = false,
                  bool         dontClobberAnything = false,
                  TR::Compilation *comp = NULL,
                  bool         lockedIntoRegister1  = false,
                  bool         lockedIntoRegister2  = false);

   void resetReg1()  {_inputs &= ~Reg1;}
   void resetReg2()  {_inputs &= ~Reg2;}
   void resetMem1()  {_inputs &= ~Mem1;}
   void resetMem2()  {_inputs &= ~Mem2;}
   void resetClob1() {_inputs &= ~Clob1;}
   void resetClob2() {_inputs &= ~Clob2;}
   void setClob1()   {_inputs |=  Clob1;}
   void setClob2()   {_inputs |=  Clob2;}
   void setReg1()    {_inputs |=  Reg1;}
   void setReg2()    {_inputs |=  Reg2;}
   void setMem1()    {_inputs |=  Mem1;}
   void setMem2()    {_inputs |=  Mem2;}
   uint8_t getMem1()    { return _inputs &  Mem1;}
   uint8_t getMem2()    { return _inputs &  Mem2;}


   uint8_t getInputs() {return _inputs;}

   };

#endif
