/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "Action.hpp"

namespace OMR
{
namespace JitBuilder
{

std::string actionName[] =
   {
     "None"
   , "ConstInt8"
   , "ConstInt16"
   , "ConstInt32"
   , "ConstInt64"
   , "ConstFloat"
   , "ConstDouble"
   , "ConstAddress"
   , "CoercePointer"
   , "Add"
   , "Sub"
   , "Mul"
   , "Load"
   , "LoadAt"
   , "LoadField"
   , "LoadIndirect"
   , "Store"
   , "StoreAt"
   , "StoreField"
   , "StoreIndirect"
   , "IndexAt"
   , "AppendBuilder"
   , "Call"
   , "Goto"
   , "Return"
   , "IfCmpGreaterThan"
   , "IfCmpLessThan"
   , "IfCmpGreaterOrEqual"
   , "IfCmpLessOrEqual"
   , "IfThenElse"
   , "Switch"
   , "ForLoop"
   , "CreateLocalArray"
   , "CreateLocalStruct"

   // New action names, order must correspond to Action enum order in Action.hpp
   // BEGIN {
   //

   //
   // } END
   // New action names, order must correspond to Action enum order in Action.hpp

   };

static_assert(sizeof(actionName)/sizeof(*actionName) == NumActions, "Missing/extra action name");

} // namespace JitBuilder
} // namespace OMR
