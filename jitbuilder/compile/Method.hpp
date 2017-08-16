/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#ifndef JITBUILDER_METHOD_INCL
#define JITBUILDER_METHOD_INCL

#ifndef TR_RESOLVEDMETHOD_COMPOSED
#define TR_RESOLVEDMETHOD_COMPOSED
#define PUT_JITBUILDER_RESOLVEDMETHOD_INTO_TR
#endif // TR_RESOLVEDMETHOD_COMPOSED

#include <string>
#include <string.h>

#include "compile/OMRMethod.hpp"
#include "compile/ResolvedMethod.hpp"

namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlType; }
namespace TR { class TypeDictionary; }
namespace TR { class IlInjector; }
namespace TR { class MethodBuilder; }
namespace TR { class FrontEnd; }


// quick and dirty implementation to get up and running
// needs major overhaul

namespace JitBuilder
{

class Method : public TR_Method
   {
   public:
   TR_ALLOC(TR_Memory::Method);

   Method() : TR_Method(TR_Method::JitBuilder) {}

   // FIXME: need to provide real code for this group
   virtual uint16_t              classNameLength() { return strlen(classNameChars()); }
   virtual uint16_t              nameLength()      { return strlen(nameChars()); }
   virtual uint16_t              signatureLength() { return strlen(signatureChars()); }
   virtual char                * nameChars()       { return "Method"; }
   virtual char                * classNameChars()  { return ""; }
   virtual char                * signatureChars()  { return "()V"; }

   virtual bool                  isConstructor()   { return false; }
   virtual bool                  isFinalInObject() { return false; }
   };

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
   ResolvedMethod(const std::string  & fileName,
                  const char         * lineNumber,
                  const std::string  & name,
                  int32_t              numParms,
                  TR::IlType        ** parmTypes,
                  TR::IlType         * returnType,
                  void               * entryPoint,
                  TR::IlInjector     * ilInjector)
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

   virtual TR_Method           * convertToMethod()                          { return this; }

   virtual const char          * signature(TR_Memory *, TR_AllocationKind);
   char                        * localName (uint32_t slot, uint32_t bcIndex, int32_t &nameLength, TR_Memory *trMemory);

   virtual char                * classNameChars()                           { return (char *) _fileName.c_str(); }
   virtual char                * nameChars()                                { return (char *) _name.c_str(); }
   virtual char                * signatureChars()                           { return _signatureChars; }
   virtual uint16_t              signatureLength()                          { return strlen(signatureChars()); }

   virtual void                * resolvedMethodAddress()                    { return (void *)_ilInjector; }

   virtual uint16_t              numberOfParameterSlots()                   { return _numParms; }
   virtual TR::DataType         parmType(uint32_t slot);
   virtual uint16_t              numberOfTemps()                            { return 0; }

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
   virtual void                  makeParameterList(TR::ResolvedMethodSymbol *);

   TR::IlInjector *getInjector(TR::IlGeneratorMethodDetails * details,
                               TR::ResolvedMethodSymbol *methodSymbol,
                               TR::FrontEnd *fe,
                               TR::SymbolReferenceTable *symRefTab);

   protected:
   std::string _fileName;
   const char *_lineNumber;

   std::string _name;
   char *_signature;
   char _signatureChars[64];

   int32_t          _numParms;
   TR::IlType    ** _parmTypes;
   TR::IlType     * _returnType;
   void           * _entryPoint;
   TR::IlInjector * _ilInjector;
   };


} // namespace JitBuilder

#if defined(PUT_JITBUILDER_RESOLVEDMETHOD_INTO_TR)

namespace TR
{
   class ResolvedMethod : public JitBuilder::ResolvedMethod
      {
      public:
         ResolvedMethod(TR_OpaqueMethodBlock *method)
            : JitBuilder::ResolvedMethod(method)
            { }

         ResolvedMethod(const std::string  & fileName,
                        char               * lineNumber,
                        const std::string  & name,
                        int32_t              numArgs,
                        TR::IlType        ** parmTypes,
                        TR::IlType         * returnType,
                        void               * entryPoint,
                        TR::IlInjector     * ilInjector)
            : JitBuilder::ResolvedMethod(fileName, lineNumber, name, numArgs,
                                   parmTypes, returnType,
                                   entryPoint, ilInjector)
            { }

         ResolvedMethod(TR::MethodBuilder *methodBuilder)
            : JitBuilder::ResolvedMethod(methodBuilder)
            { }
      };
} // namespace TR

#endif // defined(PUT_JITBUILDER_RESOLVEDMETHOD_INTO_TR)

#endif // !defined(JITBUILDER_METHOD_INCL)
