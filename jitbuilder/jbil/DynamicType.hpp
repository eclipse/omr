/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef DYNAMICTYPE_INCL
#define DYNAMICTYPE_INCL

#include <stdint.h>
#include <stdarg.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>

#include "DynamicOperation.hpp"
#include "Type.hpp"

namespace OMR
{

namespace JitBuilder
{

class TextWriter;
class TypeDictionary;
class TypeGraph;

typedef LiteralMapper *(LiteralExploder)(TypeDictionary *dict, LiteralValue *value, LiteralMapper *m);

typedef void (TypeRegistrar)(DynamicType *type, TypeDictionary *dict, TypeGraph *graph);

class DynamicType : public Type
   {
public:
   static DynamicType * create(TypeDictionary *dict, std::string name, size_t size, ValuePrinter *printer, StructType *layout, LiteralExploder *exploder, OperationExpander *expander, TypeRegistrar *registrar)
      {
      return new DynamicType(dict, name, size, printer, layout, exploder, expander, registrar);
      }

   virtual bool isDynamic() const { return true; }

   virtual StructType *layout() const { return _layout; }
   virtual LiteralMapper *explode(LiteralValue *value, LiteralMapper *m=NULL) const { return _exploder(_dict, value, m); }
   virtual bool expand(OperationReplacer *replacer) const;
   virtual void initializeTypeProductions(TypeDictionary *dict, TypeGraph *graph);

protected:
   DynamicType(TypeDictionary *dict, std::string name, size_t size, ValuePrinter *printer, StructType *layout, LiteralExploder *exploder, OperationExpander *expander, TypeRegistrar *registrar);

   StructType        * _layout;
   LiteralExploder   * _exploder;
   OperationExpander * _expander;
   TypeRegistrar     * _registrar;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(DYNAMICTYPE_INCL)