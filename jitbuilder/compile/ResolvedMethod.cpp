/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"

// needs major overhaul
JitBuilder::ResolvedMethod::ResolvedMethod(TR_OpaqueMethodBlock *method)
   {
   // trouble! trouble! where do we get TypeDictionary from now?
   _ilInjector = reinterpret_cast<TR::IlInjector *>(method);

   TR::ResolvedMethod * resolvedMethod = (TR::ResolvedMethod *)_ilInjector->methodSymbol()->getResolvedMethod();
   _fileName = resolvedMethod->classNameChars();
   _name = resolvedMethod->nameChars();
   _numParms = resolvedMethod->getNumArgs();
   _parmTypes = resolvedMethod->_parmTypes;
   _lineNumber = resolvedMethod->getLineNumber();
   _returnType = resolvedMethod->returnIlType();
   _signature = resolvedMethod->getSignature();
   _externalName = 0;
   _entryPoint = resolvedMethod->getEntryPoint();
   strncpy(_signatureChars, resolvedMethod->signatureChars(), 62); // TODO: introduce concept of robustness
   }

JitBuilder::ResolvedMethod::ResolvedMethod(TR::MethodBuilder *m)
   : _fileName(m->getDefiningFile()),
     _lineNumber(m->getDefiningLine()),
     _name((char *)m->GetMethodName()), // sad cast
     _numParms(m->getNumParameters()),
     _parmTypes(m->getParameterTypes()),
     _returnType(m->getReturnType()),
     _entryPoint(0),
     _signature(0),
     _externalName(0),
     _ilInjector(static_cast<TR::IlInjector *>(m))
   {
   computeSignatureChars();
   }

const char *
JitBuilder::ResolvedMethod::signature(TR_Memory * trMemory, TR_AllocationKind allocKind)
   {
   if( !_signature )
      {
      char * s = (char *)trMemory->allocateMemory(strlen(_fileName) + 1 + strlen(_lineNumber) + 1 + strlen(_name) + 1, allocKind);
      sprintf(s, "%s:%s:%s", _fileName, _lineNumber, _name);

      if ( allocKind == heapAlloc)
        _signature = s;

      return s;
      }
   else
      return _signature;
   }

const char *
JitBuilder::ResolvedMethod::externalName(TR_Memory *trMemory, TR_AllocationKind allocKind)
   {
   if( !_externalName)
      {
      // For C++, need to mangle name
      //char * s = (char *)trMemory->allocateMemory(1 + strlen(_name) + 1, allocKind);
      //sprintf(s, "_Z%d%si", (int32_t)strlen(_name), _name);


      // functions must be defined as extern "C"
      _externalName = _name;

      //if ( allocKind == heapAlloc)
      //  _externalName = s;
      }

   return _externalName;
   }

TR::DataType
JitBuilder::ResolvedMethod::parmType(uint32_t slot)
   {
   TR_ASSERT((slot < _numParms), "Invalid slot provided for Parameter Type");
   return _parmTypes[slot]->getPrimitiveType();
   }

void
JitBuilder::ResolvedMethod::computeSignatureChars()
   {
   char *name=NULL;
   uint32_t len=3;
   for (int32_t p=0;p < _numParms;p++)
      {
      TR::IlType *type = _parmTypes[p];
      len += strlen(type->getSignatureName());
      }
   len += strlen(_returnType->getSignatureName());
   TR_ASSERT(len < 64, "signature array may not be large enough"); // TODO: robustness

   int32_t s = 0;
   _signatureChars[s++] = '(';
   for (int32_t p=0;p < _numParms;p++)
      {
      name = _parmTypes[p]->getSignatureName();
      len = strlen(name);
      strncpy(_signatureChars+s, name, len);
      s += len;
      }
   _signatureChars[s++] = ')';
   name = _returnType->getSignatureName();
   len = strlen(name);
   strncpy(_signatureChars+s, name, len);
   s += len;
   _signatureChars[s++] = 0;
   }


char *
JitBuilder::ResolvedMethod::localName(uint32_t slot,
                          uint32_t bcIndex,
                          int32_t &nameLength,
                          TR_Memory *trMemory)
   {
   char *name=NULL;
   if (_ilInjector != NULL && _ilInjector->isMethodBuilder())
      {
      TR::MethodBuilder *bldr = _ilInjector->asMethodBuilder();
      name = (char *) bldr->getSymbolName(slot);
      if (name == NULL)
         {
         name = "";
         }
      }
   else
      {
      name = (char *) trMemory->allocateHeapMemory(8 * sizeof(char));
      sprintf(name, "Parm %2d", slot);
      }

   nameLength = strlen(name);
   return name;
   }

TR::IlInjector *
JitBuilder::ResolvedMethod::getInjector (TR::IlGeneratorMethodDetails * details,
   TR::ResolvedMethodSymbol *methodSymbol,
   TR::FrontEnd *fe,
   TR::SymbolReferenceTable *symRefTab)
   {
   _ilInjector->initialize(details, methodSymbol, fe, symRefTab);
   return _ilInjector;
   }

TR::DataType
JitBuilder::ResolvedMethod::returnType()
   {
   return _returnType->getPrimitiveType();
   }

char *
JitBuilder::ResolvedMethod::getParameterTypeSignature(int32_t parmIndex)
   {
   return _parmTypes[parmIndex]->getSignatureName();
   }

