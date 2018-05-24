/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#ifndef OMRELFRELOCATIONRESOLVER_HPP
#define OMRELFRELOCATIONRESOLVER_HPP

#pragma once

#if defined(LINUX)

#ifndef OMR_ELF_RELOCATION_RESOLVER_CONNECTOR
#define OMR_ELF_RELOCATION_RESOLVER_CONNECTOR

namespace OMR { namespace X86 { namespace AMD64 { class ELFRelocationResolver; } } }
namespace OMR { typedef ::OMR::X86::AMD64::ELFRelocationResolver ELFRelocationResolverConnector; }

#endif /* OMR_ELF_RELOCATION_RESOLVER_CONNECTOR */

#include "compiler/codegen/OMRELFRelocationResolver.hpp"

namespace OMR {
namespace X86 {
namespace AMD64 {

/**
 * @brief The ELFRelocationResolver class translates between a TR::StaticRelocation and the ELF relocation type required for the platform.
 */
class /*OMR_EXTENSIBLE*/ ELFRelocationResolver : public ::OMR::ELFRelocationResolver
   {
public:

   /**
    * @brief resolveRelocationType translates between a TR::StaticRelocation and the ELF relocation type required for the platform.
    * @param relocation The relocation object containing the information that requires translation.
    * @return The ELF relocation type appropriate for the platform
    */
   uint32_t resolveRelocationType(const TR::StaticRelocation &relocation);

private:
   };

}
}
}

#endif /* defined(LINUX) */

#endif // OMRELFRELOCATIONRESOLVER_HPP
