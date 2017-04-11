/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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
 ******************************************************************************/


#ifndef IlGen_hpp
#define IlGen_hpp

#include <stdint.h>  // for int32_t

namespace TR { class Block; }
namespace TR { class ResolvedMethodSymbol;  } 

class TR_IlGenerator
   {
   public:
   virtual bool genIL() = 0; 
   virtual int32_t currentByteCodeIndex() = 0;
   virtual TR::Block *getCurrentBlock() = 0;
   virtual int32_t currentCallSiteIndex() { return -1; }
   virtual void setCallerMethod(TR::ResolvedMethodSymbol * caller) {}
   virtual TR::ResolvedMethodSymbol *methodSymbol() const = 0;

   // contributes to eliminate warnings in JitBuilder builds
   virtual ~TR_IlGenerator() { }
   };

#endif
