/*******************************************************************************
 * Copyright (c) 2000 IBM Corp. and others
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
   /**
    * @brief This function is X86 architecture-specific extension of the function in
    * OMR::AheadOfTimeCompile. It goes through the list of iterated external relocations,
    * and computes their sizes. Later, it initializes binary templates that are suitable for
    * serialization in persistent storage through using RelocationGroup class API.
    * Furthermore, it calls to populate each binary template. by calling TR::IteratedExternalRelocation::initializeRelocation.
    * For additional description of the process, refer to
    * https://blog.openj9.org/2018/10/26/ahead-of-time-compilation-relocation/ series of blog posts,
    * as well as in a position paper:
    * Georgiy Krylov, Gerhard W. Dueck, Kenneth B. Kent, Daryl Maier, and Irwin D'Souza.
    * 2019. Ahead-of-time compilation in OMR: overview and first steps. In Proceedings of
    * the 29th Annual International Conference on Computer Science and Software Engineering
    * (CASCON '19). IBM Corp., USA, 299–304
    */
   void processRelocations();

   /**
    * @brief This function is X86 architecture-specific extension of the function in
    * OMR::OMRAheadOfTimeCompile. It is supposed to populate specific BinaryRelocationsTemplates
    * depending on the relocations kind. It is called from processRelocations through
    * TR::IteratedExternalRelocation::initializeRelocation (the call is located in Relocation.cpp).
    * The function first, populates some common attributes, and later, by switching on
    * relocation kind, populates specific RelocationBinaryTemplate fields.
    * The code could also call to OMR::AheadOfTimeCompile::initializeCommonRelocationHeader,
    * that does initialization of relocations that are not architecture-dependent.
    * @param relocation - pointer to the BinaryTemplate created in processRelocations
    * @return uint8_t* - the size of a fully populated relocation template that is used to advance the cursor in
    * the buffer of relocation binary templates.
    */
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
