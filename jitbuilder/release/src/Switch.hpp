/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
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


#ifndef SWITCH_INCL
#define SWITCH_INCL

#include <string>
#include "ilgen/MethodBuilder.hpp"

namespace TR { class TypeDictionary; }

typedef void (SwitchFunctionType)(int32_t);

class SwitchMethod : public TR::MethodBuilder
   {
   private:
   void PrintString(TR::IlBuilder *bldr, const char *s);

   public:
   SwitchMethod(TR::TypeDictionary *);
   virtual bool buildIL();

   virtual bool RequestFunction(const std::string &name);
   };

#endif // !defined(SWITCH_INCL)
