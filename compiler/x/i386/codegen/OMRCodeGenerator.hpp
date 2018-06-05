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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMR_I386_CODEGENERATOR_INCL
#define OMR_I386_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace X86 { namespace I386 { class CodeGenerator; } } }
namespace OMR { typedef OMR::X86::I386::CodeGenerator CodeGeneratorConnector; }
#else
#error OMR::X86::I386::CodeGenerator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/x/codegen/OMRCodeGenerator.hpp"

#include "codegen/RegisterConstants.hpp"  // for TR_GlobalRegisterNumber

class TR_RegisterCandidate;
namespace TR { class Block; }
namespace TR { class Node; }
template <class T> class TR_LinkHead;

namespace OMR
{

namespace X86
{

namespace I386
{

class /*OMR_EXTENSIBLE*/ CodeGenerator : public OMR::X86::CodeGenerator
   {

   public:

   CodeGenerator();

   virtual TR::Register *longClobberEvaluate(TR::Node *node);

   TR_GlobalRegisterNumber pickRegister(TR_RegisterCandidate *, TR::Block * *, TR_BitVector &, TR_GlobalRegisterNumber &, TR_LinkHead<TR_RegisterCandidate> *candidates);
   int32_t getVMThreadGlobalRegisterNumber() {return 6;}

   using OMR::X86::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge;
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *);

   bool internalPointerSupportImplemented() {return true;}
   uint32_t getRegisterMapInfoBitsMask() {return 0x00ff0000;}

   bool codegenMulDecomposition(int64_t multiplier);

   };

} // namespace I386

} // namespace X86

} // namespace OMR

#endif
