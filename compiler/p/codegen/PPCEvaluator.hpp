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

#ifndef PPC_TREE_EVALUATOR_INCL
#define PPC_TREE_EVALUATOR_INCL


#include <stddef.h>                         // for NULL
#include <stdint.h>                         // for int32_t
#include "codegen/TreeEvaluator.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class Register; }



void simplifyANDRegImm(TR::Node *, TR::Register *trgReg, TR::Register *srcReg, int32_t value, TR::CodeGenerator *cg, TR::Node *constNode=NULL);

#define VOID_BODY
#define NULL_BODY
#define BOOL_BODY

class TR_PPCComputeCC : public TR::TreeEvaluator
   {
   public:
   static bool setCarryBorrow(TR::Node *flagNode, bool invertValue, TR::Register **flagReg, TR::CodeGenerator *cg) BOOL_BODY;
   };

#undef NULL_BODY
#undef VOID_BODY

#endif
