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

#ifndef TEST_RESOLVEDMETHOD_INCL
#define TEST_RESOLVEDMETHOD_INCL

#ifndef TR_RESOLVEDMETHOD_COMPOSED
#define TR_RESOLVEDMETHOD_COMPOSED
#define PUT_TEST_RESOLVEDMETHOD_INTO_TR
#endif // TR_RESOLVEDMETHOD_COMPOSED

#include <string.h>

#include "compile/Method.hpp"
#include "compiler/compile/ResolvedMethod.hpp"

namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlType; }
namespace TR { class TypeDictionary; }
namespace TR { class IlInjector; }
namespace TR { class Method; }
namespace TR { class MethodBuilder; }
namespace TR { class FrontEnd; }


// quick and dirty implementation to get up and running
// needs major overhaul

namespace TestCompiler
{

class ResolvedMethodBase : public TR_ResolvedMethod
   {
   virtual uint16_t              nameLength()                                   { return signatureLength(); }
   virtual uint16_t              classNameLength()                              { return signatureLength(); }
   virtual uint16_t              signatureLength()                              { return strlen(signatureChars()); }

   // This group of functions only make sense for Java - we ought to provide answers from that definition
   virtual bool                  isConstructor()                                { return false; }
   virtual bool                  isNonEmptyObjectConstructor()                  { return false; }
   virtual bool                  isFinalInObject()                              { return false; }
   virtual bool                  isStatic()                                     { return true; }
   virtual bool                  isAbstract()                                   { return false; }
   virtual bool                  isCompilable(TR_Memory *)                      { return true; }
   virtual bool                  isNative()                                     { return false; }
   virtual bool                  isSynchronized()                               { return false; }
   virtual bool                  isPrivate()                                    { return false; }
   virtual bool                  isProtected()                                  { return false; }
   virtual bool                  isPublic()                                     { return true; }
   virtual bool                  isFinal()                                      { return false; }
   virtual bool                  isStrictFP()                                   { return false; }
   virtual bool                  isSubjectToPhaseChange(TR::Compilation *comp)  { return false; }

   virtual bool                  hasBackwardBranches()                          { return false; }

   virtual bool                  isNewInstanceImplThunk()                       { return false; }
   virtual bool                  isJNINative()                                  { return false; }
   virtual bool                  isJITInternalNative()                          { return false; }

   uint32_t                      numberOfExceptionHandlers()                    { return 0; }

   virtual bool                  isSameMethod(TR_ResolvedMethod *other)
      {
      return getPersistentIdentifier() == other->getPersistentIdentifier();
      }
   };

class ResolvedMethod : public ResolvedMethodBase, public Method
   {
   public:
   ResolvedMethod(TR_OpaqueMethodBlock *method);
   ResolvedMethod(TR::MethodBuilder *methodBuilder);
   ResolvedMethod(const char      * fileName,
                  const char      * lineNumber,
                  char            * name,
                  int32_t           numParms,
                  TR::IlType     ** parmTypes,
                  TR::IlType      * returnType,
                  void            * entryPoint,
                  TR::IlInjector  * ilInjector)
      : _fileName(fileName),
        _lineNumber(lineNumber),
        _name(name),
        _signature(0),
        _numParms(numParms),
        _parmTypes(parmTypes),
        _returnType(returnType),
        _entryPoint(entryPoint),
        _ilInjector(ilInjector)
      {
      computeSignatureChars();
      }

   virtual TR::Method          * convertToMethod()                          { return this; }

   virtual const char          * signature(TR_Memory *, TR_AllocationKind);
   char                        * localName (uint32_t slot, uint32_t bcIndex, int32_t &nameLength, TR_Memory *trMemory);

   virtual char                * classNameChars()                           { return (char *)_fileName; }
   virtual char                * nameChars()                                { return _name; }
   virtual char                * signatureChars()                           { return _signatureChars; }
   virtual uint16_t              signatureLength()                          { return strlen(signatureChars()); }

   virtual void                * resolvedMethodAddress()                    { return (void *)_ilInjector; }

   virtual uint16_t              numberOfParameterSlots()                   { return _numParms; }
   virtual TR::DataType         parmType(uint32_t slot);
   virtual uint16_t              numberOfTemps()                            { return 0; }

   virtual char                * getParameterTypeSignature(int32_t parmIndex);

   virtual void                * startAddressForJittedMethod()              { return (getEntryPoint()); }
   virtual void                * startAddressForInterpreterOfJittedMethod() { return 0; }

   virtual uint32_t              maxBytecodeIndex()                         { return 0; }
   virtual uint8_t             * code()                                     { return NULL; }
   virtual TR_OpaqueMethodBlock* getPersistentIdentifier()                  { return (TR_OpaqueMethodBlock *) _ilInjector; }
   virtual bool                  isInterpreted()                            { return startAddressForJittedMethod() == 0; }

   const char                  * getLineNumber()                            { return _lineNumber;}
   char                        * getSignature()                             { return _signature;}
   TR::DataType                 returnType();
   TR::IlType                  * returnIlType()                             { return _returnType; }
   int32_t                       getNumArgs()                               { return _numParms;}
   void                          setEntryPoint(void *ep)                    { _entryPoint = ep; }
   void                        * getEntryPoint()                            { return _entryPoint; }

   void                          computeSignatureCharsPrimitive();
   void                          computeSignatureChars();

   TR::IlInjector *getInjector(TR::IlGeneratorMethodDetails * details,
                               TR::ResolvedMethodSymbol *methodSymbol,
                               TR::FrontEnd *fe,
                               TR::SymbolReferenceTable *symRefTab);

   protected:
   const char *_fileName;
   const char *_lineNumber;

   char *_name;
   char *_signature;
   char _signatureChars[64];

   int32_t          _numParms;
   TR::IlType    ** _parmTypes;
   TR::IlType     * _returnType;
   void           * _entryPoint;
   TR::IlInjector * _ilInjector;
   };


} // namespace TestCompiler

#if defined(PUT_TEST_RESOLVEDMETHOD_INTO_TR)

namespace TR
{
   class ResolvedMethod : public TestCompiler::ResolvedMethod
      {
      public:
         ResolvedMethod(TR_OpaqueMethodBlock *method)
            : TestCompiler::ResolvedMethod(method)
            { }

         ResolvedMethod(char            * fileName,
                        char            * lineNumber,
                        char            * name,
                        int32_t           numArgs,
                        TR::IlType     ** parmTypes,
                        TR::IlType      * returnType,
                        void            * entryPoint,
                        TR::IlInjector  * ilInjector)
            : TestCompiler::ResolvedMethod(fileName, lineNumber, name, numArgs,
                                   parmTypes, returnType,
                                   entryPoint, ilInjector)
            { }

         ResolvedMethod(TR::MethodBuilder *methodBuilder)
            : TestCompiler::ResolvedMethod(methodBuilder)
            { }
      };
} // namespace TR

#endif // !defined(PUT_TEST_RESOLVEDMETHOD_INTO_TR)

#endif // !defined(TEST_RESOLVEDMETHOD_INCL)
