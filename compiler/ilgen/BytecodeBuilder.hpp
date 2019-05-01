/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef TR_BYTECODEBUILDER_INCL
#define TR_BYTECODEBUILDER_INCL

#include "ilgen/OMRBytecodeBuilder.hpp"

namespace TR
{
   class BytecodeBuilder : public OMR::BytecodeBuilder
      {
      public:
         BytecodeBuilder(TR::MethodBuilder *methodBuilder, int32_t bcIndex, char *name=NULL, int32_t bcLength=-1)
            : OMR::BytecodeBuilder(methodBuilder, bcIndex, name, bcLength)
            { }
         void initialize(TR::IlGeneratorMethodDetails * details,
                           TR::ResolvedMethodSymbol     * methodSymbol,
                           TR::FrontEnd                 * fe,
                           TR::SymbolReferenceTable     * symRefTab); 
      };

} // namespace TR

#endif // !defined(TR_BYTECODEBUILDER_INCL)
