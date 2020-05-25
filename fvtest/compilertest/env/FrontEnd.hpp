/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef TESTFE_INCL
#define TESTFE_INCL

#include <vector>
#include "compiler/env/FrontEnd.hpp"
#include "env/FEBase.hpp"
#include "env/jittypes.h"
#include "runtime/TestJitConfig.hpp"

namespace TR { class GCStackAtlas; }
namespace OMR { struct MethodMetaDataPOD; }
class TR_ResolvedMethod;

namespace TR
{

template <> struct FETraits<TestCompiler::FrontEnd>
   {
   typedef TestCompiler::JitConfig      JitConfig;
   static const size_t  DEFAULT_SEG_SIZE = (128 * 1024); // 128kb
   };

}

namespace TestCompiler
{

class FrontEnd : public TR::FEBase<FrontEnd>
   {
   private:
   static FrontEnd   *_instance; /* singleton */

   public:
   FrontEnd();
   static FrontEnd *instance()  { TR_ASSERT(_instance, "bad singleton"); return _instance; }

   virtual void reserveTrampolineIfNecessary(TR::Compilation *comp, TR::SymbolReference *symRef, bool inBinaryEncoding);

#if defined(TR_TARGET_S390)
   virtual void generateBinaryEncodingPrologue(TR_BinaryEncodingData *beData, TR::CodeGenerator *cg);
#endif

   virtual intptr_t methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference *symRef,  void *currentCodeCache);

  TR_ResolvedMethod * createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod,
                                            TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance);



   };

} // namespace TestCompiler

namespace TR
{
class FrontEnd : public TestCompiler::FrontEnd
   {
   public:
   FrontEnd();
   };

} // namespace TR

#endif
