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
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#ifndef OMR_X86_AHEADOFTIMECOMPILE_INCL
#define OMR_X86_AHEADOFTIMECOMPILE_INCL

#ifndef OMR_AHEADOFTIMECOMPILE_CONNECTOR
#define OMR_AHEADOFTIMECOMPILE_CONNECTOR

namespace OMR { namespace X86 {  class AheadOfTimeCompile; } }
namespace OMR { typedef OMR::X86::AheadOfTimeCompile AheadOfTimeCompileConnector; }

#endif // OMR_AHEADOFTIMECOMPILE_CONNECTOR

#include "infra/Annotations.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "compiler/codegen/OMRAheadOfTimeCompile.hpp"

namespace TR { class AheadOfTimeCompile; }

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE AheadOfTimeCompile  : public OMR::AheadOfTimeCompile
   {

public:
   AheadOfTimeCompile(uint32_t *_relocationKindToHeaderSizeMap,TR::Compilation* c)
      : OMR::AheadOfTimeCompile(_relocationKindToHeaderSizeMap,c){}

#ifdef OMR_RELOCATION_RUNTIME
   void processRelocations();
   uint8_t *initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation);
#endif
protected:
   TR::AheadOfTimeCompile* self();
private:
   static uint32_t _relocationKindToHeaderSizeMap[TR_NumExternalRelocationKinds];
   };

} // namespace X86

} // namespace OMR

#endif // ifndef OMR_X86_AHEADOFTIMECOMPILE_INCL
