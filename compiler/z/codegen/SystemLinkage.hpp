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

#ifndef TR_SYSTEMLINKAGE_INCL
#define TR_SYSTEMLINKAGE_INCL

#include "codegen/S390SystemLinkage.hpp"

#include "codegen/Linkage.hpp"
#include "codegen/LinkageConventionsEnum.hpp"

namespace TR { class CodeGenerator; }

namespace TR
{

class SystemLinkage : public OMR::SystemLinkageConnector
   {
   public:

   SystemLinkage(TR::CodeGenerator * cg, TR_S390LinkageConventions elc=TR_S390LinkageDefault, TR_LinkageConventions lc=TR_System)
      : OMR::SystemLinkageConnector(cg, elc, lc) {}
   };
}

inline TR::SystemLinkage *
toSystemLinkage(TR::Linkage * l)
   {
   return (TR::SystemLinkage *) l;
   }

#endif
