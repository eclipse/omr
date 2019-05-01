/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef X86DATASNIPPET_INCL
#define X86DATASNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "infra/vector.hpp"
#include <stdint.h>

namespace TR { class CodeGenerator; }
namespace TR { class Node; }

namespace TR {

class X86DataSnippet : public TR::Snippet
   {
   public:

   X86DataSnippet(TR::CodeGenerator *cg, TR::Node *, void *c, size_t size);

   virtual Kind                   getKind()                                { return IsData; }
   uint8_t*                       getRawData()                             { return _data.data(); }
   virtual size_t                 getDataSize() const                      { return _data.size(); }
   virtual uint32_t               getLength(int32_t estimatedSnippetStart) { return getDataSize(); }
   virtual bool                   setClassAddress(bool isClassAddress)     { return _isClassAddress = isClassAddress;}
   template <typename T> inline T getData()                                { return *((T*)getRawData()); }

   virtual uint8_t*               emitSnippetBody();
   virtual void                   print(TR::FILE* pOutFile, TR_Debug* debug);
   virtual void                   printValue(TR::FILE* pOutFile, TR_Debug* debug);
   void                           addMetaDataForCodeAddress(uint8_t *cursor);

   private:
   bool                _isClassAddress;
   TR::vector<uint8_t> _data;
   };

}

#endif
