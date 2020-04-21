/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/CodeGenerator.hpp"
#include "env/ConcreteFE.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/SystemLinkage.hpp"
#include "codegen/S390Snippets.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/IO.hpp"

uint32_t
TR::S390RestoreGPR7Snippet::getLength(int32_t estimatedSnippetStart)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

uint8_t *
TR::S390RestoreGPR7Snippet::emitSnippetBody()
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390RestoreGPR7Snippet *snippet)
   {
   TR_UNIMPLEMENTED();
   }

void
JitBuilder::FrontEnd::generateBinaryEncodingPrologue(
      TR_BinaryEncodingData *beData,
      TR::CodeGenerator *cg)
   {
   TR::Compilation* comp = cg->comp();
   TR_S390BinaryEncodingData *data = (TR_S390BinaryEncodingData *)beData;

   data->cursorInstruction = cg->getFirstInstruction();
   data->estimate = 0;
   data->preProcInstruction = data->cursorInstruction;
   data->jitTojitStart = data->cursorInstruction;
   data->cursorInstruction = NULL;

   TR::Instruction * preLoadArgs, * endLoadArgs;
   preLoadArgs = data->preProcInstruction;
   endLoadArgs = preLoadArgs;

   TR::Instruction * oldFirstInstruction = data->cursorInstruction;

   data->cursorInstruction = cg->getFirstInstruction();

   static char *disableAlignJITEP = feGetEnv("TR_DisableAlignJITEP");

   // Padding for JIT Entry Point
   if (!disableAlignJITEP)
      {
      data->estimate += 256;
      }

   while (data->cursorInstruction && data->cursorInstruction->getOpCodeValue() != TR::InstOpCode::PROC)
      {
      data->estimate = data->cursorInstruction->estimateBinaryLength(data->estimate);
      data->cursorInstruction = data->cursorInstruction->getNext();
      }

   cg->getLinkage()->createPrologue(data->cursorInstruction);
   }


