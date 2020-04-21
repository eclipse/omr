/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "compile/ResolvedMethod.hpp"

#include <string.h>
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"

class TR_FrontEnd;
class TR_OpaqueMethodBlock;
class TR_PrexArgInfo;
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class LabelSymbol; }

bool TR_ResolvedMethod::isDAAMarshallingWrapperMethod()
   {
#ifdef J9_PROJECT_SPECIFIC
   if (getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength  ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt          ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength    ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong         ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength   ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble       ||

       // ByteArray Unmarshalling methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort       ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt         ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength   ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength  ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat       ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble)
      {
      return true;
      }
   return false;
#else
   return false;
#endif
   }

bool TR_ResolvedMethod::isDAAPackedDecimalWrapperMethod()
   {
#ifdef J9_PROJECT_SPECIFIC
   if (// DAA Packed Decimal arithmetic methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal     ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal  ||

       // DAA Packed Decimal comparison methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal            ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal    ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal         ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal     ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal  ||

       // DAA Packed Decimal shift methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal  ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal ||

       // DAA Packed Decimal check methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal            ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined1 ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined2 ||

       // DAA Packed Decimal move method
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_movePackedDecimal             ||

       // DAA Packed Decimal <-> Integer
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer ||

       // DAA Packed Decimal <-> Long
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong      ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer      ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal      ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer      ||

       // DAA External Decimal <-> Integer
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal ||

       // DAA External Decimal <-> Long
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong    ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal    ||

       // DAA Packed Decimal <-> External Decimal
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal ||

       // DAA Packed Decimal <-> Unicode Decimal
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal  ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal  ||

       // DAA Packed Decimal <-> BigInteger
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigInteger   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToPackedDecimal   ||

       // DAA Packed Decimal <-> BigDecimal
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal   ||

       // DAA External Decimal <-> BigInteger
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigInteger ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToExternalDecimal ||

       // DAA External Decimal <-> BigDecimal
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToExternalDecimal ||

       // DAA Unicode Decimal <-> Integer
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal ||

       // DAA Unicode Decimal <-> Long
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal        ||

       // DAA Unicode Decimal <-> BigInteger
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigInteger  ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToUnicodeDecimal  ||

       // DAA Unicode Decimal <-> BigDecimal
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigDecimal  ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToUnicodeDecimal  ||


       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_slowSignedPackedToBigDecimal ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_slowBigDecimalToSignedPacked)
      {
      return true;
      }
   return false;
#else
   return false;
#endif
   }

bool TR_ResolvedMethod::isDAAWrapperMethod()
   {
#ifdef J9_PROJECT_SPECIFIC
   return isDAAMarshallingWrapperMethod() || isDAAPackedDecimalWrapperMethod();
#else
   return false;
#endif
   }


bool TR_ResolvedMethod::isDAAMarshallingIntrinsicMethod()
   {
#ifdef J9_PROJECT_SPECIFIC
   if (getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort_        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength_  ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt_          ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength_    ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong_         ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength_   ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat_        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble_       ||

       // ByteArray Unmarshalling methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort_       ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength_ ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt_         ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength_   ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong_        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength_  ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat_       ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble_)
      {
      return true;
      }
   return false;
#else
   return false;
#endif
   }


bool TR_ResolvedMethod::isDAAPackedDecimalIntrinsicMethod()
   {
#ifdef J9_PROJECT_SPECIFIC
   if (// DAA Packed Decimal arithmetic methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal_        ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal_   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal_   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal_     ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal_  ||

       // DAA Packed Decimal comparison methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal_            ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal_    ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal_         ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal_ ||

       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal_     ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal_  ||

       // DAA Packed Decimal shift methods
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_  ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_ ||

       // DAA Packed Decimal check method
       getRecognizedMethod() == TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_ ||

       // DAA Packed Decimal <-> Integer
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer_ ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_   ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer_ ||

       // DAA Packed Decimal <-> Long
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_      ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer_      ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_      ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer_      ||

       // DAA Packed Decimal <-> External Decimal
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal_ ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal_ ||

       // DAA Packed Decimal <-> Unicode Decimal
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal_  ||
       getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal_)
      {
      return true;
      }
   return false;
#else
   return false;
#endif
   }

bool TR_ResolvedMethod::isDAAIntrinsicMethod()
   {
#ifdef J9_PROJECT_SPECIFIC
   return isDAAMarshallingIntrinsicMethod() || isDAAPackedDecimalIntrinsicMethod();
#else
   return false;
#endif
   }

TR::Method *TR_ResolvedMethod::convertToMethod()                          { TR_UNIMPLEMENTED(); return 0; }
uint32_t     TR_ResolvedMethod::numberOfParameters()                       { TR_UNIMPLEMENTED(); return 0; }
uint32_t     TR_ResolvedMethod::numberOfExplicitParameters()               { TR_UNIMPLEMENTED(); return 0; }
TR::DataType TR_ResolvedMethod::parmType(uint32_t)                         { TR_UNIMPLEMENTED(); return TR::NoType; }
char *       TR_ResolvedMethod::getParameterTypeSignature(int32_t parmIndex) { TR_UNIMPLEMENTED(); return 0; }
TR::ILOpCodes TR_ResolvedMethod::directCallOpCode()                        { TR_UNIMPLEMENTED(); return TR::BadILOp; }
TR::ILOpCodes TR_ResolvedMethod::indirectCallOpCode()                      { TR_UNIMPLEMENTED(); return TR::BadILOp; }
TR::DataType TR_ResolvedMethod::returnType()                               { TR_UNIMPLEMENTED(); return TR::NoType; }
uint32_t     TR_ResolvedMethod::returnTypeWidth()                          { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::returnTypeIsUnsigned()                     { TR_UNIMPLEMENTED(); return 0; }
TR::ILOpCodes TR_ResolvedMethod::returnOpCode()                            { TR_UNIMPLEMENTED(); return TR::BadILOp; }
uint16_t     TR_ResolvedMethod::classNameLength()                          { TR_UNIMPLEMENTED(); return 0; }
uint16_t     TR_ResolvedMethod::nameLength()                               { TR_UNIMPLEMENTED(); return 0; }
uint16_t     TR_ResolvedMethod::signatureLength()                          { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::classNameChars()                           { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::nameChars()                                { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::signatureChars()                           { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::isConstructor()                            { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isStatic()                                 { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isAbstract()                               { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isCompilable(TR_Memory *)                  { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isInlineable(TR::Compilation *)            { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isNative()                                 { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isSynchronized()                           { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isPrivate()                                { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isProtected()                              { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isPublic()                                 { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isFinal()                                  { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isStrictFP()                               { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isInterpreted()                            { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isInterpretedForHeuristics()               { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::hasBackwardBranches()                      { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isObjectConstructor()                      { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isNonEmptyObjectConstructor()              { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isCold(TR::Compilation *, bool, TR::ResolvedMethodSymbol * /* = NULL */)             { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isSubjectToPhaseChange(TR::Compilation *) { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isSameMethod(TR_ResolvedMethod *)          { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isNewInstanceImplThunk()                   { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isJNINative()                              { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isJITInternalNative()                      { TR_UNIMPLEMENTED(); return false; }
//bool         TR_ResolvedMethod::isUnsafeWithObjectArg(TR::Compilation *)    { TR_UNIMPLEMENTED(); return false; }
void *       TR_ResolvedMethod::resolvedMethodAddress()                    { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::startAddressForJittedMethod()              { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::startAddressForJNIMethod(TR::Compilation *) { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::startAddressForJITInternalNativeMethod()   { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::startAddressForInterpreterOfJittedMethod() { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::isWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *) { TR_UNIMPLEMENTED(); return 0; }
void         TR_ResolvedMethod::setWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *){ TR_UNIMPLEMENTED(); return; }

TR_FrontEnd *TR_ResolvedMethod::fe()                                       { TR_UNIMPLEMENTED(); return 0; }
intptr_t    TR_ResolvedMethod::getInvocationCount()                       { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::setInvocationCount(intptr_t, intptr_t)   { TR_UNIMPLEMENTED(); return false; }
uint16_t     TR_ResolvedMethod::numberOfParameterSlots()                   { TR_UNIMPLEMENTED(); return 0; }
uint16_t     TR_ResolvedMethod::archetypeArgPlaceholderSlot(TR_Memory *)   { TR_UNIMPLEMENTED(); return 0; }
uint16_t     TR_ResolvedMethod::numberOfTemps()                            { TR_UNIMPLEMENTED(); return 0; }
uint16_t     TR_ResolvedMethod::numberOfPendingPushes()                    { TR_UNIMPLEMENTED(); return 0; }
uint8_t *    TR_ResolvedMethod::bytecodeStart()                            { TR_UNIMPLEMENTED(); return 0; }
uint32_t     TR_ResolvedMethod::maxBytecodeIndex()                         { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::ramConstantPool()                          { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::constantPool()                             { TR_UNIMPLEMENTED(); return 0; }

TR::DataType TR_ResolvedMethod::getLDCType(int32_t)                        { TR_UNIMPLEMENTED(); return TR::NoType; }
bool         TR_ResolvedMethod::isClassConstant(int32_t cpIndex)           { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isStringConstant(int32_t cpIndex)          { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isMethodTypeConstant(int32_t cpIndex)      { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isMethodHandleConstant(int32_t cpIndex)    { TR_UNIMPLEMENTED(); return false; }
uint32_t     TR_ResolvedMethod::intConstant(int32_t)                       { TR_UNIMPLEMENTED(); return 0; }
uint64_t     TR_ResolvedMethod::longConstant(int32_t)                      { TR_UNIMPLEMENTED(); return 0; }
float *      TR_ResolvedMethod::floatConstant(int32_t)                     { TR_UNIMPLEMENTED(); return 0; }
double *     TR_ResolvedMethod::doubleConstant(int32_t, TR_Memory *)       { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::stringConstant(int32_t)                    { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::isUnresolvedString(int32_t, bool optimizeForAOT)                { TR_UNIMPLEMENTED(); return false; }
void *       TR_ResolvedMethod::getConstantDynamicTypeFromCP(int32_t cpIndex)   { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::isConstantDynamic(int32_t cpIndex)            { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isUnresolvedConstantDynamic(int32_t cpIndex)  { TR_UNIMPLEMENTED(); return false; }
void *       TR_ResolvedMethod::dynamicConstant(int32_t cpIndex, uintptr_t *obj)              { TR_UNIMPLEMENTED(); return 0; }
void *       TR_ResolvedMethod::methodTypeConstant(int32_t)                { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::isUnresolvedMethodType(int32_t)            { TR_UNIMPLEMENTED(); return false; }
void *       TR_ResolvedMethod::methodHandleConstant(int32_t)              { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::isUnresolvedMethodHandle(int32_t)          { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::isUnresolvedCallSiteTableEntry(int32_t callSiteIndex) { TR_UNIMPLEMENTED(); return false; }
void *       TR_ResolvedMethod::callSiteTableEntryAddress(int32_t callSiteIndex)      { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::isUnresolvedMethodTypeTableEntry(int32_t cpIndex) { TR_UNIMPLEMENTED(); return false; }
void *       TR_ResolvedMethod::methodTypeTableEntryAddress(int32_t cpIndex)      { TR_UNIMPLEMENTED(); return 0; }

TR_OpaqueClassBlock *TR_ResolvedMethod::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)  { TR_UNIMPLEMENTED(); return 0; }
int32_t      TR_ResolvedMethod::classCPIndexOfFieldOrStatic(int32_t)       { TR_UNIMPLEMENTED(); return 0; }
const char * TR_ResolvedMethod::signature(TR_Memory *, TR_AllocationKind)  { TR_UNIMPLEMENTED(); return 0; }
const char * TR_ResolvedMethod::externalName(TR_Memory *, TR_AllocationKind)  { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::fieldName (int32_t, TR_Memory *, TR_AllocationKind kind)           { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::staticName(int32_t, TR_Memory *, TR_AllocationKind kind)           { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::localName (uint32_t, uint32_t, TR_Memory *){ /*TR_UNIMPLEMENTED();*/ return 0; }
char *       TR_ResolvedMethod::fieldName (int32_t, int32_t &, TR_Memory *, TR_AllocationKind kind) { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::staticName(int32_t, int32_t &, TR_Memory *, TR_AllocationKind kind) { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::localName (uint32_t, uint32_t, int32_t&, TR_Memory *){ /*TR_UNIMPLEMENTED();*/ return 0; }
char *       TR_ResolvedMethod::fieldNameChars(int32_t, int32_t &)         { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::fieldSignatureChars(int32_t, int32_t &)    { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::staticSignatureChars(int32_t, int32_t &)   { TR_UNIMPLEMENTED(); return 0; }
void * &     TR_ResolvedMethod::addressOfClassOfMethod()                   { TR_UNIMPLEMENTED(); }
uint32_t     TR_ResolvedMethod::vTableSlot(uint32_t)                       { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::virtualMethodIsOverridden()                { TR_UNIMPLEMENTED(); return false; }
void         TR_ResolvedMethod::setVirtualMethodIsOverridden()             { TR_UNIMPLEMENTED(); }
void *       TR_ResolvedMethod::addressContainingIsOverriddenBit()         { TR_UNIMPLEMENTED(); return 0; }
int32_t     TR_ResolvedMethod::virtualCallSelector(uint32_t)               { TR_UNIMPLEMENTED(); return 0; }
uint32_t     TR_ResolvedMethod::numberOfExceptionHandlers()                { TR_UNIMPLEMENTED(); return 0; }
uint8_t *    TR_ResolvedMethod::allocateException(uint32_t,TR::Compilation*){ TR_UNIMPLEMENTED(); return 0; }

int32_t      TR_ResolvedMethod::exceptionData(int32_t, int32_t *, int32_t *, int32_t *) { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::getClassNameFromConstantPool(uint32_t, uint32_t &)      { TR_UNIMPLEMENTED(); return 0; }
bool         TR_ResolvedMethod::fieldsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame)    { TR_UNIMPLEMENTED(); return false; }
bool         TR_ResolvedMethod::staticsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame)   { TR_UNIMPLEMENTED(); return false; }
char *       TR_ResolvedMethod::classNameOfFieldOrStatic(int32_t, int32_t &)            { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::classSignatureOfFieldOrStatic(int32_t, int32_t &)       { TR_UNIMPLEMENTED(); return 0; }
char *       TR_ResolvedMethod::staticNameChars(int32_t, int32_t &)                     { TR_UNIMPLEMENTED(); return 0; }
const char * TR_ResolvedMethod::newInstancePrototypeSignature(TR_Memory *, TR_AllocationKind)          { TR_UNIMPLEMENTED(); return 0; }

bool
TR_ResolvedMethod::isJ9()
   {
   return convertToMethod()->isJ9();
   }

TR::RecognizedMethod
TR_ResolvedMethod::getRecognizedMethod()
   {
   return convertToMethod()->getRecognizedMethod();
   }

TR_MethodParameterIterator *
TR_ResolvedMethod::getParameterIterator(TR::Compilation& comp)
   {
   return convertToMethod()->getParameterIterator(comp, this);
   }

uintptr_t *
TR_ResolvedMethod::getMethodHandleLocation()
   {
   TR_ASSERT(convertToMethod()->isArchetypeSpecimen(), "All methods associated with a MethodHandle must be archetype specimens");
   return NULL;
   }

bool
TR_ResolvedMethod::getUnresolvedFieldInCP(int32_t)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

bool
TR_ResolvedMethod::getUnresolvedStaticMethodInCP(int32_t)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

bool
TR_ResolvedMethod::getUnresolvedSpecialMethodInCP(int32_t)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

bool
TR_ResolvedMethod::getUnresolvedVirtualMethodInCP(int32_t)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

bool
TR_ResolvedMethod::fieldAttributes(TR::Compilation *, int32_t, uint32_t *, TR::DataType *, bool *, bool *, bool *, bool, bool *, bool)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

bool
TR_ResolvedMethod::staticAttributes(TR::Compilation *, int32_t, void * *, TR::DataType *, bool *, bool *, bool *, bool, bool *, bool)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

TR_OpaqueClassBlock * TR_ResolvedMethod::containingClass()                                 { TR_UNIMPLEMENTED(); return 0; }
TR_OpaqueClassBlock * TR_ResolvedMethod::getClassFromConstantPool(TR::Compilation *, uint32_t, bool) { TR_UNIMPLEMENTED(); return 0; }
TR_OpaqueClassBlock * TR_ResolvedMethod::classOfStatic(int32_t, bool)                            { TR_UNIMPLEMENTED(); return 0; }
TR_OpaqueClassBlock * TR_ResolvedMethod::classOfMethod()                                   { TR_UNIMPLEMENTED(); return 0; }
uint32_t              TR_ResolvedMethod::classCPIndexOfMethod(uint32_t)                    { TR_UNIMPLEMENTED(); return 0; }

TR_OpaqueMethodBlock *TR_ResolvedMethod::getNonPersistentIdentifier()                      { TR_UNIMPLEMENTED(); return 0; }
TR_OpaqueMethodBlock *TR_ResolvedMethod::getPersistentIdentifier()                         { TR_UNIMPLEMENTED(); return 0; }
TR_OpaqueClassBlock * TR_ResolvedMethod::getResolvedInterfaceMethod(int32_t, uintptr_t *) { TR_UNIMPLEMENTED(); return 0; }

TR_ResolvedMethod * TR_ResolvedMethod::owningMethod()                                      { TR_UNIMPLEMENTED(); return 0; }
void TR_ResolvedMethod::setOwningMethod(TR_ResolvedMethod*)                                { TR_UNIMPLEMENTED();  }

TR_ResolvedMethod * TR_ResolvedMethod::getResolvedStaticMethod (TR::Compilation *, int32_t, bool *)           { TR_UNIMPLEMENTED(); return 0; }
TR_ResolvedMethod * TR_ResolvedMethod::getResolvedSpecialMethod(TR::Compilation *, int32_t, bool *)           { TR_UNIMPLEMENTED(); return 0; }
TR_ResolvedMethod * TR_ResolvedMethod::getResolvedVirtualMethod(TR::Compilation *, int32_t, bool, bool *)     { TR_UNIMPLEMENTED(); return 0; }
TR_ResolvedMethod * TR_ResolvedMethod::getResolvedDynamicMethod(TR::Compilation *, int32_t, bool *)           { TR_UNIMPLEMENTED(); return 0; }
TR_ResolvedMethod * TR_ResolvedMethod::getResolvedHandleMethod (TR::Compilation *, int32_t, bool *)           { TR_UNIMPLEMENTED(); return 0; }
TR_ResolvedMethod * TR_ResolvedMethod::getResolvedHandleMethodWithSignature(TR::Compilation *, int32_t, char *) { TR_UNIMPLEMENTED(); return 0; }

uint32_t
TR_ResolvedMethod::getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock *, int32_t)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR_ResolvedMethod *
TR_ResolvedMethod::getResolvedInterfaceMethod(TR::Compilation *, TR_OpaqueClassBlock *, int32_t)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR_ResolvedMethod *
TR_ResolvedMethod::getResolvedVirtualMethod(TR::Compilation *, TR_OpaqueClassBlock *, int32_t, bool)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

void
TR_ResolvedMethod::setMethodHandleLocation(uintptr_t *location)
   {
   TR_UNIMPLEMENTED();
   }

TR::IlGeneratorMethodDetails *
TR_ResolvedMethod::getIlGeneratorMethodDetails()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR::SymbolReferenceTable *
TR_ResolvedMethod::_genMethodILForPeeking(TR::ResolvedMethodSymbol *, TR::Compilation *, bool resetVisitCount, TR_PrexArgInfo  *argInfo)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


TR::ResolvedMethodSymbol* TR_ResolvedMethod::findOrCreateJittedMethodSymbol(TR::Compilation *comp)
   {
   return comp->createJittedMethodSymbol(this);
   }

void TR_ResolvedMethod::makeParameterList(TR::ResolvedMethodSymbol *methodSym)
   {
   ListAppender<TR::ParameterSymbol> la(&methodSym->getParameterList());
   TR::ParameterSymbol *parmSymbol;
   int32_t slot = 0;
   int32_t ordinal = 0;

   uint32_t parmSlots = numberOfParameterSlots();
   for (int32_t parmIndex = 0; parmIndex < parmSlots; ++parmIndex)
      {
      parmSymbol = methodSym->comp()->getSymRefTab()->createParameterSymbol(methodSym, slot, parmType(parmIndex));
      parmSymbol->setOrdinal(ordinal++);

      char *s = getParameterTypeSignature(parmIndex);
      uint32_t len = strlen(s);
      parmSymbol->setTypeSignature(s, len);

      la.add(parmSymbol);

      ++slot;
      }

   int32_t lastInterpreterSlot = slot + numberOfTemps();
   methodSym->setTempIndex(lastInterpreterSlot, methodSym->comp()->fe());
   methodSym->setFirstJitTempIndex(methodSym->getTempIndex());
   }

TR::SymbolReferenceTable*
TR_ResolvedMethod::genMethodILForPeeking(TR::ResolvedMethodSymbol *methodSymbol, TR::Compilation  *comp, bool resetVisitCount, TR_PrexArgInfo  *argInfo)
   {
   if (comp->getOption(TR_EnableHCR))
      return NULL;

   return _genMethodILForPeeking(methodSymbol, comp, resetVisitCount, argInfo);
   }
