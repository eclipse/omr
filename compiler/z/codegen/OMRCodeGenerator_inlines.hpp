/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef OMR_Z_CODEGENERATOR_INLINES_INCL
#define OMR_Z_CODEGENERATOR_INLINES_INCL

#include "compiler/codegen/OMRCodeGenerator_inlines.hpp"
#include "codegen/OMRCodeGenerator.hpp"

template <class TR_AliasSetInterface>
bool OMR::Z::CodeGenerator::loadAndStoreMayOverlap(::TR::Node *store, size_t storeSize, ::TR::Node *load, size_t loadSize, TR_AliasSetInterface &storeAliases)
   {
   return storeAliases.contains(load->getSymbolReference(), self()->comp());
   }

#endif // OMR_Z_CODEGENERATOR_INLINES_INCL
