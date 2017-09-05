/******************************************************************************* 
 *
 * (c) Copyright IBM Corp. 2000, 2017
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "runtime/OMRCodeMetaData.hpp"

#include "compile/Compilation.hpp"              // for Compilation, comp
#include "compile/ResolvedMethod.hpp"           // for TR_ResolvedMethod


TR::CodeMetaData *
OMR::CodeMetaData::self()
   {
   return static_cast<TR::CodeMetaData *>(this);
   }

OMR::CodeMetaData::CodeMetaData(TR::Compilation *comp)
   {
   _codeAllocStart = comp->cg()->getBinaryBufferStart();
   // TODO: Size is effectively getEstimatedWarmLength() since cold code length 
   // is always 0. Once we remove cold code, we should rename getEstimatedMethodLength()
   // to getEstimatedCodeLength() and use that here.
   _codeAllocSize = comp->cg()->getEstimatedMethodLength();

   _interpreterEntryPC = comp->cg()->getCodeStart();
   
   _compiledEntryPC = _interpreterEntryPC;
   _compiledEndPC = comp->cg()->getWarmCodeEnd();

   _hotness = comp->cg()->getMethodHotness();
   }

