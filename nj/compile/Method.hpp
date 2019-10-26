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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH
 *Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef NJ_METHOD_INCL
#define NJ_METHOD_INCL

#ifndef TR_RESOLVEDMETHOD_COMPOSED
#define TR_RESOLVEDMETHOD_COMPOSED
#define PUT_NJ_RESOLVEDMETHOD_INTO_TR
#endif // TR_RESOLVEDMETHOD_COMPOSED

#include <string.h>

#include "compiler/compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"

namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlGenerator; }
namespace TR { class FrontEnd; }

// NJ Minimal Method definition
// For use by C api

namespace NJCompiler {

class Method : public TR::Method {
public:
  TR_ALLOC(TR_Memory::Method);

  Method() : TR::Method(TR::Method::Test) {}

  // FIXME: need to provide real code for this group
  virtual uint16_t classNameLength() { return strlen(classNameChars()); }
  virtual uint16_t nameLength() { return strlen(nameChars()); }
  virtual uint16_t signatureLength() { return strlen(signatureChars()); }
  virtual char *nameChars() { return "Method"; }
  virtual char *classNameChars() { return ""; }
  virtual char *signatureChars() { return "()V"; }

  virtual bool isConstructor() { return false; }
  virtual bool isFinalInObject() { return false; }
};

class ResolvedMethodBase : public TR_ResolvedMethod {
  virtual uint16_t nameLength() { return signatureLength(); }
  virtual uint16_t classNameLength() { return signatureLength(); }
  virtual uint16_t signatureLength() { return strlen(signatureChars()); }

  // This group of functions only make sense for Java - we ought to provide
  // answers from that definition
  virtual bool isConstructor() { return false; }
  virtual bool isNonEmptyObjectConstructor() { return false; }
  virtual bool isFinalInObject() { return false; }
  virtual bool isStatic() { return true; }
  virtual bool isAbstract() { return false; }
  virtual bool isCompilable(TR_Memory *) { return true; }
  virtual bool isNative() { return false; }
  virtual bool isSynchronized() { return false; }
  virtual bool isPrivate() { return false; }
  virtual bool isProtected() { return false; }
  virtual bool isPublic() { return true; }
  virtual bool isFinal() { return false; }
  virtual bool isStrictFP() { return false; }
  virtual bool isSubjectToPhaseChange(TR::Compilation *comp) { return false; }

  // FIXME is this correct?
  virtual bool hasBackwardBranches() { return false; }

  virtual bool isNewInstanceImplThunk() { return false; }
  virtual bool isJNINative() { return false; }
  virtual bool isJITInternalNative() { return false; }

  uint32_t numberOfExceptionHandlers() { return 0; }

  virtual bool isSameMethod(TR_ResolvedMethod *other) {
    return getPersistentIdentifier() == other->getPersistentIdentifier();
  }
};

class ResolvedMethod : public ResolvedMethodBase, public Method {
public:
  ResolvedMethod(TR_OpaqueMethodBlock *method);
  ResolvedMethod(
    const char *fileName, 
	  const char *lineNumber, 
	  const char *name,
	  int32_t numParms, 
	  TR::DataType *parmTypes,
	  TR::DataType returnType, 
	  void *entryPoint,
	  TR::IlGenerator *ilInjector)
      : _fileName(fileName),
        _lineNumber(lineNumber),
        _name((char*)name),
        _signature(0),
        _numParms(numParms),
        _parmTypes(parmTypes),
        _returnType(returnType),
        _entryPoint(entryPoint),
        _ilInjector(ilInjector)
      {
      computeSignatureChars();
      }

   virtual TR::Method           * convertToMethod()                          { return this; }

   virtual const char          * signature(TR_Memory *, TR_AllocationKind);
   virtual const char          * externalName(TR_Memory *, TR_AllocationKind);
   char                        * localName (uint32_t slot, uint32_t bcIndex, int32_t &nameLength, TR_Memory *trMemory);

   virtual char                * classNameChars()                           { return (char *)_fileName; }
   virtual char                * nameChars()                                { return _name; }
   virtual char                * signatureChars()                           { return _signatureChars; }
   virtual uint16_t              signatureLength()                          { return strlen(signatureChars()); }

   virtual void                * resolvedMethodAddress()                    { return (void *)_ilInjector; }

   virtual uint16_t              numberOfParameterSlots()                   { return _numParms; }
   virtual TR::DataType         parmType(uint32_t slot);
   virtual uint16_t              numberOfTemps()                            { return 0; }

   virtual void                * startAddressForJittedMethod()              { return (getEntryPoint()); }
   virtual void                * startAddressForInterpreterOfJittedMethod() { return nullptr; }

   virtual uint32_t              maxBytecodeIndex()                         { return 0; }
   virtual uint8_t             * code()                                     { return nullptr; }
   virtual TR_OpaqueMethodBlock* getPersistentIdentifier()                  { return (TR_OpaqueMethodBlock *) _ilInjector; }
   virtual bool                  isInterpreted()                            { return startAddressForJittedMethod() == 0; }

   const char                  * getLineNumber()                            { return _lineNumber;}
   char                        * getSignature()                             { return _signature;}
   TR::DataType                  returnType()                               { return _returnType; }
   int32_t                       getNumArgs()                               { return _numParms;}
   void                          setEntryPoint(void *ep)                    { _entryPoint = ep; }
   void                        * getEntryPoint()                            { return _entryPoint; }

  void computeSignatureChars();
  virtual void makeParameterList(TR::ResolvedMethodSymbol *);

  TR::IlGenerator *getInjector(
	  TR::IlGeneratorMethodDetails * details,
	  TR::ResolvedMethodSymbol *methodSymbol,
	  TR::FrontEnd *fe,
	  TR::SymbolReferenceTable *symRefTab);

protected:
   const char *_fileName;
   const char *_lineNumber;
   char *_name;
   char *_signature;
   char  _signatureChars[64];
   char *_externalName;

  int32_t          _numParms;
  TR::DataType     *_parmTypes;
  TR::DataType     _returnType;
  void             *_entryPoint;
  TR::IlGenerator  *_ilInjector;
};

} // namespace NJCompiler

#if defined(PUT_NJ_RESOLVEDMETHOD_INTO_TR)

namespace TR {
class ResolvedMethod : public NJCompiler::ResolvedMethod {
public:
  ResolvedMethod(TR_OpaqueMethodBlock *method)
      : NJCompiler::ResolvedMethod(method) {}
  ResolvedMethod(char *fileName, 
                 char *lineNumber, 
                 char *name, 
                 int32_t numArgs,
                 TR::DataType *parmTypes, 
                 TR::DataType returnType,
                 void *entryPoint, 
                 TR::IlGenerator *ilInjector)
      : NJCompiler::ResolvedMethod(fileName, lineNumber, name, numArgs,
                                   parmTypes, returnType, 
								   entryPoint, ilInjector) 
      { }
};
} // namespace TR

#endif // !defined(PUT_NJ_RESOLVEDMETHOD_INTO_TR)

#endif // !defined(NJ_METHOD_INCL)
