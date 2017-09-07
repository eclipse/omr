/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef TR_ARM_INSTOPCODE_INCL
#define TR_ARM_INSTOPCODE_INCL



namespace TR
{

/*
 * TODO: this is temporary so that ARM instruction can be re-shaped first.
 * Once ARM instruction is done, then this needs to be done properly.
 */
class InstOpCode
   {
   public:

   enum Mnemonic
      {
      BAD
      };

   InstOpCode(); // TODO: this needs to be private later
   InstOpCode(Mnemonic m): _mnemonic(m)  {}

   Mnemonic getMnemonic() {return _mnemonic;}
   void setMnemonic(Mnemonic op) {_mnemonic = op;}

   private:

   Mnemonic _mnemonic;

   /* OMR TODO: Delegating method; need to be removed and replace with getMnemonic */
   //TR::InstOpCode::Mnemonic getOpCodeValue()                  { return _mnemonic;; }
   //TR::InstOpCode::Mnemonic setOpCodeValue(TR::InstOpCode::Mnemonic op) { return _mnemonic = op; }

   };
}

#endif
