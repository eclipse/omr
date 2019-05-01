/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef OMR_ILREFERENCE_INCL
#define OMR_ILREFERENCE_INCL

namespace TR { class SymbolReference; }

namespace OMR
{

class IlReference
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   /**
    * @brief really just a wrapper for a TR::SymbolReference
    */
   IlReference(TR::SymbolReference *symRef)
      : _symRef(symRef)
      {
      }

   /**
    * @brief returns the wrapped TR::SymbolReference
    */
   TR::SymbolReference *symRef()
      {
      return _symRef;
      }

protected:
   /**
    * @brief the wrapped TR::SymbolReference pointer
    */
   TR::SymbolReference * _symRef;
   };

} // namespace OMR

#endif // !defined(OMR_ILREFERENCE_INCL)
