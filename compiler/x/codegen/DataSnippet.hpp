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

#ifndef IA32DATASNIPPET_INCL
#define IA32DATASNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stdint.h>                  // for uint8_t, int32_t, int64_t, etc

namespace TR { class CodeGenerator; }
namespace TR { class Node; }

namespace TR {

class IA32DataSnippet : public TR::Snippet
   {
   uint8_t _length;
   bool    _isClassAddress;
   protected:
   uint8_t _value[16];
   public:

   IA32DataSnippet(TR::CodeGenerator *cg, TR::Node *, void *c, uint8_t size);

   virtual Kind getKind() { return IsData; }
   uint8_t* getValue()  { return _value; }
   virtual uint8_t *emitSnippetBody();
   virtual uint8_t getDataSize() { return _length; }
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual bool setClassAddress(bool isClassAddress) { return _isClassAddress = isClassAddress;}

   void addMetaDataForCodeAddress(uint8_t *cursor);

   int32_t getDataAs2Bytes() { return *((int16_t *) &_value); }
   int32_t getDataAs4Bytes() { return *((int32_t *) &_value); }
   int64_t getDataAs8Bytes() { return *((int64_t *) &_value); }
   };

}

#endif
