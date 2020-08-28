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

#include "optimizer/VPConstraint.hpp"

#include <ctype.h>
#include <stddef.h>
#include <math.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/ObjectModel.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#ifdef J9_PROJECT_SPECIFIC
#include "env/VMAccessCriticalSection.hpp"
#endif
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/ValuePropagation.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/PersistentCHTable.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/VMJ9.h"
#endif

// ***************************************************************************
//
// Methods of Value Propagation Constraints
//
// ***************************************************************************
TR::VPShortConstraint   *TR::VPConstraint::asShortConstraint()   { return NULL; }
TR::VPShortConst        *TR::VPConstraint::asShortConst()        { return NULL; }
TR::VPShortRange        *TR::VPConstraint::asShortRange()        { return NULL; }
TR::VPIntConstraint     *TR::VPConstraint::asIntConstraint()     { return NULL; }
TR::VPIntConst          *TR::VPConstraint::asIntConst()          { return NULL; }
TR::VPIntRange          *TR::VPConstraint::asIntRange()          { return NULL; }
TR::VPLongConstraint    *TR::VPConstraint::asLongConstraint()    { return NULL; }
TR::VPLongConst         *TR::VPConstraint::asLongConst()         { return NULL; }
TR::VPLongRange         *TR::VPConstraint::asLongRange()         { return NULL; }
TR::VPFloatConstraint   *TR::VPConstraint::asFloatConstraint()   { return NULL; }
TR::VPFloatConst        *TR::VPConstraint::asFloatConst()        { return NULL; }
TR::VPFloatRange        *TR::VPConstraint::asFloatRange()        { return NULL; }
TR::VPDoubleConstraint  *TR::VPConstraint::asDoubleConstraint()  { return NULL; }
TR::VPDoubleConst       *TR::VPConstraint::asDoubleConst()       { return NULL; }
TR::VPDoubleRange       *TR::VPConstraint::asDoubleRange()       { return NULL; }
TR::VP_BCDValue         *TR::VPConstraint::asBCDValue()          { return NULL; }
TR::VP_BCDSign          *TR::VPConstraint::asBCDSign()           { return NULL; }
TR::VPClass             *TR::VPConstraint::asClass()             { return NULL; }
TR::VPClassType         *TR::VPConstraint::asClassType()         { return NULL; }
TR::VPResolvedClass     *TR::VPConstraint::asResolvedClass()     { return NULL; }
TR::VPFixedClass        *TR::VPConstraint::asFixedClass()        { return NULL; }
TR::VPConstString       *TR::VPConstraint::asConstString()       { return NULL; }
TR::VPKnownObject       *TR::VPConstraint::asKnownObject()       { return NULL; }
TR::VPUnresolvedClass   *TR::VPConstraint::asUnresolvedClass()   { return NULL; }
TR::VPClassPresence     *TR::VPConstraint::asClassPresence()     { return NULL; }
TR::VPNullObject        *TR::VPConstraint::asNullObject()        { return NULL; }
TR::VPNonNullObject     *TR::VPConstraint::asNonNullObject()     { return NULL; }
TR::VPPreexistentObject *TR::VPConstraint::asPreexistentObject() { return NULL; }
TR::VPArrayInfo         *TR::VPConstraint::asArrayInfo()         { return NULL; }
TR::VPObjectLocation    *TR::VPConstraint::asObjectLocation()    { return NULL; }
TR::VPMergedConstraints *TR::VPConstraint::asMergedConstraints() { return NULL; }
TR::VPMergedConstraints *TR::VPConstraint::asMergedShortConstraints() {return NULL;}
TR::VPMergedConstraints *TR::VPConstraint::asMergedIntConstraints() { return NULL; }
TR::VPMergedConstraints *TR::VPConstraint::asMergedLongConstraints(){ return NULL; }
TR::VPMergedConstraints *TR::VPConstraint::asMergedFloatConstraints() { return NULL; }
TR::VPMergedConstraints *TR::VPConstraint::asMergedDoubleConstraints() { return NULL; }
TR::VPUnreachablePath   *TR::VPConstraint::asUnreachablePath()   { return NULL; }
TR::VPSync              *TR::VPConstraint::asVPSync()            { return NULL; }
TR::VPRelation          *TR::VPConstraint::asRelation()          { return NULL; }
TR::VPLessThanOrEqual   *TR::VPConstraint::asLessThanOrEqual()   { return NULL; }
TR::VPGreaterThanOrEqual*TR::VPConstraint::asGreaterThanOrEqual(){ return NULL; }
TR::VPEqual             *TR::VPConstraint::asEqual()             { return NULL; }
TR::VPNotEqual          *TR::VPConstraint::asNotEqual()          { return NULL; }
TR::VPShortConstraint   *TR::VPShortConstraint::asShortConstraint()     { return this;}
TR::VPShortConst        *TR::VPShortConst::asShortConst()          { return this;}
TR::VPShortRange        *TR::VPShortRange::asShortRange()          { return this;}
TR::VPIntConstraint     *TR::VPIntConstraint::asIntConstraint()         { return this; }
TR::VPIntConst          *TR::VPIntConst::asIntConst()                   { return this; }
TR::VPIntRange          *TR::VPIntRange::asIntRange()                   { return this; }
TR::VPLongConstraint    *TR::VPLongConstraint::asLongConstraint()       { return this; }
TR::VPLongConst         *TR::VPLongConst::asLongConst()                 { return this; }
TR::VPLongRange         *TR::VPLongRange::asLongRange()                 { return this; }
TR::VPFloatConstraint   *TR::VPFloatConstraint::asFloatConstraint()     { return this; }
TR::VPFloatConst        *TR::VPFloatConst::asFloatConst()               { return this; }
TR::VPFloatRange        *TR::VPFloatRange::asFloatRange()               { return this; }
TR::VPDoubleConstraint  *TR::VPDoubleConstraint::asDoubleConstraint()   { return this; }
TR::VPDoubleConst       *TR::VPDoubleConst::asDoubleConst()             { return this; }
TR::VPDoubleRange       *TR::VPDoubleRange::asDoubleRange()             { return this; }
TR::VPClass             *TR::VPClass::asClass()                         { return this; }
TR::VPClassType         *TR::VPClassType::asClassType()                 { return this; }
TR::VPResolvedClass     *TR::VPResolvedClass::asResolvedClass()         { return this; }
TR::VPFixedClass        *TR::VPFixedClass::asFixedClass()               { return this; }
TR::VPConstString       *TR::VPConstString::asConstString()             { return this; }
TR::VPKnownObject       *TR::VPKnownObject::asKnownObject()             { return this; }
TR::VPUnresolvedClass   *TR::VPUnresolvedClass::asUnresolvedClass()     { return this; }
TR::VPClassPresence     *TR::VPClassPresence::asClassPresence()         { return this; }
TR::VPNullObject        *TR::VPNullObject::asNullObject()               { return this; }
TR::VPNonNullObject     *TR::VPNonNullObject::asNonNullObject()         { return this; }
TR::VPPreexistentObject *TR::VPPreexistentObject::asPreexistentObject() { return this; }
TR::VPArrayInfo         *TR::VPArrayInfo::asArrayInfo()                 { return this; }
TR::VPObjectLocation    *TR::VPObjectLocation::asObjectLocation()       { return this; }
TR::VPMergedConstraints *TR::VPMergedConstraints::asMergedConstraints() { return this; }
TR::VPMergedConstraints *TR::VPMergedConstraints::asMergedShortConstraints() { return (_type.isInt16()) ? this : NULL; }
TR::VPMergedConstraints *TR::VPMergedConstraints::asMergedIntConstraints() { return (_type.isInt32()) ? this : NULL; }
TR::VPMergedConstraints *TR::VPMergedConstraints::asMergedLongConstraints() { return (_type.isInt64()) ? this : NULL; }
TR::VPMergedConstraints *TR::VPMergedConstraints::asMergedFloatConstraints() { return (_type.getDataType() == TR::Float) ? this : NULL; }
TR::VPMergedConstraints *TR::VPMergedConstraints::asMergedDoubleConstraints() { return (_type.getDataType() == TR::Double) ? this : NULL; }
TR::VPUnreachablePath   *TR::VPUnreachablePath::asUnreachablePath()     { return this; }
TR::VPSync              *TR::VPSync::asVPSync()                         { return this; }
TR::VPRelation          *TR::VPRelation::asRelation()                   { return this; }
TR::VPLessThanOrEqual   *TR::VPLessThanOrEqual::asLessThanOrEqual()     { return this; }
TR::VPGreaterThanOrEqual*TR::VPGreaterThanOrEqual::asGreaterThanOrEqual(){ return this; }
TR::VPEqual             *TR::VPEqual::asEqual()                         { return this; }
TR::VPNotEqual          *TR::VPNotEqual::asNotEqual()                   { return this; }

/* Helper functions for comparing floating point types
 * Return 1 if lhs is greater than rhs; 
 *       -1 if lhs is smaller than rhs;
 *        0 if lhs is equal to rhs
 */
int32_t FPCompare(float lhs, float rhs)
   {
   return TR::Compiler->arith.floatComparefloat(lhs, rhs);
   }

int32_t FPCompare(double lhs, double rhs)
   {
   return TR::Compiler->arith.doubleComparedouble(lhs, rhs);
   }

int16_t TR::VPConstraint::getLowShort()
   {
   if (isUnsigned())
       return TR::getMinUnsigned<TR::Int16>();
   return TR::getMinSigned<TR::Int16>();
   }

int16_t TR::VPConstraint::getHighShort()
   {
   if (isUnsigned())
       return TR::getMaxUnsigned<TR::Int16>();
   return TR::getMaxSigned<TR::Int16>();
   }

int32_t TR::VPConstraint::getLowInt()
   {
   if (isUnsigned())
      return TR::getMinUnsigned<TR::Int32>();
   return TR::getMinSigned<TR::Int32>();
   }

int32_t TR::VPConstraint::getHighInt()
   {
   if (isUnsigned())
      return TR::getMaxUnsigned<TR::Int32>();
   return TR::getMaxSigned<TR::Int32>();
   }

int64_t TR::VPConstraint::getLowLong()
   {
   return TR::getMinSigned<TR::Int64>();
   }

int64_t TR::VPConstraint::getHighLong()
   {
   return TR::getMaxSigned<TR::Int64>();
   }

float TR::VPConstraint::getLowFloat()
   {
   return TR::getMinFloat();
   }

float TR::VPConstraint::getHighFloat()
   {
   return TR::getMaxFloat();
   }

double TR::VPConstraint::getLowDouble()
   {
   return TR::getMinDouble();
   }

double TR::VPConstraint::getHighDouble()
   {
   return TR::getMaxDouble();
   }

uint16_t TR::VPConstraint::getUnsignedLowShort()
   {
   if ( (getLowShort() ^ getHighShort()) >= 0)       // if both numbers are the same sign, return the small value
      return (uint16_t)getLowShort();

   return TR::getMinUnsigned<TR::Int16>();
   }

uint16_t TR::VPConstraint::getUnsignedHighShort()
   {
   if ( (getLowShort() ^ getHighShort()) >= 0)       // if both numbers have the same sign, getHigh is the high value
      return (uint16_t)getHighShort();

   return TR::getMaxUnsigned<TR::Int16>();
   }

uint32_t TR::VPConstraint::getUnsignedLowInt()
   {
   if ( (getLowInt() ^ getHighInt()) >= 0)       // if both numbers are the same sign, return the small value
      return (uint32_t)getLowInt();

   return TR::getMinUnsigned<TR::Int32>();
   }

uint32_t TR::VPConstraint::getUnsignedHighInt()
   {
   if ( (getLowInt() ^ getHighInt()) >= 0)       // if both numbers have the same sign, getHigh is the high value
      return (uint32_t)getHighInt();

   return TR::getMaxUnsigned<TR::Int32>();
   }

uint64_t TR::VPConstraint::getUnsignedLowLong()
   {
   if ( (getLowLong() ^ getHighLong()) >= 0)       // if both numbers are the same sign, return the small value
      return (uint64_t)getLowLong();

   return TR::getMinUnsigned<TR::Int64>();
   }

uint64_t TR::VPConstraint::getUnsignedHighLong()
   {
   if ( (getLowLong() ^ getHighLong()) >= 0)       // if both numbers have the same sign, getHigh is the high value
      return (uint64_t)getHighLong();

   return TR::getMaxUnsigned<TR::Int64>();    // consumer beware! You don't want to assign this to a int64_t
   }


bool TR::VPConstraint::isNullObject()
   {
   return false;
   }

bool TR::VPConstraint::isNonNullObject()
   {
   return false;
   }

bool TR::VPConstraint::isPreexistentObject()
   {
   return false;
   }

TR_OpaqueClassBlock *TR::VPConstraint::getClass()
   {
   return NULL;
   }

bool TR::VPConstraint::isFixedClass()
   {
   return false;
   }

bool TR::VPConstraint::isConstString()
   {
   return false;
   }

float TR::VPConstraint::fpAddWithOverflow(float a, float b, bool& overflow)
   {
   float sum = TR::Compiler->arith.floatAddFloat(a, b);

   //The overflow flag is set when the arithmetic sum is greater the max/smaller than the min possible float value 
   //the compiler can represent. This happends only when both of the two operands have the same sign bit. Therefore, 
   //we check if the operation yields to an overflow by first comparing the signbits of the two floating points. 
   if ((FPCompare(a, 0.0f) == 1) == (FPCompare(b, 0.0f) == 1))
      {
      //Then instead of directly adding the two operands(, which might cause undefined behavior), we check if
      //the second operand is greater than the max value/smaller than the min value the first operand can be added to
      //without an overflow.
      float float_max = TR::getMaxFloat();
      float float_min = TR::getMinFloat();
      if (FPCompare(a, 0.0f) == 1)
         overflow = FPCompare(b, TR::Compiler->arith.floatSubtractFloat(float_max, a)) == 1;
         
      else
         overflow = FPCompare(b, TR::Compiler->arith.floatSubtractFloat(float_min, a)) == -1;
      }
   else 
      overflow = false;

   return sum;
   }

float TR::VPConstraint::fpSubWithOverflow(float a, float b, bool& overflow)
   {
   float diff = fpAddWithOverflow(a, TR::Compiler->arith.floatNegate(b), overflow);
   return diff;
   }

double TR::VPConstraint::fpAddWithOverflow(double a, double b, bool& overflow)
   {
   double sum =FPCompare(a, b);

   //The overflow flag is set when the arithmetic sum is greater the max/smaller than the min possible double value 
   //the compiler can represent. This happends only when both of the two operands have the same sign bit. Therefore, 
   //we check if the operation yields to an overflow by first comparing the signbits of the two floating points. 
   if ((FPCompare(a, 0.0) == 1) == (FPCompare(b, 0.0) == 1))
      {
      //Then instead of directly adding the two operands(, which might cause undefined behavior), we check if
      //the second operand is greater than the max value/smaller than the min value the first operand can be added to
      //without an overflow.
      double double_max = TR::getMaxFloat();
      double double_min = TR::getMinFloat();
      if (FPCompare(a, 0.0) == 1)
         overflow = FPCompare(b, TR::Compiler->arith.doubleSubtractDouble(double_max, a)) == 1;
         
      else
         overflow = FPCompare(b, TR::Compiler->arith.doubleSubtractDouble(double_min, a)) == -1;
      }
   else 
      overflow = false;

   return sum;
   }

double TR::VPConstraint::fpSubWithOverflow(double a, double b, bool& overflow)
   {
   double diff = fpAddWithOverflow(a, TR::Compiler->arith.doubleNegate(b), overflow);
   return diff;
   }

// VP_SPECIALKLASS is created so that any type that
// intersects with it results in a null type. It is
// not meant to be propagated during the analysis
//
bool TR::VPConstraint::isSpecialClass(uintptr_t klass)
   {
   if (klass == VP_SPECIALKLASS)
      return true;
   return false;
   }

TR_YesNoMaybe TR::VPConstraint::canOverflow()
   {
   return TR_maybe;
   }

TR::VPClassType *TR::VPConstraint::getClassType()
   {
   return NULL;
   }

TR::VPClassPresence *TR::VPConstraint::getClassPresence()
   {
   return NULL;
   }

TR::VPArrayInfo *TR::VPConstraint::getArrayInfo()
   {
   return NULL;
   }

TR::VPPreexistentObject *TR::VPConstraint::getPreexistence()
   {
   return NULL;
   }

TR::VPObjectLocation *TR::VPConstraint::getObjectLocation()
   {
   return NULL;
   }

TR::VPKnownObject *TR::VPConstraint::getKnownObject()
   {
   return NULL;
   }

TR::VPConstString *TR::VPConstraint::getConstString()
   {
   return NULL;
   }

const char *TR::VPConstraint::getClassSignature(int32_t &len)
   {
   return NULL;
   }

TR_YesNoMaybe TR::VPConstraint::isStackObject()
   {
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPConstraint::isHeapObject()
   {
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPConstraint::isClassObject()
   {
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPConstraint::isJavaLangClassObject()
   {
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPConstraint::isJ9ClassObject()
   {
   return TR_maybe;
   }

int16_t TR::VPShortConstraint::getLowShort()
   {
   return getLow();
   }

int16_t TR::VPShortConstraint::getHighShort()
   {
   return getHigh();
   }

int32_t TR::VPIntConstraint::getLowInt()
   {
   return getLow();
   }

int32_t TR::VPIntConstraint::getHighInt()
   {
   return getHigh();
   }


int64_t TR::VPLongConstraint::getLowLong()
   {
   return getLow();
   }

int64_t TR::VPLongConstraint::getHighLong()
   {
   return getHigh();
   }

float TR::VPFloatConstraint::getLowFloat()
   {
   return getLow();
   }

float TR::VPFloatConstraint::getHighFloat()
   {
   return getHigh();
   }

double TR::VPDoubleConstraint::getLowDouble()
   {
   return getLow();
   }

double TR::VPDoubleConstraint::getHighDouble()
   {
   return getHigh();
   }

int16_t TR::VPMergedConstraints::getLowShort()
   {
   return _constraints.getListHead()->getData()->getLowShort();
   }

int16_t TR::VPMergedConstraints::getHighShort()
   {
   return _constraints.getListHead()->getData()->getHighShort();
   }

int32_t TR::VPMergedConstraints::getLowInt()
   {
   return _constraints.getListHead()->getData()->getLowInt();
   }

int32_t TR::VPMergedConstraints::getHighInt()
   {
   return _constraints.getLastElement()->getData()->getHighInt();
   }

int64_t TR::VPMergedConstraints::getLowLong()
   {
   return _constraints.getListHead()->getData()->getLowLong();
   }

int64_t TR::VPMergedConstraints::getHighLong()
   {
   return _constraints.getLastElement()->getData()->getHighLong();
   }

float TR::VPMergedConstraints::getLowFloat()
   {
   return _constraints.getListHead()->getData()->getLowFloat();
   }

float TR::VPMergedConstraints::getHighFloat()
   {
   return _constraints.getLastElement()->getData()->getHighFloat();
   }

double TR::VPMergedConstraints::getLowDouble()
   {
   return _constraints.getListHead()->getData()->getLowDouble();
   }

double TR::VPMergedConstraints::getHighDouble()
   {
   return _constraints.getLastElement()->getData()->getHighDouble();
   }

uint16_t TR::VPMergedConstraints::getUnsignedLowShort()
   {
   return _constraints.getListHead()->getData()->getUnsignedLowShort();
   }

uint16_t TR::VPMergedConstraints::getUnsignedHighShort()
   {
   return _constraints.getListHead()->getData()->getUnsignedHighShort();
   }

uint32_t TR::VPMergedConstraints::getUnsignedLowInt()
   {
   return _constraints.getListHead()->getData()->getUnsignedLowInt();
   }

uint32_t TR::VPMergedConstraints::getUnsignedHighInt()
   {
   return _constraints.getLastElement()->getData()->getHighInt();
   }

uint64_t TR::VPMergedConstraints::getUnsignedLowLong()
   {
   return _constraints.getListHead()->getData()->getUnsignedLowLong();
   }

uint64_t TR::VPMergedConstraints::getUnsignedHighLong()
   {
   return _constraints.getLastElement()->getData()->getUnsignedHighLong();
   }

const char *TR::VPClass::getClassSignature(int32_t &len)
   {
   if (_type)
      return _type->getClassSignature(len);
   return NULL;
   }

bool TR::VPClass::isNullObject()
   {
   if (_presence)
      return _presence->isNullObject();
   return false;
   }

bool TR::VPClass::isNonNullObject()
   {
   if (_presence)
      return _presence->isNonNullObject();
   return false;
   }

bool TR::VPClass::isPreexistentObject()
   {
   if (_preexistence)
      return _preexistence->isPreexistentObject();
   return false;
   }

TR_OpaqueClassBlock *TR::VPClass::getClass()
   {
   if (_type)
      return _type->getClass();
   return NULL;
   }

bool TR::VPClass::isFixedClass()
   {
   if (_type)
      return _type->isFixedClass();
   return false;
   }

bool TR::VPClass::isConstString()
   {
   if (_type)
      return _type->isConstString();
   return false;
   }

TR_YesNoMaybe TR::VPClass::isStackObject()
   {
   if (_location)
      return _location->isStackObject();
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPClass::isHeapObject()
   {
   if (_location)
      return _location->isHeapObject();
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPClass::isClassObject()
   {
   if (_location && _location->isClassObject() != TR_maybe)
      return _location->isClassObject();
   if (_type && _type->isJavaLangClassObject() != TR_maybe)
      return _type->isJavaLangClassObject();
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPClass::isJavaLangClassObject()
   {
   if (_location && _location->isJavaLangClassObject() != TR_maybe)
      return _location->isJavaLangClassObject();
   if (_type && _type->isJavaLangClassObject() != TR_maybe)
      return _type->isJavaLangClassObject();
   return TR_maybe;
   }

TR_YesNoMaybe TR::VPClass::isJ9ClassObject()
   {
   if (_location != NULL)
      return _location->isJ9ClassObject();
   else if (_type != NULL)
      return TR_no; // in absence of _location, _type means an instance
   else
      return TR_maybe;
   }

TR::VPClassType *TR::VPClass::getClassType()
   {
   return _type;
   }

TR::VPArrayInfo *TR::VPClass::getArrayInfo()
   {
   return _arrayInfo;
   }

TR::VPClassPresence *TR::VPClass::getClassPresence()
   {
   return _presence;
   }

TR::VPPreexistentObject *TR::VPClass::getPreexistence()
   {
   return _preexistence;
   }

TR::VPObjectLocation *TR::VPClass::getObjectLocation()
   {
   return _location;
   }

TR::VPKnownObject *TR::VPClass::getKnownObject()
   {
   return _type? _type->asKnownObject() : NULL;
   }

TR::VPConstString *TR::VPClass::getConstString()
   {
   return _type? _type->asConstString() : NULL;
   }

TR::VPClassType *TR::VPClassType::getClassType()
   {
   return this;
   }

TR::VPClassPresence *TR::VPClassPresence::getClassPresence()
   {
   return this;
   }

TR::VPPreexistentObject *TR::VPPreexistentObject::getPreexistence()
   {
   return this;
   }

TR::VPObjectLocation *TR::VPObjectLocation::getObjectLocation()
   {
   return this;
   }

TR::DataType TR::VPClassType::getPrimitiveArrayDataType()
   {
   if (_sig[0] != '[')
      return TR::NoType;
   switch (_sig[1])
      {
      case 'Z':
      case 'B': return TR::Int8;
      case 'C':
      case 'S': return TR::Int16;
      case 'I': return TR::Int32;
      case 'J': return TR::Int64;
      case 'F': return TR::Float;
      case 'D': return TR::Double;
      }
   return TR::NoType;
   }

TR_YesNoMaybe TR::VPClassType::isClassObject()
   {
   return isJavaLangClassObject();
   }

TR_YesNoMaybe TR::VPClassType::isJavaLangClassObject()
   {
   // TR_yes will cause callers to misinterpret
   // this->getClassType()==this as the represented class, i.e.
   // it would make this look just like Class.class.
   if (_len == 17 && strncmp(_sig, "Ljava/lang/Class;", 17) == 0)
      return TR_maybe;
   if ((_len == 18 && strncmp(_sig, "Ljava/lang/Object;", 18) == 0) ||
         (_len == 22 && strncmp(_sig, "Ljava/io/Serializable;", 22) == 0) ||
         (_len == 36 && strncmp(_sig, "Ljava/lang/reflect/AnnotatedElement;", 36) == 0) ||
         (_len == 38 && strncmp(_sig, "Ljava/lang/reflect/GenericDeclaration;", 38) == 0) ||
         (_len == 24 && strncmp(_sig, "Ljava/lang/reflect/Type;", 24) == 0))
      return TR_maybe;
   return TR_no; // java.lang.Class is final and is the direct subclass of Object.
   }

TR_YesNoMaybe TR::VPClassType::isJ9ClassObject()
   {
   return TR_no;
   }

TR_YesNoMaybe TR::VPKnownObject::isJavaLangClassObject()
   {
   // The class held in the base TR::VPFixedClass is usually the class to which
   // the instance belongs, but if that would be java/lang/Class, then it's
   // instead the represented class.
   return _isJavaLangClass ? TR_yes : TR_no;
   }

bool TR::VPKnownObject::isArrayWithConstantElements(TR::Compilation * comp)
   {
   TR::KnownObjectTable *knot = comp->getKnownObjectTable();
   TR_ASSERT(knot, "TR::KnownObjectTable should not be null");
   return knot->isArrayWithConstantElements(_index);
   }

TR_YesNoMaybe TR::VPClassType::isArray()
   {
   if (_sig[0] == '[')
      return TR_yes;
   if ((strncmp(_sig, "Ljava/lang/Object;", 18) == 0) ||
        isCloneableOrSerializable())
      return TR_maybe;
   return TR_no; // all arrays are direct subclasses of Object or other arrays (both cases covered above)
   }

TR::VPResolvedClass::VPResolvedClass(TR_OpaqueClassBlock *klass, TR::Compilation * comp)
   : TR::VPClassType(ResolvedClassPriority), _class(klass)
   {
   if (TR::VPConstraint::isSpecialClass((uintptr_t)klass))
      { _len = 0; _sig = 0; }
   else
      _sig = TR::Compiler->cls.classSignature_DEPRECATED(comp, klass, _len, comp->trMemory());
   }

TR::VPResolvedClass::VPResolvedClass(TR_OpaqueClassBlock *klass, TR::Compilation * comp, int32_t p)
   : TR::VPClassType(p), _class(klass)
   {
   if (TR::VPConstraint::isSpecialClass((uintptr_t)klass))
      { _len = 0; _sig = 0; }
   else
      _sig = TR::Compiler->cls.classSignature_DEPRECATED(comp, klass, _len, comp->trMemory());
   }

TR_OpaqueClassBlock *TR::VPResolvedClass::getClass()
   {
   return _class;
   }

const char *TR::VPResolvedClass::getClassSignature(int32_t &len)
   {
   len = _len;
   return _sig;
   }

TR::VPClassType *TR::VPResolvedClass::getArrayClass(OMR::ValuePropagation *vp)
   {
   TR_OpaqueClassBlock *arrayClass = vp->fe()->getArrayClassFromComponentClass(getClass());
   if (arrayClass)
      return TR::VPResolvedClass::create(vp, arrayClass);

   // TODO - when getArrayClassFromComponentClass is fixed up to always return
   // the array class, remove the above "if" and the following code.
   //
   char *arraySig = (char *)vp->trMemory()->allocateStackMemory(_len+2);
   arraySig[0] = '[';
   arraySig[_len+1] = 0;
   memcpy(arraySig+1, _sig, _len);
   return TR::VPUnresolvedClass::create(vp, arraySig, _len+1, vp->comp()->getCurrentMethod());
   }

bool TR::VPResolvedClass::isReferenceArray(TR::Compilation *comp)
   {
   return TR::Compiler->cls.isReferenceArray(comp, _class);
   }

bool TR::VPResolvedClass::isPrimitiveArray(TR::Compilation *comp)
   {
   return TR::Compiler->cls.isPrimitiveArray(comp, _class);
   }

bool TR::VPResolvedClass::isJavaLangObject(OMR::ValuePropagation *vp)
   {
#ifdef J9_PROJECT_SPECIFIC
   void *javaLangObject = vp->comp()->getObjectClassPointer();
   if (javaLangObject)
      return javaLangObject == _class;
#endif

   return (_len == 18 && !strncmp(_sig, "Ljava/lang/Object;", 18));
   }

bool TR::VPClassType::isCloneableOrSerializable()
   {
   if (_len == 21 && !strncmp(_sig, "Ljava/lang/Cloneable;", 21) )
      return true;
   if (_len == 22 && !strncmp(_sig, "Ljava/io/Serializable;", 22) )
      return true;
   return false;
   }

bool TR::VPFixedClass::isFixedClass()
   {
   return true;
   }

bool TR::VPConstString::isConstString()
   {
   return true;
   }

TR::VPClassType *TR::VPFixedClass::getArrayClass(OMR::ValuePropagation *vp)
   {
   TR_OpaqueClassBlock *arrayClass = vp->fe()->getArrayClassFromComponentClass(getClass());
   if (arrayClass)
      return TR::VPFixedClass::create(vp, arrayClass);

   // TODO - when getArrayClassFromComponentClass is fixed up to always return
   // the array class, remove the above "if" and the following code.
   //
   char *arraySig = (char *)vp->trMemory()->allocateStackMemory(_len+2);
   arraySig[0] = '[';
   arraySig[_len+1] = 0;
   memcpy(arraySig+1, _sig, _len);
   return TR::VPUnresolvedClass::create(vp, arraySig, _len+1, vp->comp()->getCurrentMethod());
   }

TR_YesNoMaybe TR::VPFixedClass::isArray()
   {
   if (*_sig == '[')
      return TR_yes;
   return TR_no;
   }

const char *TR::VPUnresolvedClass::getClassSignature(int32_t &len)
   {
   len = _len;
   return _sig;
   }

TR::VPClassType *TR::VPUnresolvedClass::getArrayClass(OMR::ValuePropagation *vp)
   {
   char *arraySig = (char *)vp->trMemory()->allocateStackMemory(_len+2);
   arraySig[0] = '[';
   arraySig[_len+1] = 0;
   memcpy(arraySig+1, _sig, _len);
   return TR::VPUnresolvedClass::create(vp, arraySig, _len+1, _method);
   }

bool TR::VPUnresolvedClass::isReferenceArray(TR::Compilation *comp)
   {
   return _sig[0] == '[' && (_sig[1] == '[' || _sig[1] == 'L');
   }

bool TR::VPUnresolvedClass::isPrimitiveArray(TR::Compilation *comp)
   {
   return _sig[0] == '[' && _sig[1] != '[' && _sig[1] != 'L';
   }

bool TR::VPNullObject::isNullObject()
   {
   return true;
   }

bool TR::VPNonNullObject::isNonNullObject()
   {
   return true;
   }

bool TR::VPPreexistentObject::isPreexistentObject()
   {
   return true;
   }

TR::VPArrayInfo *TR::VPArrayInfo::getArrayInfo()
   {
   return this;
   }

TR_YesNoMaybe TR::VPObjectLocation::isWithin(VPObjectLocationKind area)
   {
   if (isKindSubset(_kind, area))
      return TR_yes;
   else if ((_kind & area) != 0)
      return TR_maybe;
   else
      return TR_no;
   }

TR_YesNoMaybe TR::VPObjectLocation::isStackObject()
   {
   return isWithin(StackObject);
   }

TR_YesNoMaybe TR::VPObjectLocation::isHeapObject()
   {
   return isWithin(HeapObject);
   }

TR_YesNoMaybe TR::VPObjectLocation::isClassObject()
   {
   return isWithin(ClassObject);
   }

TR_YesNoMaybe TR::VPObjectLocation::isJavaLangClassObject()
   {
   return isWithin(JavaLangClassObject);
   }

TR_YesNoMaybe TR::VPObjectLocation::isJ9ClassObject()
   {
   return isWithin(J9ClassObject);
   }

TR::VPRelation *TR::VPLessThanOrEqual::getComplement(OMR::ValuePropagation *vp)
   {
   TR::VPRelation *rel = TR::VPGreaterThanOrEqual::create(vp, -increment());
   if (hasArtificialIncrement())
      rel->setHasArtificialIncrement();
   return rel;
   }

TR::VPRelation *TR::VPGreaterThanOrEqual::getComplement(OMR::ValuePropagation *vp)
   {
   TR::VPRelation *rel = TR::VPLessThanOrEqual::create(vp, -increment());
   if (hasArtificialIncrement())
      rel->setHasArtificialIncrement();
   return rel;
   }

TR::VPRelation *TR::VPEqual::getComplement(OMR::ValuePropagation *vp)
   {
   if (increment() == 0)
      return this;
   return TR::VPEqual::create(vp, -increment());
   }

TR::VPRelation *TR::VPNotEqual::getComplement(OMR::ValuePropagation *vp)
   {
   if (increment() == 0)
      return this;
   return TR::VPNotEqual::create(vp, -increment());
   }

// ***************************************************************************
//
// Creation of Value Propagation Constraints
//
// ***************************************************************************

TR::VPConstraint *TR::VPConstraint::create(OMR::ValuePropagation *vp, const char *sig, int32_t len, TR_ResolvedMethod *method, bool isFixedClass)
   {
   // Create a constraint if possible from any arbitrary signature
   //
   switch (sig[0])
      {
      case 'L':
      case '[':
         return TR::VPClassType::create(vp, sig, len, method, isFixedClass);
      case 'B':
         return TR::VPIntRange::create(vp, TR::Int8, TR_no /* signed */);
      case 'Z':
         return TR::VPIntRange::create(vp, TR::Int8, TR_yes /* unsigned */); // FIXME: we can probably do 0/1 here
      case 'C':
         return TR::VPIntRange::create(vp, TR::Int16, TR_yes /* unsigned */);
      case 'S':
         return TR::VPIntRange::create(vp, TR::Int16, TR_no /* signed */);
      }
   return NULL;
   }

TR::VPShortConst *TR::VPShortConst::create(OMR::ValuePropagation *vp, int16_t v)
   {
   int32_t hash = ((uint32_t)v) % VP_HASH_TABLE_SIZE;
   TR::VPShortConst * constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
       {
       constraint = entry->constraint->asShortConst();
       if (constraint && constraint->getShort() == v)
           return constraint;
       }
   constraint = new (vp->trStackMemory()) TR::VPShortConst(v);
   vp->addConstraint(constraint,hash);
   return constraint;
   }

TR::VPIntConst *TR::VPIntConst::create(OMR::ValuePropagation *vp, int32_t v)
   {
   // If the constant is zero, return the cached constraint
   //
   if (v == 0)
      return vp->_constantZeroConstraint;

   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)v) % VP_HASH_TABLE_SIZE;
   TR::VPIntConst *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asIntConst();
      if (constraint && constraint->getInt() == v)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPIntConst(v);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPConstraint *TR::VPShortConst::createExclusion(OMR::ValuePropagation *vp, int16_t v)
   {
   if (v == TR::getMinSigned<TR::Int16>())
       return TR::VPShortRange::create(vp,v+1,TR::getMaxSigned<TR::Int16>());
   if (v == TR::getMaxSigned<TR::Int16>())
       return TR::VPShortRange::create(vp,TR::getMinSigned<TR::Int16>(),v-1);
   return TR::VPMergedConstraints::create(vp, TR::VPShortRange::create(vp,TR::getMinSigned<TR::Int16>(),v-1),TR::VPShortRange::create(vp,v+1,TR::getMaxSigned<TR::Int16>()));
   }

TR::VPConstraint *TR::VPIntConst::createExclusion(OMR::ValuePropagation *vp, int32_t v)
   {
   if (v == TR::getMinSigned<TR::Int32>())
      return TR::VPIntRange::create(vp, v+1, TR::getMaxSigned<TR::Int32>());
   if (v == TR::getMaxSigned<TR::Int32>())
      return TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), v-1);
   return TR::VPMergedConstraints::create(vp, TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), v-1), TR::VPIntRange::create(vp, v+1, TR::getMaxSigned<TR::Int32>()));
   }

TR::VPShortConstraint * TR::VPShortRange::create(OMR::ValuePropagation * vp, int16_t low, int16_t high, TR_YesNoMaybe canOverflow)
   {
   if (low == TR::getMinSigned<TR::Int16>() && high == TR::getMaxSigned<TR::Int16>())
       return NULL;

   if (low == high)
       return TR::VPShortConst::create(vp,low);

   uint32_t uint32low = (uint32_t)low;
   uint32_t uint32high = (uint32_t)high;

   int32_t hash = ((uint32low<<8)+uint32high) % VP_HASH_TABLE_SIZE;
   TR::VPShortRange *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for(entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
       {
        constraint = entry->constraint->asShortRange();
        if(constraint &&
           constraint->_low == low &&
           constraint->_high == high &&
           constraint->_overflow == canOverflow)
           return constraint;
       }
   constraint = new (vp->trStackMemory()) TR::VPShortRange(low,high);
   constraint->setCanOverflow(canOverflow);
   vp->addConstraint(constraint,hash);
   return constraint;
   }

TR::VPShortConstraint *TR::VPShortRange::create(OMR::ValuePropagation *vp)
   {
   return TR::VPShortRange::createWithPrecision(vp, VP_UNDEFINED_PRECISION);
   }

TR::VPShortConstraint *TR::VPShortRange::createWithPrecision(OMR::ValuePropagation *vp, int32_t precision, bool isNonNegative)
   {
   int64_t lo, hi;
   constrainRangeByPrecision(TR::getMinSigned<TR::Int16>(), TR::getMaxSigned<TR::Int16>(), precision, lo, hi, isNonNegative);
   return TR::VPShortRange::create(vp, lo, hi);
   }

TR::VPIntConstraint *TR::VPIntRange::create(OMR::ValuePropagation *vp, int32_t low, int32_t high, TR_YesNoMaybe canOverflow)
   {
   if (low == TR::getMinSigned<TR::Int32>() && high == TR::getMaxSigned<TR::Int32>())
      return NULL;

//    if (isUnsigned &&
//          ((uint32_t)low == TR::getMinUnsigned<TR::Int32>() && (uint32_t)high == TR::getMaxUnsigned<TR::Int32>()))
//       return NULL;

   if (low == high)
      return TR::VPIntConst::create(vp, low);

   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)((low << 16) + high)) % VP_HASH_TABLE_SIZE;
   TR::VPIntRange *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asIntRange();
      if (constraint &&
            constraint->_low == low &&
            constraint->_high == high &&
            constraint->_overflow == canOverflow)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPIntRange(low, high);
   constraint->setCanOverflow(canOverflow);
   //if (isUnsigned)
   //   constraint->setIsUnsigned(true);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPIntConstraint *TR::VPIntRange::create(OMR::ValuePropagation *vp, TR::DataTypes dt, TR_YesNoMaybe isUnsigned)
   {
   return TR::VPIntRange::createWithPrecision(vp, dt, VP_UNDEFINED_PRECISION, isUnsigned);
   }

TR::VPIntConstraint *TR::VPIntRange::createWithPrecision(OMR::ValuePropagation *vp, TR::DataType dt, int32_t precision, TR_YesNoMaybe isUnsigned, bool isNonNegative)
   {
   TR_ASSERT(dt > TR::NoType && dt < TR::Int64, "Bad range for datatype in integerLoad constant propagation\n");

   int64_t lo = TR::getMinSigned<TR::Int64>();
   int64_t hi = TR::getMaxSigned<TR::Int64>();

   if (dt == TR::Int32)
      constrainRangeByPrecision(TR::getMinSigned<TR::Int32>(), TR::getMaxSigned<TR::Int32>(), precision, lo, hi, isNonNegative);

   if (isUnsigned == TR_no)
      {
      if (dt == TR::Int8)
         constrainRangeByPrecision(TR::getMinSigned<TR::Int8>(), TR::getMaxSigned<TR::Int8>(), precision, lo, hi, isNonNegative);
      else if (dt == TR::Int16)
         constrainRangeByPrecision(TR::getMinSigned<TR::Int16>(), TR::getMaxSigned<TR::Int16>(), precision, lo, hi, isNonNegative);
      }
   else if (isUnsigned == TR_yes)
      {
      if (dt == TR::Int8)
         constrainRangeByPrecision(0, TR::getMaxUnsigned<TR::Int8>(), precision, lo, hi, isNonNegative);
      else if (dt == TR::Int16)
         constrainRangeByPrecision(0, TR::getMaxUnsigned<TR::Int16>(), precision, lo, hi, isNonNegative);
      }
   else
      {
      if (dt == TR::Int8)
         constrainRangeByPrecision(TR::getMinSigned<TR::Int8>(), TR::getMaxUnsigned<TR::Int8>(), precision, lo, hi, isNonNegative);
      else if (dt == TR::Int16)
         constrainRangeByPrecision(TR::getMinSigned<TR::Int16>(), TR::getMaxUnsigned<TR::Int16>(), precision, lo, hi, isNonNegative);
      }
   return TR::VPIntRange::create(vp, lo, hi);
   }

TR::VPLongConst *TR::VPLongConst::create(OMR::ValuePropagation *vp, int64_t v)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = (int32_t)(v >> 32);
   hash += (int32_t)v;
   hash = ((uint32_t)hash) % VP_HASH_TABLE_SIZE;
   TR::VPLongConst *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asLongConst();
      if (constraint && constraint->_low == v)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPLongConst(v);
   vp->addConstraint(constraint, hash);
   return constraint;
   }
TR::VPConstraint *TR::VPLongConst::createExclusion(OMR::ValuePropagation *vp, int64_t v)
   {
   if (v == TR::getMinSigned<TR::Int64>())
      return TR::VPLongRange::create(vp, v+1, TR::getMaxSigned<TR::Int64>());
   if (v == TR::getMaxSigned<TR::Int64>())
      return TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), v-1);
   return TR::VPMergedConstraints::create(vp, TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), v-1), TR::VPLongRange::create(vp, v+1, TR::getMaxSigned<TR::Int64>()));
   }

TR::VPLongConstraint *TR::VPLongRange::create(OMR::ValuePropagation *vp, int64_t low, int64_t high,
                                            bool powerOfTwo, TR_YesNoMaybe canOverflow)
   {
   if (low == TR::getMinSigned<TR::Int64>() && high == TR::getMaxSigned<TR::Int64>() && !powerOfTwo)
      return NULL;

   if (low == high)
      return TR::VPLongConst::create(vp, low);

   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)low + (uint32_t)high) % VP_HASH_TABLE_SIZE;
   TR::VPLongRange *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asLongRange();
      if (constraint &&
            constraint->_low == low &&
            constraint->_high == high &&
            constraint->_overflow == canOverflow)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPLongRange(low, high);
   constraint->setCanOverflow(canOverflow);
   vp->addConstraint(constraint, hash);

   if (powerOfTwo)
      constraint->setIsPowerOfTwo();

   return constraint;
   }


TR::VPFloatConst *TR::VPFloatConst::create(OMR::ValuePropagation *vp, float v)
   {
   // If the constraint does not already exist, create it
   //
   std::hash<float> float_hash_func;
   int32_t hash = ((uint32_t) float_hash_func(v)) % VP_HASH_TABLE_SIZE;
   TR::VPFloatConst *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asFloatConst();
      if (constraint && constraint->_low == v)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPFloatConst(v);
   vp->addConstraint(constraint, hash);
   return constraint;
   }


/* Create a complementary Constraint against the value v. 
*/
TR::VPConstraint *TR::VPFloatConst::createExclusion(OMR::ValuePropagation *vp, float v)
   {
   // TO-DO: Need to find a workaround for excluding a float number without using std::nextafter as the OMR AppVeyor build on github does not support std::nextafter function. 
   // The std::nextafter int std library helps find the very next float representation to the current float.
   // Currently we return a full range upon calling *TR::VPFloatConst::createExclusion(), as this is considered to be most conservative operation.

   return NULL;

   // if (TR::Compiler->arith.floatComparefloat(v, std::numeric_limits<float>::lowest()) == 0)
   //    {
   //    float immediateAfterNegINF = std::nextafter(std::numeric_limits<float>::lowest(), 0.0f);
   //    return TR::VPIntRange::create(vp, immediateAfterNegINF, std::numeric_limits<float>::max());
   //    }
      
   // if (TR::Compiler->arith.floatComparefloat(v, std::numeric_limits<float>::max()) == 0)
   //    {
   //    float immediateBeforePosINF = std::nextafter(std::numeric_limits<float>::max(), 0.0f);
   //    return TR::VPIntRange::create(vp, std::numeric_limits<float>::lowest(), immediateBeforePosINF);
   //    }

   // float immediateBefore, immediateAfter;
   // immediateBefore = std::nextafter(v, std::numeric_limits<float>::lowest());
   // immediateAfter = std::nextafter(v, std::numeric_limits<float>::max());
   // return TR::VPMergedConstraints::create(vp, TR::VPIntRange::create(vp, std::numeric_limits<float>::lowest(), immediateBefore), TR::VPIntRange::create(vp, immediateAfter, std::numeric_limits<float>::max()));
   }


TR::VPDoubleConst *TR::VPDoubleConst::create(OMR::ValuePropagation *vp, double v)
   {
   // If the constraint does not already exist, create it
   //
   std::hash<double> double_hash_func;
   int32_t hash = ((uint32_t) double_hash_func(v)) % VP_HASH_TABLE_SIZE;
   TR::VPDoubleConst *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asDoubleConst();
      if (constraint && constraint->_low == v)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPDoubleConst(v);
   vp->addConstraint(constraint, hash);
   return constraint;
   }


/* Create a complementary Constraint against the value v. 
*/
TR::VPConstraint *TR::VPDoubleConst::createExclusion(OMR::ValuePropagation *vp, double v)
   {
   // TO-DO: see the comments in TR::VPFloatConst::createExclusion
   return NULL;
   }


TR::VPFloatConstraint *TR::VPFloatRange::create(OMR::ValuePropagation *vp, float low, float high, TR_YesNoMaybe canOverflow)
   {
   if (FPCompare(low, TR::getMinFloat()) == 0 && 
       FPCompare(high, TR::getMaxFloat()) == 0)
      return NULL;

   if (FPCompare(low, high) == 0)
      return TR::VPFloatConst::create(vp, low);

   // If the constraint does not already exist, create it
   //
   std::hash<float> float_hash_func;
   int32_t hash = ((uint32_t) float_hash_func(low) + (uint32_t) float_hash_func(high)) % VP_HASH_TABLE_SIZE;
   TR::VPFloatRange *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asFloatRange();
      if (constraint &&
            constraint->_low == low &&
            constraint->_high == high &&
            constraint->_overflow == canOverflow)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPFloatRange(low, high);
   constraint->setCanOverflow(canOverflow);
   vp->addConstraint(constraint, hash);

   return constraint;
   }


TR::VPDoubleConstraint *TR::VPDoubleRange::create(OMR::ValuePropagation *vp, double low, double high, TR_YesNoMaybe canOverflow)
   {
   if (FPCompare(low, TR::getMinDouble()) == 0 && 
       FPCompare(high, TR::getMaxDouble()) == 0)
      return NULL;

   if (FPCompare(low, high) == 0)
      return TR::VPDoubleConst::create(vp, low);

   // If the constraint does not already exist, create it
   //
   std::hash<double> double_hash_func;
   int32_t hash = ((uint32_t) double_hash_func(low) + (uint32_t) double_hash_func(high)) % VP_HASH_TABLE_SIZE;
   TR::VPDoubleRange *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asDoubleRange();
      if (constraint &&
            constraint->_low == low &&
            constraint->_high == high &&
            constraint->_overflow == canOverflow)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPDoubleRange(low, high);
   constraint->setCanOverflow(canOverflow);
   vp->addConstraint(constraint, hash);

   return constraint;
   }


TR::VPConstraint *TR::VPClass::create(OMR::ValuePropagation *vp, TR::VPClassType *type, TR::VPClassPresence *presence,
                                    TR::VPPreexistentObject *preexistence, TR::VPArrayInfo *arrayInfo, TR::VPObjectLocation *location)
   {
   // We shouldn't create a class constraint that contains the "null" constraint
   // since null objects are not typed.
   //
   if (presence)
      {
      TR_ASSERT(!presence->isNullObject(), "Can't create a class constraint with a NULL constraint");
      }

   // If only one of the parts is non-null we don't need a TR::VPClass constraint
   //
   if (!type)
      {
      if (!presence)
         {
         if (!preexistence)
            {
            if (!arrayInfo)
               return location;
            else if (!location)
               return arrayInfo;
            }
         else if (!arrayInfo && !location)
            return preexistence;
         }
      else if (!preexistence && !arrayInfo && !location)
         return presence;
      }
   else if (!presence && !preexistence && !arrayInfo && !location)
      return type;

#ifdef J9_PROJECT_SPECIFIC
   // TR::VPFixedClass combined with JavaLangClassObject location picks out a
   // particular java/lang/Class instance, for which we can have a known
   // object. Inject one here if we don't already have one.
   TR::KnownObjectTable *knot = vp->comp()->getOrCreateKnownObjectTable();
   if (knot
       && location != NULL
       && location->isJavaLangClassObject() == TR_yes
       && type != NULL
       && type->asFixedClass() != NULL
       && type->asKnownObject() == NULL
       && !isSpecialClass((uintptr_t)type->getClass()))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(vp->comp()->fe());
      uintptr_t objRefOffs = fej9->getOffsetOfJavaLangClassFromClassField();
      uintptr_t *objRef = (uintptr_t*)(type->getClass() + objRefOffs);
      TR::KnownObjectTable::Index index = knot->getOrCreateIndexAt(objRef);
      type = TR::VPKnownObject::createForJavaLangClass(vp, index);
      }
#endif

   // If the constraint does not already exist, create it
   //
   int32_t hash = (((int32_t)(intptr_t)type)>>2) + (((int32_t)(intptr_t)presence)>>2) + (((int32_t)(intptr_t)preexistence)>>2) +
      (((int32_t)(intptr_t)arrayInfo)>>2) + (((int32_t)(intptr_t)location)>>2);
   hash = ((uint32_t)hash) % VP_HASH_TABLE_SIZE;
   TR::VPClass *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asClass();
      if (constraint &&
          constraint->_type         == type &&
          constraint->_presence     == presence &&
          constraint->_preexistence == preexistence &&
          constraint->_arrayInfo    == arrayInfo &&
          constraint->_location     == location)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPClass(type, presence, preexistence, arrayInfo, location);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPClassType *TR::VPClassType::create(OMR::ValuePropagation *vp, TR::SymbolReference *symRef, bool isFixedClass, bool isPointerToClass)
   {
   if (!symRef->isUnresolved())
      {
      TR_OpaqueClassBlock *classObject = (TR_OpaqueClassBlock*)symRef->getSymbol()->getStaticSymbol()->getStaticAddress();
      if (isPointerToClass)
         classObject = *((TR_OpaqueClassBlock**)classObject);
      if (isFixedClass)
         return TR::VPFixedClass::create(vp, classObject);
      return TR::VPResolvedClass::create(vp, classObject);
      }

   int32_t len;
   char *name = TR::Compiler->cls.classNameChars(vp->comp(), symRef, len);
   TR_ASSERT(name, "can't get class name from symbol reference");
   char *sig = classNameToSignature(name, len, vp->comp());
   //return TR::VPUnresolvedClass::create(vp, sig, len, symRef->getOwningMethod(vp->comp()));
   return TR::VPClassType::create(vp, sig, len, symRef->getOwningMethod(vp->comp()), isFixedClass);
   }

TR::VPClassType *TR::VPClassType::create(OMR::ValuePropagation *vp, const char *sig, int32_t len, TR_ResolvedMethod *method, bool isFixed, TR_OpaqueClassBlock *classObject)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (!classObject)
      classObject = vp->fe()->getClassFromSignature(sig, len, method);
   if (classObject)
      {
      bool isClassInitialized = false;
      bool allowForAOT = vp->comp()->getOption(TR_UseSymbolValidationManager);
      TR_PersistentClassInfo * classInfo =
         vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classObject, vp->comp(), allowForAOT);
      if (classInfo && classInfo->isInitialized())
         {
         if (isFixed)
            return TR::VPFixedClass::create(vp, classObject);
         return TR::VPResolvedClass::create(vp, classObject);
         }
      }
#endif
   return TR::VPUnresolvedClass::create(vp, sig, len, method);
   }

TR::VPResolvedClass *TR::VPResolvedClass::create(OMR::ValuePropagation *vp, TR_OpaqueClassBlock *klass)
   {
   // If the class is final, we really want to make this a fixed class
   //
   if (!TR::VPConstraint::isSpecialClass((uintptr_t)klass) && TR::Compiler->cls.isClassFinal(vp->comp(), klass))
      {
      if (TR::Compiler->cls.isClassArray(vp->comp(), klass))
         {
         // An array class is fixed if the base class for the array is final
         //
         TR_OpaqueClassBlock * baseClass = vp->fe()->getLeafComponentClassFromArrayClass(klass);
         if (baseClass && TR::Compiler->cls.isClassFinal(vp->comp(), baseClass))
            return TR::VPFixedClass::create(vp, klass);
         }
      else
         return TR::VPFixedClass::create(vp, klass);
      }

   // If the constraint does not already exist, create it
   //
   int32_t hash = (int32_t)((((uintptr_t)klass) >> 2) % VP_HASH_TABLE_SIZE);
   TR::VPResolvedClass *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asResolvedClass();
      if (constraint && !constraint->asFixedClass() &&
          constraint->getClass() == klass)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPResolvedClass(klass, vp->comp());
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPFixedClass *TR::VPFixedClass::create(OMR::ValuePropagation *vp, TR_OpaqueClassBlock *klass)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = (int32_t)((((uintptr_t)klass) << 2) % VP_HASH_TABLE_SIZE);
   TR::VPFixedClass *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asFixedClass();
      if (constraint && !constraint->hasMoreThanFixedClassInfo() && constraint->getClass() == klass)
         {
         return constraint;
         }
      }
   constraint = new (vp->trStackMemory()) TR::VPFixedClass(klass, vp->comp());
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPKnownObject *TR::VPKnownObject::create(OMR::ValuePropagation *vp, TR::KnownObjectTable::Index index, bool isJavaLangClass)
   {
   TR::KnownObjectTable *knot = vp->comp()->getKnownObjectTable();
   TR_ASSERT(knot, "Can't create a TR::VPKnownObject without a known-object table");
   if (!knot)
      return NULL;
   if (knot->isNull(index)) // No point in worrying about the NULL case because existing constraints handle that optimally
      return NULL;

   int32_t hash = (index * 3331) % VP_HASH_TABLE_SIZE;
   TR::VPKnownObject *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->getKnownObject();
      if (constraint && constraint->_index == index)
         return constraint;
      }

   // Must make a new one; try to acquire VM access and grab the object's class
   //
   constraint = NULL;
#ifdef J9_PROJECT_SPECIFIC
      {
      TR::VMAccessCriticalSection vpKnownObjectCriticalSection(vp->comp(),
                                                                TR::VMAccessCriticalSection::tryToAcquireVMAccess);

      if (vpKnownObjectCriticalSection.hasVMAccess())
         {
         TR_OpaqueClassBlock *clazz = TR::Compiler->cls.objectClass(vp->comp(), knot->getPointer(index));
         TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(clazz);
         if (isJavaLangClass)
            {
            TR_ASSERT(clazz == jlClass, "Use createForJavaLangClass only for instances of java/lang/Class");
            clazz = TR::Compiler->cls.classFromJavaLangClass(vp->comp(), knot->getPointer(index));
            }
         else
            {
            // clazz is already right
            TR_ASSERT(clazz != jlClass, "For java/lang/Class instances, caller needs to use createForJavaLangClass.");
            }
         constraint = new (vp->trStackMemory()) TR::VPKnownObject(clazz, vp->comp(), index, isJavaLangClass);
         vp->addConstraint(constraint, hash);
         }
      }
#endif
   return constraint;
   }

TR::VPConstString *TR::VPConstString::create(OMR::ValuePropagation *vp, TR::SymbolReference *symRef)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::VMAccessCriticalSection vpConstStringCriticalSection(vp->comp(),
                                                             TR::VMAccessCriticalSection::tryToAcquireVMAccess);

   if (vpConstStringCriticalSection.hasVMAccess())
      {
      uintptr_t stringStaticAddr = (uintptr_t) symRef->getSymbol()->castToStaticSymbol()->getStaticAddress();
      uintptr_t string = vp->comp()->fej9()->getStaticReferenceFieldAtAddress(stringStaticAddr);
      // with no vmaccess, staticAddress cannot be guaranteed to remain the same
      // during the analysis. so use a different hash input
      //
      ////int32_t hash = ((uintptr_t)string >> 2) % VP_HASH_TABLE_SIZE;
      ////int32_t hash = ((uintptr_t)(symRef->getSymbol()->castToStaticSymbol()) >> 2) % VP_HASH_TABLE_SIZE;

      // since vmaccess has been acquired already, chars cannot be null
      //
      int32_t len = vp->comp()->fej9()->getStringLength(string);
      int32_t i = 0;
      uint32_t hashValue = 0;
      for (int32_t i = 0; i < len && i < TR_MAX_CHARS_FOR_HASH; i++)
         hashValue += TR::Compiler->cls.getStringCharacter(vp->comp(), string, i);

      int32_t hash = (int32_t)(((uintptr_t)hashValue) % VP_HASH_TABLE_SIZE);

      OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
      TR::VPConstString *constraint;
      for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
         {
         constraint = entry->constraint->asConstString();
         if (constraint)
            {
            uintptr_t constraintStaticAddr = (uintptr_t)constraint->_symRef->getSymbol()->castToStaticSymbol()->getStaticAddress();
            if (string == vp->comp()->fej9()->getStaticReferenceFieldAtAddress(constraintStaticAddr))
               {
               return constraint;
               }
            }
         }
      constraint = new (vp->trStackMemory()) TR::VPConstString(vp->comp()->getStringClassPointer(), vp->comp(), symRef);
      vp->addConstraint(constraint, hash);
      return constraint;
      }
#endif
   return NULL;
   }

uint16_t TR::VPConstString::charAt(int32_t i, TR::Compilation * comp)
   {
   uint16_t result = 0;
#ifdef J9_PROJECT_SPECIFIC
   TR_FrontEnd * fe = comp->fe();
   TR::VMAccessCriticalSection charAtCriticalSection(comp,
                                                      TR::VMAccessCriticalSection::tryToAcquireVMAccess);
   if (charAtCriticalSection.hasVMAccess())
      {
      uintptr_t stringStaticAddr = (uintptr_t)_symRef->getSymbol()->castToStaticSymbol()->getStaticAddress();
      uintptr_t string = comp->fej9()->getStaticReferenceFieldAtAddress(stringStaticAddr);
      int32_t len = comp->fej9()->getStringLength(string);
      bool canRead = true;
      if (i < 0 || i >= len)
         canRead = false;
      if (canRead)
         result = TR::Compiler->cls.getStringCharacter(comp, string, i);
      }
#endif
   return result;
   }


bool TR::VPConstString::getFieldByName(TR::SymbolReference *symRef, void* &val, TR::Compilation * comp)
   {
   return TR::Compiler->cls.getStringFieldByName(comp, _symRef, symRef, val);
   }

TR::VPUnresolvedClass *TR::VPUnresolvedClass::create(OMR::ValuePropagation *vp, const char *sig, int32_t len, TR_ResolvedMethod *method)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = (((uint32_t)(uintptr_t)method >> 2) + len) % VP_HASH_TABLE_SIZE;
   TR::VPUnresolvedClass *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asUnresolvedClass();
      if (constraint && constraint->_len == len &&
          constraint->_method == method)
         {
         if (!strncmp(constraint->_sig, sig, len))
            return constraint;
         }
      }
   constraint = new (vp->trStackMemory()) TR::VPUnresolvedClass(sig, len, method);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPNullObject *TR::VPNullObject::create(OMR::ValuePropagation *vp)
   {
   return vp->_nullObjectConstraint;
   }

TR::VPNonNullObject *TR::VPNonNullObject::create(OMR::ValuePropagation *vp)
   {
   return vp->_nonNullObjectConstraint;
   }

TR::VPPreexistentObject *TR::VPPreexistentObject::create(OMR::ValuePropagation *vp, TR_OpaqueClassBlock *c)
   {
   TR_ASSERT(vp->comp()->ilGenRequest().details().supportsInvalidation(), "Can't use TR::VPPreexistentObject unless the compiled method supports invalidation");
   int32_t hash = (int32_t)((((uintptr_t)c) << 2) % VP_HASH_TABLE_SIZE);
   TR::VPPreexistentObject *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->getPreexistence();
      if (constraint && constraint->getPreexistence()->getAssumptionClass() == c)
         {
         return constraint;
         }
      }

   constraint = new (vp->trStackMemory()) TR::VPPreexistentObject(c);
   vp->addConstraint(constraint, hash);
   return constraint;
   //return vp->_preexistentObjectConstraint;
   }

TR::VPArrayInfo *TR::VPArrayInfo::create(OMR::ValuePropagation *vp, int32_t lowBound, int32_t highBound, int32_t elementSize)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)((lowBound << 16) + highBound + elementSize)) % VP_HASH_TABLE_SIZE;
   TR::VPArrayInfo *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asArrayInfo();
      if (constraint && constraint->lowBound() == lowBound && constraint->highBound() == highBound && constraint->elementSize() == elementSize)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPArrayInfo(lowBound, highBound, elementSize);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPArrayInfo *TR::VPArrayInfo::create(OMR::ValuePropagation *vp, char *sig)
   {
   TR_ASSERT(*sig == '[', "expecting array signature");
   TR::DataType d = TR::Symbol::convertSigCharToType(sig[1]);
   int32_t stride;
   if (d == TR::Address)
      stride = TR::Compiler->om.sizeofReferenceField();
   else
      stride = TR::Symbol::convertTypeToSize(d);

   return TR::VPArrayInfo::create(vp, 0, TR::getMaxSigned<TR::Int32>() / stride, stride);
   }

TR::VPMergedConstraints *TR::VPMergedConstraints::create(OMR::ValuePropagation *vp, TR::VPConstraint *first, TR::VPConstraint *second)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = (int32_t)(((((uintptr_t)first) >> 2) + (((uintptr_t)second) >> 2)) % VP_HASH_TABLE_SIZE);

   TR::VPMergedConstraints *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asMergedConstraints();
      if (constraint)
         {
         ListElement<TR::VPConstraint> *p = constraint->_constraints.getListHead();
         if (p->getData() == first)
            {
            p = p->getNextElement();
            if (p->getData() == second && !p->getNextElement())
               return constraint;
            }
         }
      }
   TR_ScratchList<TR::VPConstraint> list(vp->trMemory());
   list.add(second);
   list.add(first);

   constraint = new (vp->trStackMemory()) TR::VPMergedConstraints(list.getListHead(), vp->trMemory());
   if (first->isUnsigned() && second->isUnsigned())
      constraint->setIsUnsigned(true);

   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPObjectLocation *TR::VPObjectLocation::create(OMR::ValuePropagation *vp, VPObjectLocationKind kind)
   {
   int32_t hash = (kind * 4109) % VP_HASH_TABLE_SIZE;
   TR::VPObjectLocation *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asObjectLocation();
      if (constraint && constraint->_kind == kind)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPObjectLocation(kind);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPMergedConstraints *TR::VPMergedConstraints::create(OMR::ValuePropagation *vp, ListElement<TR::VPConstraint> *list)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = 0;
   bool allUnsigned = true;
   if (!list)
      allUnsigned = false;

   ListElement<TR::VPConstraint> *p1, *p2;
   for (p1 = list; p1; p1 = p1->getNextElement())
      {
      if (!p1->getData()->isUnsigned())
         allUnsigned = false;
      hash += (int32_t)(((uintptr_t)(p1->getData())) >> 2);
      }
   hash = ((uint32_t)hash) % VP_HASH_TABLE_SIZE;

   TR::VPMergedConstraints *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asMergedConstraints();
      if (constraint)
         {
         for (p1 = list, p2 = constraint->_constraints.getListHead();
              p1 && p2 && p1->getData() == p2->getData();
              p1 = p1->getNextElement(), p2 = p2->getNextElement())
            ;
         if (!p1 && !p2)
            return constraint;
         }
      }
   constraint = new (vp->trStackMemory()) TR::VPMergedConstraints(list, vp->trMemory());
   if (allUnsigned)
      constraint->setIsUnsigned(true);

   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPUnreachablePath *TR::VPUnreachablePath::create(OMR::ValuePropagation *vp)
   {
   return vp->_unreachablePathConstraint;
   }

TR::VPSync *TR::VPSync::create(OMR::ValuePropagation *vp, TR_YesNoMaybe v)
   {
   int32_t hash = ((uint32_t)(v << 2) * 4109) % VP_HASH_TABLE_SIZE;
   TR::VPSync *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asVPSync();
      if (constraint &&
            (constraint->syncEmitted() == v))
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPSync(v);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPLessThanOrEqual *TR::VPLessThanOrEqual::create(OMR::ValuePropagation *vp, int32_t incr)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)((incr << 16) + LessThanOrEqualPriority)) % VP_HASH_TABLE_SIZE;
   TR::VPLessThanOrEqual *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asLessThanOrEqual();
      if (constraint && constraint->increment() == incr)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPLessThanOrEqual(incr);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPGreaterThanOrEqual *TR::VPGreaterThanOrEqual::create(OMR::ValuePropagation *vp, int32_t incr)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)((incr << 16) + GreaterThanOrEqualPriority)) % VP_HASH_TABLE_SIZE;
   TR::VPGreaterThanOrEqual *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asGreaterThanOrEqual();
      if (constraint && constraint->increment() == incr)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPGreaterThanOrEqual(incr);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPEqual *TR::VPEqual::create(OMR::ValuePropagation *vp, int32_t incr)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)((incr << 16) + EqualPriority)) % VP_HASH_TABLE_SIZE;
   TR::VPEqual *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asEqual();
      if (constraint && constraint->increment() == incr)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPEqual(incr);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

TR::VPNotEqual *TR::VPNotEqual::create(OMR::ValuePropagation *vp, int32_t incr)
   {
   // If the constraint does not already exist, create it
   //
   int32_t hash = ((uint32_t)((incr << 16) + NotEqualPriority)) % VP_HASH_TABLE_SIZE;
   TR::VPNotEqual *constraint;
   OMR::ValuePropagation::ConstraintsHashTableEntry *entry;
   for (entry = vp->_constraintsHashTable[hash]; entry; entry = entry->next)
      {
      constraint = entry->constraint->asNotEqual();
      if (constraint && constraint->increment() == incr)
         return constraint;
      }
   constraint = new (vp->trStackMemory()) TR::VPNotEqual(incr);
   vp->addConstraint(constraint, hash);
   return constraint;
   }

// ***************************************************************************
//
// Merging Value Propagation Constraints (logical OR)
//
// ***************************************************************************

TR::VPConstraint * TR::VPConstraint::merge(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   // If this is the same constraint, just return it
   //
   if (this == other)
      return this;

   // Call the merge method on the constraint with the highest merge priority
   //
   if (other->priority() > priority())
      return other->merge1(this, vp);
   return merge1(other, vp);
   }

TR::VPConstraint * TR::VPConstraint::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   // Default action is to generalize the constraint
   //
   return NULL;
   }


TR::VPConstraint *TR::VPShortConstraint::merge1(TR::VPConstraint * other, OMR::ValuePropagation * vp)
   {
   TRACER(vp, this, other);

     TR::VPShortConstraint *otherShort = other->asShortConstraint();
     if (otherShort)
        {
        if (otherShort->getLow() < getLow())
            return otherShort->merge1(this,vp);
        if (otherShort->getHigh() <= getHigh())
            return this;
        if (otherShort->getLow() <= getHigh() + 1)
           {
           if (getLow() == TR::getMinSigned<TR::Int16>() && otherShort->getHigh() == TR::getMaxSigned<TR::Int16>())
               return NULL;
           return TR::VPShortRange::create(vp, getLow(),otherShort->getHigh());
           }
        }
     return NULL;
   }

TR::VPConstraint *TR::VPIntConstraint::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPIntConstraint *otherInt = other->asIntConstraint();
   //if (otherInt && otherInt->isUnsigned() && isUnsigned())
   //   return merge1(other, vp, true);

   if (otherInt)
      {
      if (otherInt->getLow() < getLow())
         return otherInt->merge1(this, vp);
      if (otherInt->getHigh() <= getHigh())
         return this; // This range covers the other
      if (otherInt->getLow() <= getHigh()+1)
         {
         // Ranges overlap
         //
         if (getLow() == TR::getMinSigned<TR::Int32>() && otherInt->getHigh() == TR::getMaxSigned<TR::Int32>())
            return NULL; // Constraint has now gone
         return TR::VPIntRange::create(vp, getLow(), otherInt->getHigh());
         }

      return TR::VPMergedConstraints::create(vp, this, other);
      }
   else
      {
      // due to the presence of aladd's, ints might merge with longs
      TR::VPLongConstraint *otherLong = other->asLongConstraint();
      if (otherLong)
         {
         int64_t lowVal, highVal;
         // if wrap-around is possible, the constraint should be gone
         if (((int64_t)otherLong->getLow() < (int64_t)TR::getMinSigned<TR::Int32>()) ||
             ((int64_t)otherLong->getHigh() > (int64_t)TR::getMaxSigned<TR::Int32>()))
            return NULL;

         // compute lower of the lows
         if ((int64_t)otherLong->getLow() < (int64_t)getLow())
            lowVal = otherLong->getLow();
         else
            lowVal = getLow();

         // compute higher of the highs
         if ((int64_t)otherLong->getHigh() <= (int64_t)getHigh())
            highVal = getHigh();
         else
            highVal = otherLong->getHigh();

         // check for range overlap. if there is an overlap, create
         // the appropriate Int Range.
         if ((int64_t)otherLong->getLow() <= (int64_t)(getHigh()+1))
            {
            if (getLow() == TR::getMinSigned<TR::Int32>() && (int64_t)otherLong->getHigh() == (int64_t)TR::getMaxSigned<TR::Int32>())
               return NULL;
            return TR::VPIntRange::create(vp, (int32_t)lowVal, (int32_t)highVal);
            }
         else
            {
            // the ranges are distinct, before merging, they should be ordered
            //
            if ((int64_t)otherLong->getLow() < (int64_t)getLow())
               return TR::VPMergedConstraints::create(vp, TR::VPIntRange::create(vp, (int32_t)otherLong->getLow(), (int32_t)otherLong->getHigh()), this);
            else
               return TR::VPMergedConstraints::create(vp, this, TR::VPIntRange::create(vp, (int32_t)otherLong->getLow(), (int32_t)otherLong->getHigh()));
            }
         }
      }
   return NULL;
   }

TR::VPConstraint *TR::VPLongConstraint::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPLongConstraint *otherLong = other->asLongConstraint();
   if (otherLong)
      {
      if (otherLong->getLow() < getLow())
         return otherLong->merge1(this, vp);
      if (otherLong->getHigh() <= getHigh())
         return this; // This range covers the other
      if (otherLong->getLow() <= getHigh()+1)
         {
         // Ranges overlap
         //
         if (getLow() == TR::getMinSigned<TR::Int64>() && otherLong->getHigh() == TR::getMaxSigned<TR::Int64>())
            return NULL; // Constraint has now gone
         return TR::VPLongRange::create(vp, getLow(), otherLong->getHigh());
         }
      return TR::VPMergedConstraints::create(vp, this, other);
      }
   else
      {
      TR::VPIntConstraint *otherInt = other->asIntConstraint();
      if (otherInt)
         {
         int64_t lowVal, highVal;

         if ((int64_t)otherInt->getLow() < (int64_t)getLow())
            lowVal = otherInt->getLow();
         else
            lowVal = getLow();
         if ((int64_t)otherLong->getHigh() <= (int64_t)getHigh())
            highVal = getHigh();
         else
            highVal = otherInt->getHigh();
         // check for range overlap. if there is an overlap, create
         // the appropriate Long Range.
         if ((int64_t)otherInt->getLow() <= (int64_t)(getHigh()+1))
            {
            if (lowVal == TR::getMinSigned<TR::Int64>() && highVal == TR::getMaxSigned<TR::Int64>())
               return NULL;
            return TR::VPLongRange::create(vp, lowVal, highVal);
            }
         else
            {
            // the ranges are distinct, before merging, they should be ordered
            //
            if ((int64_t)otherInt->getLow() < (int64_t)getLow())
               return TR::VPMergedConstraints::create(vp, TR::VPLongRange::create(vp, otherInt->getLow(), otherInt->getHigh()), this);
            else
               return TR::VPMergedConstraints::create(vp, this, TR::VPLongRange::create(vp, otherInt->getLow(), otherInt->getHigh()));
            }
         }
      }
   return NULL;
   }


/* Take the union of `this` and `other` constraints
 * If one contains the other, return the larger one
 * If they are mutually inclusive, return a new Constraint of their union 
 * If they are mutually exclusive, return a MergedConstraint of their union
 */
TR::VPConstraint *TR::VPFloatConstraint::merge1(TR::VPConstraint * other, OMR::ValuePropagation * vp)
   {
   TRACER(vp, this, other);

     TR::VPFloatConstraint *otherFloat = other->asFloatConstraint();
     if (otherFloat)
        {
        if (FPCompare(otherFloat->getLow(), getLow()) == -1)
            return otherFloat->merge1(this,vp);
        if (FPCompare(otherFloat->getHigh(), getHigh()) == 1)
            return this;
        if (FPCompare(otherFloat->getLow(), getHigh()) == -1)
           {
           if (FPCompare(getLow(), TR::getMinFloat()) == 0 && 
               FPCompare(otherFloat->getHigh(), TR::getMaxFloat()) == 0)
               return NULL;
           return TR::VPFloatRange::create(vp, getLow(),otherFloat->getHigh());
           }
         return TR::VPMergedConstraints::create(vp, this, other);
        }
     return NULL;
   }


/* Take the union of `this` and `other` constraints
 * If one contains the other, return the larger one
 * If they are mutually inclusive, return a new Constraint of their union 
 * If they are mutually exclusive, return a MergedConstraint of their union
 */
TR::VPConstraint *TR::VPDoubleConstraint::merge1(TR::VPConstraint * other, OMR::ValuePropagation * vp)
   {
   TRACER(vp, this, other);

     TR::VPDoubleConstraint *otherDouble = other->asDoubleConstraint();
     if (otherDouble)
        {
        if (FPCompare(otherDouble->getLow(), getLow()) == -1)
            return otherDouble->merge1(this,vp);
        if (FPCompare(otherDouble->getHigh(), getHigh()) == 1)
            return this;
        if (FPCompare(otherDouble->getLow(), getHigh()) == -1)
           {
           if (FPCompare(getLow(), TR::getMinDouble()) == 0 && 
               FPCompare(otherDouble->getHigh(), TR::getMaxDouble()) == 0)
               return NULL;
           return TR::VPDoubleRange::create(vp, getLow(),otherDouble->getHigh());
           }
         return TR::VPMergedConstraints::create(vp, this, other);
        }
     return NULL;
   }

TR::VPConstraint *TR::VPClass::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPClassType         *type         = NULL;
   TR::VPClassPresence     *presence     = NULL;
   TR::VPPreexistentObject *preexistence = NULL;
   TR::VPArrayInfo         *arrayInfo    = NULL;
   TR::VPObjectLocation    *location     = NULL;

   if (other->asClass())
      {
      TR::VPClass *otherClass = other->asClass();
      if (_type && otherClass->_type)
         type = (TR::VPClassType*)_type->merge(otherClass->_type, vp);
      if (_presence && otherClass->_presence)
         presence = (TR::VPClassPresence*)_presence->merge(otherClass->_presence, vp);
      if (_preexistence && otherClass->_preexistence)
         preexistence = _preexistence;
      if (_arrayInfo && otherClass->_arrayInfo)
         arrayInfo = (TR::VPArrayInfo*)_arrayInfo->merge(otherClass->_arrayInfo, vp);
      if (_location && otherClass->_location)
         location = (TR::VPObjectLocation*)_location->merge(otherClass->_location, vp);
      }
   else if (other->asClassType())
      {
      TR::VPClassType *otherType = other->asClassType();
      if (_type)
         type = (TR::VPClassType*)_type->merge(otherType, vp);
      }
   else if (other->asClassPresence())
      {
      if (other->isNullObject())
         {
         type = _type;
         location = _location;
         }
      TR::VPClassPresence *otherPresence = other->asClassPresence();
      if (_presence)
         presence = (TR::VPClassPresence*)_presence->merge(otherPresence, vp);
      }
   else if (other->asPreexistentObject())
      {
      if (_preexistence && (_preexistence->getAssumptionClass() == other->asPreexistentObject()->getAssumptionClass()))
         preexistence = _preexistence;
      }
   else if (other->asArrayInfo())
      {
      TR::VPArrayInfo *otherInfo = other->asArrayInfo();
      if (_arrayInfo)
         arrayInfo = (TR::VPArrayInfo*)_arrayInfo->merge(otherInfo, vp);
      }
   else if (other->asObjectLocation())
      {
      TR::VPObjectLocation *otherInfo = other->asObjectLocation();
      if (_location)
         location = (TR::VPObjectLocation*)_location->merge(otherInfo, vp);
      }
   else
      return NULL;

   if (type || presence || preexistence || arrayInfo || location)
      return TR::VPClass::create(vp, type, presence, preexistence, arrayInfo, location);
   return NULL;
   }

TR::VPConstraint *TR::VPResolvedClass::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPResolvedClass *otherClass = other->asResolvedClass();
   if (otherClass)
      {
      TR_OpaqueClassBlock *c1 = getClass();
      TR_OpaqueClassBlock *c2 = otherClass->getClass();
      if ((vp->fe()->isInstanceOf(c1, c2, false, true, true)) == TR_yes)
         {
         return otherClass;
         }
      if ((vp->fe()->isInstanceOf(c2, c1, false, true, true)) == TR_yes)
         {
         return this;
         }
      }
   else if (other->asClassPresence())
      {
      if (other->isNullObject())
         return this;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPFixedClass::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (other->asFixedClass())
      return NULL; // They can't be the same class

   TR::VPResolvedClass *otherClass = other->asResolvedClass();
   if (otherClass)
      {
      TR_OpaqueClassBlock *c1 = getClass();
      TR_OpaqueClassBlock *c2 = otherClass->getClass();
      if (vp->fe()->isInstanceOf(c1, c2, true, true, true) == TR_yes)
         return other;
      }
   else if (other->asClassPresence())
      {
      if (other->isNullObject())
         return this;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPKnownObject::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPKnownObject *otherKnownObject = other->getKnownObject();
   TR::VPConstString *otherConstString = other->asConstString();
   if (otherKnownObject)
      {
      if (getIndex() == otherKnownObject->getIndex())
         return this; // Provably the same object.  Return "this" because it is no stricter than "other"
      else
         return NULL; // Provably different objects, so now we know nothing
      }
   else if (otherConstString)
      {
      // TODO:
      // - neither of known object and const string is necessarily weaker,
      // - we should be looking at the string contents,
      // - even if they don't match, we may have, e.g. fixed class String.
      TR::KnownObjectTable *knot = vp->comp()->getKnownObjectTable();
      TR_ASSERT(knot, "Can't create a TR::VPKnownObject without a known-object table");
      if (getIndex() == knot->getExistingIndexAt((uintptr_t*)otherConstString->getSymRef()->getSymbol()->castToStaticSymbol()->getStaticAddress()))
         {
         // Now we're in an interesting position: which is stronger?  A
         // TR::VPConstString, or a TR::VPKnownObject on the same object?
         // TR::VPConstString doesn't know about object identity--only
         // contents--and there's a lot of VP machinery that kicks in for
         // strings, so that's probably better.
         //
         // HOWEVER, this has always returned the known object constraint,
         // so let's not change it just yet...
         //
         return this;
         }
      else
         {
         // Provably different objects; now we know nothing
         return NULL;
         }
      }

   // If the known object info tells us nothing, do what we can based on type info.
   //
   return Super::merge1(other, vp);
   }

TR::VPConstraint *TR::VPConstString::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (other->asConstString())
      return NULL; // can't be the same string

   TR::VPResolvedClass *otherClass = other->asResolvedClass();
   if (otherClass)
      {
      if (otherClass->getClass() == getClass())
         return other;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPArrayInfo::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPArrayInfo *otherInfo = other->asArrayInfo();
   if (!otherInfo)
      return NULL;

   int32_t lowBound = _lowBound;
   int32_t highBound = _highBound;
   int32_t elementSize = _elementSize;
   if (otherInfo->_lowBound < lowBound)
      lowBound = otherInfo->_lowBound;
   if (otherInfo->_highBound > highBound)
      highBound = otherInfo->_highBound;
   if (otherInfo->_elementSize != elementSize)
      elementSize = 0;
   if (lowBound == 0 && highBound == TR::getMaxSigned<TR::Int32>() && elementSize == 0)
      return NULL;
   return TR::VPArrayInfo::create(vp, lowBound, highBound, elementSize);
   }

TR::VPConstraint *TR::VPObjectLocation::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPObjectLocation *otherInfo = other->asObjectLocation();
   if (!otherInfo)
      return NULL;

   if (_kind == otherInfo->_kind)
      return this;

   // for now just extend to join distinct locations where they're
   // both subsets of ClassObject, i.e. in cases where previously they would
   // have both been ClassObject. However, most (or all?) combinations have a
   // natural join. Possibly TODO
   if (isKindSubset(_kind, ClassObject) && isKindSubset(otherInfo->_kind, ClassObject))
      return TR::VPObjectLocation::create(vp, ClassObject);

   return NULL;
   }

TR::VPConstraint *TR::VPSync::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPSync *otherSync = other->asVPSync();
   if (!otherSync)
      return NULL;

   if (otherSync->syncEmitted() == TR_no)
      return other;
   else if (syncEmitted() == TR_no)
      return this;
   return this;
   }

TR::VPConstraint *TR::VPMergedConstraints::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPConstraint *otherCur;
   ListElement<TR::VPConstraint> *otherNext;

   TR::VPMergedConstraints *otherList = other->asMergedConstraints();
   if (otherList)
      {
      otherNext = otherList->_constraints.getListHead();
      otherCur = otherNext->getData();
      otherNext = otherNext->getNextElement();
      }
   else
      {
      otherCur = other;
      otherNext = NULL;
      }

   if (_type.isInt16())
       return shortMerge(otherCur,otherNext,vp);
   if (_type.isInt32())
      return intMerge(otherCur, otherNext, vp);
   if (_type.isInt64())
      return longMerge(otherCur, otherNext, vp);
   if (_type.isFloat())
      return floatMerge(otherCur, otherNext, vp);
   if (_type.isDouble())
      return doubleMerge(otherCur, otherNext, vp);
   return NULL;
   }

TR::VPConstraint *TR::VPMergedConstraints::shortMerge(TR::VPConstraint * other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation * vp)
   {
   TR::VPShortConstraint *otherCur = other->asShortConstraint();

   TR_ScratchList<TR::VPConstraint>  result (vp->trMemory());
   ListElement <TR::VPConstraint> *  next   = _constraints.getListHead();
   TR::VPShortConstraint          *  cur    = next->getData()->asShortConstraint();
   ListElement<TR::VPConstraint>  *  lastResultEntry = NULL;
   TR::VPConstraint               *  mergeResult;

   if (otherCur)
      {
      next = next->getNextElement();
      while (cur || otherCur)
         {
         if (lastResultEntry &&
             lastResultEntry->getData()->asShortConstraint())
            {
            // Merge the last result entry with cur and/or otherCur
            //
            TR::VPShortConstraint *lastResult = lastResultEntry->getData()->asShortConstraint();
            if (cur && (!otherCur || cur->getLow() <= otherCur->getLow()))
               {
               if (lastResult->getHigh() == TR::getMaxSigned<TR::Int16>() || cur->getLow() <= lastResult->getHigh()+1)
                  {
                  mergeResult = lastResult->merge(cur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(cur, lastResultEntry);
                  }
               if (next)
                  {
                  cur = next->getData()->asShortConstraint();
                  TR_ASSERT(cur, "Expecting short constraints in shortMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               if (lastResult->getHigh() == TR::getMaxSigned<TR::Int16>() || otherCur->getLow() <= lastResult->getHigh()+1)
                  {
                  mergeResult = lastResult->merge(otherCur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(otherCur, lastResultEntry);
                  }
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asShortConstraint();
                  TR_ASSERT(otherCur, "Expecting short constraints in shortMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         else
            {
            // Put the lower of cur and otherCur into the result list
            //
            if (cur && (!otherCur || cur->getLow() <= otherCur->getLow()))
               {
               lastResultEntry = result.add(cur);
               if (next)
                  {
                  cur = next->getData()->asShortConstraint();
                  TR_ASSERT(cur, "Expecting short constraints in shortMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               lastResultEntry = result.add(otherCur);
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asShortConstraint();
                  TR_ASSERT(otherCur, "Expecting short constraints in shortMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();
      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
    else
      {
       TR_ASSERT(false, "Merging short with another type");
      }

   return NULL;
   }

TR::VPConstraint *TR::VPMergedConstraints::intMerge(TR::VPConstraint *other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp)
   {
   TR::VPIntConstraint *otherCur = other->asIntConstraint();
   //if (otherCur && otherCur->isUnsigned())
   //   return intMerge(otherCur, otherNext, vp, true);

   TR_ScratchList<TR::VPConstraint>         result(vp->trMemory());
   ListElement<TR::VPConstraint> *next = _constraints.getListHead();
   TR::VPIntConstraint           *cur  = next->getData()->asIntConstraint();
   ListElement<TR::VPConstraint> *lastResultEntry = NULL;
   TR::VPConstraint              *mergeResult;


   //TR_ASSERT(cur && otherCur, "Expecting int constraints in intMerge");

   if (otherCur)
      {
      next = next->getNextElement();
      while (cur || otherCur)
         {
         if (lastResultEntry &&
             lastResultEntry->getData()->asIntConstraint())
            {
            // Merge the last result entry with cur and/or otherCur
            //
            TR::VPIntConstraint *lastResult = lastResultEntry->getData()->asIntConstraint();
            if (cur && (!otherCur || cur->getLow() <= otherCur->getLow()))
               {
               if (lastResult->getHigh() == TR::getMaxSigned<TR::Int32>() || cur->getLow() <= lastResult->getHigh()+1)
                  {
                  mergeResult = lastResult->merge(cur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(cur, lastResultEntry);
                  }
               if (next)
                  {
                  cur = next->getData()->asIntConstraint();
                  TR_ASSERT(cur, "Expecting int constraints in intMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               if (lastResult->getHigh() == TR::getMaxSigned<TR::Int32>() || otherCur->getLow() <= lastResult->getHigh()+1)
                  {
                  mergeResult = lastResult->merge(otherCur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(otherCur, lastResultEntry);
                  }
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asIntConstraint();
                  TR_ASSERT(otherCur, "Expecting int constraints in intMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         else
            {
            // Put the lower of cur and otherCur into the result list
            //
            if (cur && (!otherCur || cur->getLow() <= otherCur->getLow()))
               {
               lastResultEntry = result.add(cur);
               if (next)
                  {
                  cur = next->getData()->asIntConstraint();
                  TR_ASSERT(cur, "Expecting int constraints in intMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               lastResultEntry = result.add(otherCur);
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asIntConstraint();
                  TR_ASSERT(otherCur, "Expecting int constraints in intMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();
      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }

   else
      {
      // merging an int with a long
      TR::VPLongConstraint *otherCur = other->asLongConstraint();
      if (otherCur)
         {
         next = next->getNextElement();
         while (cur || otherCur)
            {
            if (lastResultEntry)
               {
               // Merge the last result entry with cur and/or otherCur
               //
               TR::VPIntConstraint *lastResult = lastResultEntry->getData()->asIntConstraint();
               if (cur && (!otherCur || (int64_t)cur->getLow() <= (int64_t)otherCur->getLow()))
                  {
                  if (lastResult->getHigh() == TR::getMaxSigned<TR::Int32>() || cur->getLow() <= lastResult->getHigh()+1)
                     {
                     mergeResult = lastResult->merge(cur, vp);
                     if (!mergeResult)
                        return NULL;
                     lastResultEntry->setData(mergeResult);
                     }
                  else
                     {
                     lastResultEntry = result.addAfter(cur, lastResultEntry);
                     }
                  if (next)
                     {
                     cur = next->getData()->asIntConstraint();
                     TR_ASSERT(cur, "Expecting int constraints in intMerge");
                     next = next->getNextElement();
                     }
                  else
                     cur = NULL;
                  }
               else
                  {
                  if (lastResult->getHigh() == TR::getMaxSigned<TR::Int32>() || (int64_t)otherCur->getLow() <= (int64_t)lastResult->getHigh()+1)
                     {
                     mergeResult = lastResult->merge(otherCur, vp);
                     if (!mergeResult)
                        return NULL;
                     lastResultEntry->setData(mergeResult);
                     }
                  else
                     {
                     // check for the possibility of a wrap-around
                     if ((int64_t)otherCur->getLow() < (int64_t)TR::getMinSigned<TR::Int32>() ||
                         (int64_t)otherCur->getHigh() > (int64_t)TR::getMaxSigned<TR::Int32>())
                        return NULL;
                     else
                        lastResultEntry = result.addAfter(TR::VPIntRange::create(vp, (int32_t)otherCur->getLow(), (int32_t)otherCur->getHigh()), lastResultEntry);
                     }
                  if (otherNext)
                     {
                     otherCur = otherNext->getData()->asLongConstraint();
                     //TR_ASSERT(otherCur, "Expecting int constraints in intMerge");
                     otherNext = otherNext->getNextElement();
                     }
                  else
                     otherCur = NULL;
                  }
               }
            else
               {
               // Put the lower of cur and otherCur into the result list
               //
               if (cur && (!otherCur || (int64_t)cur->getLow() <= (int64_t)otherCur->getLow()))
                  {
                  lastResultEntry = result.add(cur);
                  if (next)
                     {
                     cur = next->getData()->asIntConstraint();
                     TR_ASSERT(cur, "Expecting int constraints in intMerge");
                     next = next->getNextElement();
                     }
                  else
                     cur = NULL;
                  }
               else
                  {
                  // check for the possibility of a wrap-around
                  if ((int64_t)otherCur->getLow() < (int64_t)TR::getMinSigned<TR::Int32>() ||
                      (int64_t)otherCur->getHigh() > (int64_t)TR::getMaxSigned<TR::Int32>())
                     return NULL;
                  else
                     lastResultEntry = result.add(TR::VPIntRange::create(vp, (int32_t)otherCur->getLow(), (int32_t)otherCur->getHigh()));
                  if (otherNext)
                     {
                     otherCur = otherNext->getData()->asLongConstraint();
                     TR_ASSERT(otherCur, "Expecting int constraints in intMerge");
                     otherNext = otherNext->getNextElement();
                     }
                  else
                     otherCur = NULL;
                  }
               }
            }

         lastResultEntry = result.getListHead();
         if (!lastResultEntry->getNextElement())
            return lastResultEntry->getData();
         return TR::VPMergedConstraints::create(vp, lastResultEntry);
         }
      }
   return NULL;
   }

TR::VPConstraint *TR::VPMergedConstraints::longMerge(TR::VPConstraint *other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp)
   {
   TR_ScratchList<TR::VPConstraint>         result(vp->trMemory());
   ListElement<TR::VPConstraint> *next = _constraints.getListHead();
   TR::VPLongConstraint          *cur  = next->getData()->asLongConstraint();
   ListElement<TR::VPConstraint> *lastResultEntry = NULL;
   TR::VPConstraint              *mergeResult;

   TR::VPLongConstraint *otherCur = other->asLongConstraint();

   TR_ASSERT(cur && otherCur, "Expecting long constraints in longMerge");
   if (otherCur)
      {
      next = next->getNextElement();
      while (cur || otherCur)
         {
         if (lastResultEntry)
            {
            // Merge the last result entry with cur and/or otherCur
            //
            TR::VPLongConstraint *lastResult = lastResultEntry->getData()->asLongConstraint();
            if (cur && (!otherCur || cur->getLow() <= otherCur->getLow()))
               {
               if (lastResult->getHigh() == TR::getMaxSigned<TR::Int64>() || cur->getLow() <= lastResult->getHigh()+1)
                  {
                  mergeResult = lastResult->merge(cur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(cur, lastResultEntry);
                  }
               if (next)
                  {
                  cur = next->getData()->asLongConstraint();
                  TR_ASSERT(cur, "Expecting long constraints in longMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               if (lastResult->getHigh() == TR::getMaxSigned<TR::Int64>() || otherCur->getLow() <= lastResult->getHigh()+1)
                  {
                  mergeResult = lastResult->merge(otherCur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(otherCur, lastResultEntry);
                  }
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asLongConstraint();
                  TR_ASSERT(otherCur, "Expecting long constraints in longMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         else
            {
            // Put the lower of cur and otherCur into the result list
            //
            if (cur && (!otherCur || cur->getLow() <= otherCur->getLow()))
               {
               lastResultEntry = result.add(cur);
               if (next)
                  {
                  cur = next->getData()->asLongConstraint();
                  TR_ASSERT(cur, "Expecting long constraints in longMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               lastResultEntry = result.add(otherCur);
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asLongConstraint();
                  TR_ASSERT(otherCur, "Expecting long constraints in longMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();
      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
   else
      {
      // merging a long with an int
      TR::VPIntConstraint *otherCur = other->asIntConstraint();
      if (otherCur)
         {
         next = next->getNextElement();
         while (cur || otherCur)
            {
            if (lastResultEntry)
               {
               // Merge the last result entry with cur and/or otherCur
               //
               TR::VPLongConstraint *lastResult = lastResultEntry->getData()->asLongConstraint();
               if (cur && (!otherCur || (int64_t)cur->getLow() <= (int64_t)otherCur->getLow()))
                  {
                  if (lastResult->getHigh() == TR::getMaxSigned<TR::Int64>() || cur->getLow() <= lastResult->getHigh()+1)
                     {
                     mergeResult = lastResult->merge(cur, vp);
                     if (!mergeResult)
                        return NULL;
                     lastResultEntry->setData(mergeResult);
                     }
                  else
                     {
                     lastResultEntry = result.addAfter(cur, lastResultEntry);
                     }
                  if (next)
                     {
                     cur = next->getData()->asLongConstraint();
                     TR_ASSERT(cur, "Expecting long constraints in longMerge");
                     next = next->getNextElement();
                     }
                  else
                     cur = NULL;
                  }
               else
                  {
                  int64_t otherLow = (int64_t)otherCur->getLow();
                  int64_t otherHigh = (int64_t)otherCur->getHigh();
                  TR::VPLongConstraint *otherCurLong = TR::VPLongRange::create(vp, otherLow, otherHigh);
                  if (lastResult->getHigh() == TR::getMaxSigned<TR::Int64>() || otherLow <= (int64_t)lastResult->getHigh()+1)
                     {
                     mergeResult = lastResult->merge(otherCurLong, vp);
                     if (!mergeResult)
                        return NULL;
                     lastResultEntry->setData(mergeResult);
                     }
                  else
                     {
                     lastResultEntry = result.addAfter(otherCurLong, lastResultEntry);
                     }
                  if (otherNext)
                     {
                     otherCur = otherNext->getData()->asIntConstraint();
                     //TR_ASSERT(otherCur, "Expecting long constraints in longMerge");
                     otherNext = otherNext->getNextElement();
                     }
                  else
                     otherCur = NULL;
                  }
               }
            else
               {
               // Put the lower of cur and otherCur into the result list
               //
               if (cur && (!otherCur || (int64_t)cur->getLow() <= (int64_t)otherCur->getLow()))
                  {
                  lastResultEntry = result.add(cur);
                  if (next)
                     {
                     cur = next->getData()->asLongConstraint();
                     TR_ASSERT(cur, "Expecting long constraints in longMerge");
                     next = next->getNextElement();
                     }
                  else
                     cur = NULL;
                  }
               else
                  {
                  lastResultEntry = result.add(TR::VPLongRange::create(vp, (int64_t)otherCur->getLow(), (int64_t)otherCur->getHigh()));
                  if (otherNext)
                     {
                     otherCur = otherNext->getData()->asIntConstraint();
                     //TR_ASSERT(otherCur, "Expecting long constraints in longMerge");
                     otherNext = otherNext->getNextElement();
                     }
                  else
                     otherCur = NULL;
                  }
               }
            }

         lastResultEntry = result.getListHead();
         if (!lastResultEntry->getNextElement())
            return lastResultEntry->getData();
         return TR::VPMergedConstraints::create(vp, lastResultEntry);
         }
      }
   return NULL;
   }

TR::VPConstraint *TR::VPMergedConstraints::floatMerge(TR::VPConstraint * other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation * vp)
   {
   TR::VPFloatConstraint *otherCur = other->asFloatConstraint();

   TR_ScratchList<TR::VPConstraint>  result (vp->trMemory());
   ListElement <TR::VPConstraint> *  next   = _constraints.getListHead();
   TR::VPFloatConstraint          *  cur    = next->getData()->asFloatConstraint();
   ListElement<TR::VPConstraint>  *  lastResultEntry = NULL;
   TR::VPConstraint               *  mergeResult;

   if (otherCur)
      {
      next = next->getNextElement();
      while (cur || otherCur)
         {
         if (lastResultEntry &&
             lastResultEntry->getData()->asFloatConstraint())
            {
            // Merge the last result entry with cur and/or otherCur
            //
            TR::VPFloatConstraint *lastResult = lastResultEntry->getData()->asFloatConstraint();
            if (cur && (!otherCur || FPCompare(cur->getLow(), otherCur->getLow()) <= 0))
               {
               if (FPCompare(lastResult->getHigh(), TR::getMaxFloat()) == 0 || 
                   FPCompare(cur->getLow(), lastResult->getHigh()) <= 0)
                  {
                  mergeResult = lastResult->merge(cur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(cur, lastResultEntry);
                  }
               if (next)
                  {
                  cur = next->getData()->asFloatConstraint();
                  TR_ASSERT_FATAL(cur, "Expecting float constraints in floatMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               if (FPCompare(lastResult->getHigh(), TR::getMaxFloat()) == 0 || 
                   FPCompare(otherCur->getLow(), lastResult->getHigh()) <= 0)
                  {
                  mergeResult = lastResult->merge(otherCur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(otherCur, lastResultEntry);
                  }
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asFloatConstraint();
                  TR_ASSERT(otherCur, "Expecting float constraints in floatMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         else
            {
            // Put the lower of cur and otherCur into the result list
            //
            if (cur && (!otherCur || FPCompare(cur->getLow(), otherCur->getLow()) <= 0))
               {
               lastResultEntry = result.add(cur);
               if (next)
                  {
                  cur = next->getData()->asFloatConstraint();
                  TR_ASSERT(cur, "Expecting float constraints in floatMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               lastResultEntry = result.add(otherCur);
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asFloatConstraint();
                  TR_ASSERT(otherCur, "Expecting float constraints in floatMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();
      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
    else
      {
       TR_ASSERT_FATAL(false, "Merging float with another type");
      }

   return NULL;
   }


TR::VPConstraint *TR::VPMergedConstraints::doubleMerge(TR::VPConstraint * other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation * vp)
   {
   TR::VPDoubleConstraint *otherCur = other->asDoubleConstraint();

   TR_ScratchList<TR::VPConstraint>  result (vp->trMemory());
   ListElement <TR::VPConstraint> *  next   = _constraints.getListHead();
   TR::VPDoubleConstraint         *  cur    = next->getData()->asDoubleConstraint();
   ListElement<TR::VPConstraint>  *  lastResultEntry = NULL;
   TR::VPConstraint               *  mergeResult;

   if (otherCur)
      {
      next = next->getNextElement();
      while (cur || otherCur)
         {
         if (lastResultEntry &&
             lastResultEntry->getData()->asDoubleConstraint())
            {
            // Merge the last result entry with cur and/or otherCur
            //
            TR::VPDoubleConstraint *lastResult = lastResultEntry->getData()->asDoubleConstraint();
            if (cur && (!otherCur || FPCompare(cur->getLow(), otherCur->getLow()) <= 0))
               {
               if (FPCompare(lastResult->getHigh(), TR::getMaxDouble()) == 0 || 
                   FPCompare(cur->getLow(), lastResult->getHigh()) <= 0)
                  {
                  mergeResult = lastResult->merge(cur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(cur, lastResultEntry);
                  }
               if (next)
                  {
                  cur = next->getData()->asDoubleConstraint();
                  TR_ASSERT_FATAL(cur, "Expecting double constraints in doubleMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               if (FPCompare(lastResult->getHigh(), TR::getMaxDouble()) == 0 || 
                   FPCompare(otherCur->getLow(), lastResult->getHigh()) <= 0)
                  {
                  mergeResult = lastResult->merge(otherCur, vp);
                  if (!mergeResult)
                     return NULL;
                  lastResultEntry->setData(mergeResult);
                  }
               else
                  {
                  lastResultEntry = result.addAfter(otherCur, lastResultEntry);
                  }
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asDoubleConstraint();
                  TR_ASSERT(otherCur, "Expecting double constraints in doubleMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         else
            {
            // Put the lower of cur and otherCur into the result list
            //
            if (cur && (!otherCur || FPCompare(cur->getLow(), otherCur->getLow()) <= 0))
               {
               lastResultEntry = result.add(cur);
               if (next)
                  {
                  cur = next->getData()->asDoubleConstraint();
                  TR_ASSERT(cur, "Expecting double constraints in doubleMerge");
                  next = next->getNextElement();
                  }
               else
                  cur = NULL;
               }
            else
               {
               lastResultEntry = result.add(otherCur);
               if (otherNext)
                  {
                  otherCur = otherNext->getData()->asDoubleConstraint();
                  TR_ASSERT(otherCur, "Expecting double constraints in doubleMerge");
                  otherNext = otherNext->getNextElement();
                  }
               else
                  otherCur = NULL;
               }
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();
      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
    else
      {
       TR_ASSERT_FATAL(false, "Merging double with another type");
      }

   return NULL;
   }


TR::VPConstraint *TR::VPLessThanOrEqual::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPLessThanOrEqual *otherLE = other->asLessThanOrEqual();
   if (otherLE)
      {
      if (otherLE->increment() < increment())
         return this;
      return other;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPGreaterThanOrEqual::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPGreaterThanOrEqual *otherGE = other->asGreaterThanOrEqual();
   if (otherGE)
      {
      if (otherGE->increment() > increment())
         return this;
      return other;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPEqual::merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPGreaterThanOrEqual *otherGE = other->asGreaterThanOrEqual();
   if (otherGE)
      {
      if (otherGE->increment() <= increment())
         return other;
      return NULL;
      }
   TR::VPLessThanOrEqual *otherLE = other->asLessThanOrEqual();
   if (otherLE)
      {
      if (otherLE->increment() >= increment())
         return other;
      return NULL;
      }
   return NULL;
   }

// ***************************************************************************
//
// Intersecting Value Propagation Constraints (logical AND)
//
// ***************************************************************************

TR::VPConstraint * TR::VPConstraint::intersect(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   // If this is the same constraint, just return it
   //
   if (!other)
      {
      if (vp->trace())
         traceMsg(vp->comp(), "setIntersectionFailed to true because NULL constraint found this = 0x%p, other = 0x%p\n", this, other);
      vp->setIntersectionFailed(true);
      return NULL;
      }

   if ((this == other)) // && !this->asClass())
      return this;

   // Call the intersect method on the constraint with the highest merge priority
   //
   TR::VPConstraint *result;
   if (other->priority() > priority())
      result = other->intersect1(this, vp);
   else
      result = intersect1(other, vp);

   if (vp->trace() && !result)
      {
      traceMsg(vp->comp(), "\nCannot intersect constraints:\n   ");
      print(vp->comp(), vp->comp()->getOutFile());
      traceMsg(vp->comp(), "\n   ");
      other->print(vp->comp(), vp->comp()->getOutFile());
      traceMsg(vp->comp(), "\n");
      traceMsg(vp->comp(), "priority: %d; other->priority: %d\n", priority(), other->priority());
      }

   return result;
   }


TR::VPConstraint * TR::VPConstraint::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   // Default action is to generalize the constraint
   //
   return NULL;
   }

// //unsigned version of the intersect routine below
// TR::VPConstraint *TR::VPIntConstraint::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp, bool Unsigned)
//    {
//    TR::VPIntConstraint *otherInt = other->asIntConstraint();
//     if (otherInt)
//       {
//       TR_ASSERT(isUnsigned() && other->isUnsigned(), "Expecting unsigned constraints in integer intersect\n");
//       if ((uint32_t)otherInt->getLow() < (uint32_t)getLow())
//          return otherInt->intersect(this, vp);
//       if ((uint32_t)otherInt->getHigh() <= (uint32_t)getHigh())
//          return other;
//       if ((uint32_t)otherInt->getLow() <= (uint32_t)getHigh())
//          return TR::VPIntRange::create(vp, otherInt->getLow(), getHigh(), true);
//       }
//    return NULL;
//    }

TR::VPConstraint *TR::VPShortConstraint::intersect1(TR::VPConstraint * other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPShortConstraint *otherShort = other->asShortConstraint();
   TR::VPIntConstraint *otherInt = other->asIntConstraint();
   TR::VPLongConstraint *otherLong = other->asLongConstraint();
   if (otherShort)
      {
      if (otherShort->getLow() < getLow())
         return otherShort->intersect(this, vp);
      if (otherShort->getHigh() <= getHigh())
         return other;
      if (otherShort->getLow() <= getHigh())
         return TR::VPShortRange::create(vp, otherShort->getLow(), getHigh());
      return NULL;
      }
   else if (otherInt)
      {
      int64_t lowVal, highVal;
      if ((int64_t)otherInt->getLow() < (int64_t)getLow())
         lowVal = getLow();
      else
         lowVal = otherInt->getLow();
      if ((int64_t)otherInt->getHigh() <= (int64_t)getHigh())
         highVal = otherInt->getHigh();
      else
         highVal = getHigh();

      return TR::VPShortRange::create(vp, (int16_t)lowVal, (int16_t)highVal);
      }
   else if (otherLong)
      {
      int64_t lowVal, highVal;
      if ((int64_t)otherLong->getLow() < (int64_t)getLow())
         lowVal = getLow();
      else
         lowVal = otherLong->getLow();
      if ((int64_t)otherLong->getHigh() <= (int64_t)getHigh())
         highVal = otherLong->getHigh();
      else
         highVal = getHigh();

      return TR::VPShortRange::create(vp, (int16_t)lowVal, (int16_t)highVal);
      }
   return NULL;
   }

TR::VPConstraint *TR::VPIntConstraint::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPIntConstraint *otherInt = other->asIntConstraint();
   //if (otherInt && otherInt->isUnsigned() && isUnsigned())
   //   return intersect1(other, vp, true);

   if (otherInt)
      {
      if (otherInt->getLow() < getLow())
         return otherInt->intersect(this, vp);
      if (otherInt->getHigh() <= getHigh())
         return other;
      if (otherInt->getLow() <= getHigh())
         return TR::VPIntRange::create(vp, otherInt->getLow(), getHigh());
      return NULL;
      }
   else
      {
      // due to the presence of aladd's, ints might intersect
      // with longs. take care of that here by creating an appropriate
      // IntRange
      TR::VPLongConstraint *otherLong = other->asLongConstraint();
      if (otherLong)
         {
         int64_t lowVal, highVal;
         if ((int64_t)otherLong->getLow() < (int64_t)getLow())
            lowVal = getLow();
         else
            lowVal = otherLong->getLow();
         if ((int64_t)otherLong->getHigh() <= (int64_t)getHigh())
            highVal = otherLong->getHigh();
         else
            highVal = getHigh();

         return TR::VPLongRange::create(vp, (int32_t)lowVal, (int32_t)highVal);
         }
      }
   return NULL;
   }

TR::VPConstraint *TR::VPLongConstraint::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPLongConstraint *otherLong = other->asLongConstraint();
   if (otherLong)
      {
      if (otherLong->getLow() < getLow())
         return otherLong->intersect(this, vp);
      if (otherLong->getHigh() <= getHigh())
         return other;
      if (otherLong->getLow() <= getHigh())
         return TR::VPLongRange::create(vp, otherLong->getLow(), getHigh());
      return NULL;
      }
   else
      {
      // see comment on aladd's above
      TR::VPIntConstraint *otherInt = other->asIntConstraint();
      if (otherInt)
         {
         if ((int64_t)otherInt->getLow() < (int64_t)getLow())
            return otherInt->intersect(this, vp);
         if ((int64_t)otherInt->getHigh() <= (int64_t)getHigh())
            return TR::VPLongRange::create(vp, getLow(), otherInt->getHigh());
         if ((int64_t)otherInt->getLow() <= (int64_t)getHigh())
            {
            if ((int64_t)getHigh() > (int64_t)TR::getMaxSigned<TR::Int32>())
               return TR::VPLongRange::create(vp, otherInt->getLow(), TR::getMaxSigned<TR::Int32>());
            else
               return TR::VPLongRange::create(vp, otherInt->getLow(), getHigh());
            }
         return NULL;
         }
      }
   return NULL;
   }


/* Take the intersect of `this` and `other` constraints
 * If one contains the other, return the smaller one
 * If they are mutually inclusive, return a new Constraint of their intersection 
 * If they are mutually exclusive, return NULL
 */
TR::VPConstraint *TR::VPFloatConstraint::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPFloatConstraint *otherFloat = other->asFloatConstraint();
   if (otherFloat)
      {
      if (FPCompare(otherFloat->getLow(), getLow()) == -1)
         return otherFloat->intersect(this, vp);
      if (FPCompare(otherFloat->getHigh(), getHigh()) == -1)
         return other;
      if (FPCompare(otherFloat->getLow(), getHigh()) == -1)
         return TR::VPFloatRange::create(vp, otherFloat->getLow(), getHigh());
      return NULL;
      }
   return NULL;
   }


/* Take the intersect of `this` and `other` constraints
 * If one contains the other, return the smaller one
 * If they are mutually inclusive, return a new Constraint of their intersection 
 * If they are mutually exclusive, return NULL
 */
TR::VPConstraint *TR::VPDoubleConstraint::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPDoubleConstraint *otherDouble = other->asDoubleConstraint();
   if (otherDouble)
      {
      if (FPCompare(otherDouble->getLow(), getLow()) == -1)
         return otherDouble->intersect(this, vp);
      if (FPCompare(otherDouble->getHigh(), getHigh()) == -1)
         return other;
      if (FPCompare(otherDouble->getLow(), getHigh()) == -1)
         return TR::VPDoubleRange::create(vp, otherDouble->getLow(), getHigh());
      return NULL;
      }
   return NULL;
   }


TR::VPClassType *TR::VPClassType::classTypesCompatible(TR::VPClassType * otherType, OMR::ValuePropagation *vp)
   {
   TR::VPResolvedClass *rc = asResolvedClass();
   TR::VPResolvedClass *otherRc = otherType->asResolvedClass();
   if (rc && otherRc && !rc->isFixedClass() && !otherRc->isFixedClass())
      {
      TR_OpaqueClassBlock *c1 = rc->getClass();
      TR_OpaqueClassBlock *c2 = otherRc->getClass();

      if (!TR::Compiler->cls.isInterfaceClass(vp->comp(), c1) && !TR::Compiler->cls.isInterfaceClass(vp->comp(), c2))
         {
         TR_YesNoMaybe ans = vp->fe()->isInstanceOf(c1, c2, false, false, true);
         if (ans == TR_no)
            return NULL;

         ans = vp->fe()->isInstanceOf(c1, c2, true, true, true);

         if (ans == TR_yes)
            return this;

         ans = vp->fe()->isInstanceOf(c2, c1, true, true, true);

         if (ans == TR_yes)
            return otherType;
         }

      return this;
      }
   else
      {
      return (TR::VPClassType*)this->intersect(otherType, vp);
      }
   }




// this routine encapsulates code that used to exist
// in VPClass::intersect
// it is called directly by handlers for instanceOf, checkCast
void TR::VPClass::typeIntersect(TR::VPClassPresence* &presence, TR::VPClassType* &type, TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (type && TR::VPConstraint::isSpecialClass((uintptr_t)type->getClass()))
      type = NULL;

   if (other->asClass())
      {
      TR::VPClass *otherClass = other->asClass();
      bool classTypeFound = false;
      bool constrainPresence = true;
      if (_presence)
         {
         if (otherClass->_presence)
            {
            presence = (TR::VPClassPresence*)_presence->intersect(otherClass->_presence, vp);
            if (!presence)
               constrainPresence = false;
            }
         }
      else
         presence = otherClass->_presence;

      if (!constrainPresence)
         return;

      if (presence && presence->isNullObject())
         return;

      if (otherClass->_type && TR::VPConstraint::isSpecialClass((uintptr_t)otherClass->_type->getClass()))
         type = NULL;
      else if (type)
         {
         // any type intersecting with the specialClass will result
         // in no type
         if (TR::VPConstraint::isSpecialClass((uintptr_t)type->getClass()))
            type = NULL;
         else if (otherClass->_type)
            {
            TR::VPClassType *otherType = otherClass->_type;
            ///if (otherType && otherType->isClassObject() == TR_yes)
            ///   dumpOptDetails(vp->comp(), "type is classobject: %d\n", TR_yes);
            //
            // the following cases are due to the fact that for loadaddrs
            // and ialoads<vft-symbol> vp keeps track of the underlying type ;
            // but these are actually of type java/lang/Class. when confronted
            // with the intersection between a loadaddr (whose underlying type is A)
            // and an aload (whose actual type is java/lang/Class), vp cannot intersect
            // types (when it should succeed) causing it to do wrong things
            // like fold branches etc. to detect this scenario, loadaddrs and
            // iaload<vft-symbols> are primed with a ClassObject property.
            //
            // case 1: VPClass wrapper intersects with a VPClass wrapper (i)
            //e.g. <fixedClass, classObject> with <resolvedClass, non-null>
            if (_location && _location->isClassObject() == TR_yes)
               {
               if (otherType->asResolvedClass())
                  {
                  TR::VPResolvedClass *rc = otherType->asResolvedClass();
                  if (rc->getClass() == vp->fe()->getClassClassPointer(rc->getClass()))
                     {
                     if (vp->trace())
                        {
                        traceMsg(vp->comp(), "   1Intersecting type is a class object\n");
                        otherType->print(vp->comp(), vp->comp()->getOutFile());
                        traceMsg(vp->comp(), "\n");
                        }

                     // this means otherType could be a java/lang/Class
                     // actual intersection of the types below is going to fail & return
                     // NULL because _type cannot be an instanceOf java/lang/Class
                     // (since java/lang/Class is final) ; so type becomes otherType
                     //
                     // addendum: dont propagate the type, since the constraint can
                     // be identified using the classobject property anyway
                     ///type = otherType;
                     classTypeFound = true;
                     }
                  //case 2: VPClass wrapper intersects with a VPClass wrapper (ii)
                  //e.g. <fixedClass, classObject> with <resolvedClass, classObject>
                  else if (otherClass->_location && otherClass->_location->isClassObject() == TR_yes)
                     {
                     TR::VPResolvedClass *rc = type->asResolvedClass();
                     if (rc && rc->getClass() == vp->fe()->getClassClassPointer(rc->getClass()))
                        {
                        if (vp->trace())
                           {
                           traceMsg(vp->comp(), "   Current type is a class object\n");
                           this->print(vp->comp(), vp->comp()->getOutFile());
                           traceMsg(vp->comp(), "\n");
                           }
                        // resulting type is this type
                        //////type = _type;
                        classTypeFound = true;
                        }
                     }
                  }
               }
            //case 3: VPClass wrapper intersects with a VPClass wrapper (iii)
            //e.g. <fixedClass, non-null> with <resolvedClass, classObject>
            else if (otherClass->_location && otherClass->_location->isClassObject() == TR_yes)
               {
               TR::VPResolvedClass *rc = type->asResolvedClass();
               if (rc && rc->getClass() == vp->fe()->getClassClassPointer(rc->getClass()))
                  {
                  if (vp->trace())
                     {
                     traceMsg(vp->comp(), "   2Intersecting type is a class object\n");
                     this->print(vp->comp(), vp->comp()->getOutFile());
                     traceMsg(vp->comp(), "\n");
                     }
                  // resulting type is this type
                  ///////type = _type;
                  classTypeFound = true;
                  }
               }
            // now do the actual type intersection
            if (!classTypeFound)
               {
               type = type->classTypesCompatible(otherType, vp);
               }
            }
         }
      else
         type = otherClass->_type;
      }
   else if (other->asClassPresence())
      {
      TR::VPClassPresence *otherPresence = other->asClassPresence();
      if (_presence)
         {
         presence = (TR::VPClassPresence*)_presence->intersect(otherPresence, vp);
         }
      else
         presence = otherPresence;
      }
   else if (other->asClassType())
      {
      bool classTypeFound = false;
      TR::VPClassType *otherType = other->asClassType();
      if (TR::VPConstraint::isSpecialClass((uintptr_t)otherType->getClass()))
         type = NULL;
      else if (type)
         {
         ///if (otherType && otherType->isClassObject() == TR_yes)
         ///   dumpOptDetails(vp->comp(), "type is classobject: %d\n", TR_yes);
         if (TR::VPConstraint::isSpecialClass((uintptr_t)type->getClass()))
            {
            type = NULL;
            classTypeFound = true;
            }
         //
         //case 4: VPClass wrapper intersects with a VPClassType
         //e.g. <fixedClass, classObject> with <resolvedClass>
         else if (_location && _location->isClassObject() == TR_yes)
            {
            if (otherType && otherType->asResolvedClass())
               {
               TR::VPResolvedClass *rc = otherType->asResolvedClass();
               if (rc->getClass() == vp->fe()->getClassClassPointer(rc->getClass()))
                  {
                  if (vp->trace())
                     {
                     traceMsg(vp->comp(), "   Intersecting type is a class object\n");
                     otherType->print(vp->comp(), vp->comp()->getOutFile());
                     traceMsg(vp->comp(), "\n");
                     }
                  // resulting type is the otherType
                  //
                  // addendum: dont propagate the type, since the constraint can
                  // be identified using the classobject property anyway
                  ///type = otherType;
                  classTypeFound = true;
                  }
               }
            }
         if (!classTypeFound)
            type = type->classTypesCompatible(otherType, vp);
         }
      else
         type = otherType;
      }
   }

TR::VPConstraint *TR::VPClass::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPClassType         *type          = _type;
   TR::VPClassPresence     *presence      = _presence;
   TR::VPPreexistentObject *preexistence  = _preexistence;
   TR::VPArrayInfo         *arrayInfo     = _arrayInfo;
   TR::VPObjectLocation    *location      = _location;

   const bool thisClassObjectYes = isClassObject() == TR_yes;
   const bool otherClassObjectYes = other->isClassObject() == TR_yes;
   if (thisClassObjectYes != otherClassObjectYes)
      {
      // One is known to be a class object and the other isn't, so don't try to
      // intersect the types (which have different meanings). The intersection
      // is a class of some kind (if nonempty).
      TR::VPConstraint *classObj = this, *nonClassObj = other;
      if (otherClassObjectYes)
         std::swap(classObj, nonClassObj);

      TR::VPObjectLocation *classLoc = classObj->getObjectLocation();
      TR_ASSERT(classLoc != NULL, "classObj has no location\n");
      if (classObj->isNullObject()
          || nonClassObj->isNullObject()
          || nonClassObj->isClassObject() == TR_no
          || (classLoc->isJavaLangClassObject() == TR_yes && nonClassObj->isJavaLangClassObject() == TR_no)
          || (classLoc->isJ9ClassObject() == TR_yes && nonClassObj->isJ9ClassObject() == TR_no))
         {
         // Referent can only be null.
         if (isNonNullObject() || other->isNonNullObject())
            return NULL;
         else
            return TR::VPNullObject::create(vp);
         }

      // At this point we know the nonClassObj permits classLoc.
      location = classLoc;

      // classLoc means that any associated TR::VPClass constrains the
      // represented class, about which nonClassObj has no information.
      // Make sure to avoid propagating the "special class" constraint.
      type = classObj->getClassType();
      if (type != NULL && isSpecialClass((uintptr_t)type->getClass()))
         type = NULL;

      arrayInfo = NULL; // doesn't apply to classes
      presence = NULL;
      if (isNonNullObject() || other->isNonNullObject())
         presence = TR::VPNonNullObject::create(vp);
      }
   else if (other->asClass())
      {
      TR::VPClass *otherClass = other->asClass();
      typeIntersect(presence, type, otherClass, vp);
      // If intersection of presence failed, return null
      if (!presence && _presence && otherClass->_presence)
         return NULL;
      // If the resulting object is null, return
      //
      else if (presence && presence->isNullObject())
         return presence;
      // If intersection of types failed, return null
      if (!type && _type && otherClass->_type)
         {
         if (TR::VPConstraint::isSpecialClass((uintptr_t)_type->getClass()) ||
               TR::VPConstraint::isSpecialClass((uintptr_t)otherClass->_type->getClass()))
            ; // do nothing
         else if ((_presence && _presence->isNonNullObject()) || (other->asClassPresence() && other->asClassPresence()->isNonNullObject()))
            return NULL;
         else
            return TR::VPNullObject::create(vp);
         }

      if (_preexistence)
         preexistence = _preexistence;
      else
         preexistence = otherClass->_preexistence;

      if (_arrayInfo)
         {
         if (otherClass->_arrayInfo)
            {
            arrayInfo = (TR::VPArrayInfo*)_arrayInfo->intersect(otherClass->_arrayInfo, vp);
            if (!arrayInfo)
               return NULL;
            }
         }
      else
         arrayInfo = otherClass->_arrayInfo;

      if (_location)
         {
         if (otherClass->_location)
            {
            location = (TR::VPObjectLocation *)_location->intersect(otherClass->_location, vp);
            if (!location)
               return NULL;
            }
         }
      else
         location = otherClass->_location;
      }

   else if (other->asClassType())
      {
      TR::VPClassType *otherType = other->asClassType();
      // We get the normal interpretation of _type (type of an instance).
      TR::VPClassPresence *p = NULL;
      typeIntersect(p, type, other, vp);
      // If intersection of types failed, return null
      if (!type && _type && otherType)
         {
         if (TR::VPConstraint::isSpecialClass((uintptr_t)_type->getClass()) ||
               TR::VPConstraint::isSpecialClass((uintptr_t)otherType->getClass()))
            ; // do nothing
         else if (_presence && _presence->isNonNullObject())
            return NULL;
         else
            return TR::VPNullObject::create(vp);
         }

      // code below moved to typeIntersect
#if 0
      if (_type)
         {
         ///if (otherType && otherType->isClassObject() == TR_yes)
         ///   dumpOptDetails(vp->comp(), "type is classobject: %d\n", TR_yes);
         //case 4: VPClass intersects with a VPClassType
         //e.g. <fixedClass,classObject> with <resolvedClass>
         if (_location && _location->isClassObject() == TR_yes)
            {
            if (otherType && otherType->asResolvedClass())
               {
               TR::VPResolvedClass *rc = otherType->asResolvedClass();
               if (rc->getClass() == vp->fe()->getClassClassPointer(rc->getClass()))
                  {
                  if (vp->trace())
                     {
                     traceMsg(vp->comp(), "   Intersecting type is a class object\n");
                     otherType->print(vp->comp(), vp->comp()->getOutFile());
                     traceMsg(vp->comp(), "\n");
                     }
                  return other;
                  }
               }
            }
         type = (TR::VPClassType*)_type->intersect(otherType, vp);
         if (!type)
            return NULL;
         }
      else
         type = otherType;
#endif
      }

   else if (other->asClassPresence())
      {
      TR::VPClassType *t = NULL;
      typeIntersect(presence, t, other, vp);
      // If intersection of presence failed, return null
      if (!presence && _presence && other->asClassPresence())
         return NULL;
      // If the result is null, just return null
      //
      if (presence && presence->isNullObject())
         return presence;

      // code below moved to typeIntersect
#if 0
      TR::VPClassPresence *otherPresence = other->asClassPresence();
      if (_presence)
         {
         presence = (TR::VPClassPresence*)_presence->intersect(otherPresence, vp);
         if (!presence)
            return NULL;
         }
      else
         presence = otherPresence;

      // If the result is null, just return null
      //
      if (presence && presence->isNullObject())
         return presence;
#endif
      }

   else if (other->asPreexistentObject())
      {
      if (!_preexistence)
         preexistence = other->asPreexistentObject();
      else if (_preexistence->getAssumptionClass() != other->asPreexistentObject()->getAssumptionClass())
         preexistence = NULL;
      }

   else if (other->asArrayInfo())
      {
      TR::VPArrayInfo *otherInfo = other->asArrayInfo();
      if (_arrayInfo)
         {
         arrayInfo = (TR::VPArrayInfo*)_arrayInfo->intersect(otherInfo, vp);
         if (!arrayInfo)
            return NULL;
         }
      else
         arrayInfo = otherInfo;
      }
   else if (other->asObjectLocation())
      {
      TR::VPObjectLocation *otherInfo = other->asObjectLocation();
      if (_location)
         {
         location = (TR::VPObjectLocation *)_location->intersect(otherInfo, vp);
         if (!location)
            return NULL;
         }
      else
         location = otherInfo;
      }
   else
      return NULL;

   if (type || presence || preexistence || arrayInfo || location)
      return TR::VPClass::create(vp, type, presence, preexistence, arrayInfo, location);
   return NULL;
   }

TR::VPConstraint *TR::VPResolvedClass::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPResolvedClass *otherRes = other->asResolvedClass();
   if (otherRes)
      {
      TR_OpaqueClassBlock *c1 = getClass();
      TR_OpaqueClassBlock *c2 = otherRes->getClass();
      if ((vp->fe()->isInstanceOf(c2, c1, false, true, true)) == TR_yes)
         {
         return otherRes;
         }
      }
   else if (other->asUnresolvedClass())
      {
      if (isJavaLangObject(vp))
         return other;

      // Make sure they are both arrays or both classes
      //
      int32_t len = 0, thisLen, otherLen;
      const char *thisSig = getClassSignature(thisLen);
      const char *otherSig = other->getClassSignature(otherLen);
      if (*thisSig == *otherSig)
         {
         while (*thisSig == '[')
            {
            if (*otherSig != *thisSig)
               {
                 if (!((otherLen == 21 && !strncmp(otherSig, "Ljava/lang/Cloneable;", 21)) ||
                       (otherLen == 22 && !strncmp(otherSig, "Ljava/io/Serializable;", 22)) ||
                       (otherLen == 18 && !strncmp(otherSig, "Ljava/lang/Object;", 18))))
                  return NULL;
               break;
               }
            thisSig++;
            otherSig++;
            otherLen--;
            }

         if (((*thisSig != 'L') && (*thisSig != '[')) &&
             ((*otherSig == 'L') || (*otherSig == '[')))
            return NULL;

         return this;
         }
      // JLS Section 10.7: "Every array implements the interfaces Cloneable and java.io.Serializable"
      else if (*getClassSignature(len) == '[' && other->asUnresolvedClass()->isCloneableOrSerializable())
         return this;
      else if (((thisLen == 21 && !strncmp(thisSig, "Ljava/lang/Cloneable;", 21)) ||
                (thisLen == 22 && !strncmp(thisSig, "Ljava/io/Serializable;", 22))) &&
               (*otherSig == '['))
         {
         return other;
         }
          // Plumhall rtresolve failure, every array is instance of java/lang/Object
      else if (*getClassSignature(len) == '[' && other->asUnresolvedClass()->isJavaLangObject(vp))
         return this;
      return NULL;
      }
   else if (other->asClassPresence())
      {
      if (other->isNullObject())
         return other;
      return TR::VPClass::create(vp, this, other->asClassPresence(), NULL, NULL, NULL);
      }
   else if (other->asPreexistentObject())
      return TR::VPClass::create(vp, this, NULL, other->asPreexistentObject(), NULL, NULL);
   else if (other->asArrayInfo())
      return TR::VPClass::create(vp, this, NULL, NULL, other->asArrayInfo(), NULL);
   else if (other->asObjectLocation())
      {
      TR::VPObjectLocation *location = other->asObjectLocation();
      TR_YesNoMaybe classObject = isClassObject();
      if (classObject != TR_maybe)
         {
         location = TR::VPObjectLocation::create(vp, classObject == TR_yes ?
                                                TR::VPObjectLocation::ClassObject : TR::VPObjectLocation::NotClassObject);
         location = (TR::VPObjectLocation *) location->intersect(other->asObjectLocation(), vp);
         if (!location) return NULL;
         }
      return TR::VPClass::create(vp, this, NULL, NULL, NULL, location);
      }
   return this;
   }

TR::VPConstraint *TR::VPFixedClass::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPResolvedClass *otherClass = other->asResolvedClass();
   if (otherClass)
      {
      TR_OpaqueClassBlock *c1 = getClass();
      TR_OpaqueClassBlock *c2 = otherClass->getClass();
      if (vp->fe()->isInstanceOf(c1, c2, true, true, true) != TR_no)
         return this;
      }
   else if (other->asUnresolvedClass())
      {
      // Make sure they are both arrays or both classes
      //
      int32_t len = 0, thisLen, otherLen;
      const char *thisSig = getClassSignature(thisLen);
      const char *otherSig = other->getClassSignature(otherLen);

      // if the current is a java.lang.Object
      // then the other could be a class or an array
      if (isJavaLangObject(vp))
         {
         if (*otherSig == '[')
            {
            while (*otherSig != '[')
               otherSig++;
            if (!((otherLen == 21 && !strncmp(otherSig, "Ljava/lang/Cloneable;", 21)) ||
                  (otherLen == 22 && !strncmp(otherSig, "Ljava/io/Serializable;", 22)) ||
                  (otherLen == 18 && !strncmp(otherSig, "Ljava/lang/Object;", 18))))
               return NULL;
            }
         else if (!(other->asUnresolvedClass()->isCloneableOrSerializable() ||
               other->asUnresolvedClass()->isJavaLangObject(vp)))
            return NULL;
         //return other;
         }

      if (*thisSig == *otherSig)
         {
         while (*thisSig == '[')
            {
            if (*otherSig != *thisSig)
               {
               if (!((otherLen == 21 && !strncmp(otherSig, "Ljava/lang/Cloneable;", 21)) ||
                     (otherLen == 22 && !strncmp(otherSig, "Ljava/io/Serializable;", 22)) ||
                     (otherLen == 18 && !strncmp(otherSig, "Ljava/lang/Object;", 18))))
                  return NULL;
               break;
               }
            thisSig++;
            otherSig++;
            otherLen--;
            }

         if ((*thisSig != 'L') && ((*otherSig == 'L') || (*otherSig == '[')))
            {
            if (! ((*thisSig == '[') && (otherLen == 18 && !strncmp(otherSig, "Ljava/lang/Object;", 18))) )
               return NULL;
            }

         // retain the fact that its a fixed type
         return this;
         //return TR::VPResolvedClass::create(vp, _class);
         }
      // JLS Section 10.7: "Every array implements the interfaces Cloneable and java.io.Serializable"
      else if (*getClassSignature(len) == '[' && other->asUnresolvedClass()->isCloneableOrSerializable())
         return this;
          // Plumhall rtresolve failure, every array is instance of java/lang/Object
      else if (*getClassSignature(len) == '[' && other->asUnresolvedClass()->isJavaLangObject(vp))
         return this;
      return NULL;
      }
   else if (other->asClassPresence())
      {
      if (other->isNullObject())
         return other;
      return TR::VPClass::create(vp, this, other->asClassPresence(), NULL, NULL, NULL);
      }
   else if (other->asPreexistentObject())
      return TR::VPClass::create(vp, this, NULL, other->asPreexistentObject(), NULL, NULL);
   else if (other->asArrayInfo())
      return TR::VPClass::create(vp, this, NULL, NULL, other->asArrayInfo(), NULL);
   else if (other->asObjectLocation())
      {
      TR::VPObjectLocation *location = other->asObjectLocation();
      TR_YesNoMaybe classObject = isClassObject();
      if (classObject != TR_maybe)
         {
         location = TR::VPObjectLocation::create(vp, classObject == TR_yes ?
                                                TR::VPObjectLocation::ClassObject : TR::VPObjectLocation::NotClassObject);
         location = (TR::VPObjectLocation *) location->intersect(other->asObjectLocation(), vp);
         if (!location) return NULL;
         }
      return TR::VPClass::create(vp, this, NULL, NULL, NULL, location);
      }
   return NULL;
   }

TR::VPConstraint *TR::VPKnownObject::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPKnownObject *otherKnownObject = other->getKnownObject();
   if (otherKnownObject)
      {
      if (getIndex() == otherKnownObject->getIndex())
         return other; // Provably the same object.  Return "other" because it is no less strict than "this"
      else
         return NULL;  // Impossible: provably different objects
      }

   TR::VPConstString *otherConstString = other->getConstString();
   if (otherConstString)
      {
      // TODO:
      // - we should be looking at the string contents,
      // - known object should be more specific (though it allows null).
      TR::KnownObjectTable *knot = vp->comp()->getKnownObjectTable();
      TR_ASSERT(knot, "Can't create a TR::VPKnownObject without a known-object table");
      if (getIndex() == knot->getOrCreateIndexAt((uintptr_t*)otherConstString->getSymRef()->getSymbol()->castToStaticSymbol()->getStaticAddress()))
         return other; // A const string constraint is more specific than known object
      else
         return NULL; // Two different objects
      }

   // If the known object info tells us nothing, do what we can based on type info.
   //
   return Super::intersect1(other, vp);
   }

TR::VPConstraint *TR::VPConstString::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
#ifdef J9_PROJECT_SPECIFIC
   TRACER(vp, this, other);

   if (other->asConstString())
      return NULL; // can't be the same string

   TR::VPResolvedClass *otherClass = other->asResolvedClass();
   if (otherClass)
      {
      TR_OpaqueClassBlock *c1 = getClass();
      TR_OpaqueClassBlock *c2 = otherClass->getClass();
	if (vp->fe()->isInstanceOf(c1, c2, true, true, true) != TR_no)
         return this;
      }
   else if (other->asUnresolvedClass())
     {
      // Make sure they are both arrays or both classes
      //
      int32_t len = 0, otherLen;
      const char thisSig[] = "Ljava/lang/String;";
      const char *otherSig = other->getClassSignature(otherLen);
      if (*thisSig == *otherSig)
         return TR::VPResolvedClass::create(vp, vp->comp()->getStringClassPointer());

      return NULL;
      }
   else if (other->asClassPresence())
      {
      if (other->isNullObject() &&
          !isNonNullObject())
         return other;

      if (!other->isNullObject() ||
          !isNonNullObject())
         return this;
      }
   else if (other->asObjectLocation())
      {
      TR::VPObjectLocation *location = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::HeapObject);
      location = (TR::VPObjectLocation*)location->intersect(other->asObjectLocation(), vp);
      if (!location) return NULL;
      return TR::VPClass::create(vp, this, NULL, NULL, NULL, location);
      }
#endif

   return NULL;
   }

TR::VPConstraint *TR::VPUnresolvedClass::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (other->asClassPresence())
      {
      if (other->isNullObject())
         return other;
      return TR::VPClass::create(vp, this, other->asClassPresence(), NULL, NULL, NULL);
      }
   else if (other->asPreexistentObject())
      return TR::VPClass::create(vp, this, NULL, other->asPreexistentObject(), NULL, NULL);
   else if (other->asArrayInfo())
      return TR::VPClass::create(vp, this, NULL, NULL, other->asArrayInfo(), NULL);
   else if (other->asObjectLocation())
      return TR::VPClass::create(vp, this, NULL, NULL, NULL, other->asObjectLocation()); // FIXME: unless rtresolve, should be on heap
   return this;
   }

TR::VPConstraint *TR::VPNullObject::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (isNullObject())
      return this;
   if (other->asPreexistentObject())
      return TR::VPClass::create(vp, NULL, this, other->asPreexistentObject(), NULL, NULL);
   if (other->asArrayInfo())
      return TR::VPClass::create(vp, NULL, this, NULL, other->asArrayInfo(), NULL);
   if (other->asObjectLocation())
      {
      if (other->isStackObject() == TR_yes ||
          other->isHeapObject()  == TR_yes ||
          other->isClassObject() == TR_yes)
         return NULL;
      return this; // no advantage in knowing that a null object is not on the heap, etc
      }
   return NULL;
   }

TR::VPConstraint *TR::VPNonNullObject::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (other->asPreexistentObject())
      return TR::VPClass::create(vp, NULL, this, other->asPreexistentObject(), NULL, NULL);
   if (other->asArrayInfo())
      return TR::VPClass::create(vp, NULL, this, NULL, other->asArrayInfo(), NULL);
   if (other->asObjectLocation())
      return TR::VPClass::create(vp, NULL, this, NULL, NULL, other->asObjectLocation());
   return NULL;
   }

TR::VPConstraint *TR::VPPreexistentObject::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (other->asArrayInfo())
      return TR::VPClass::create(vp, NULL, NULL, this, other->asArrayInfo(), NULL);
   if (other->asObjectLocation())
      return TR::VPClass::create(vp, NULL, NULL, this, NULL, other->asObjectLocation());
   return NULL;
   }

TR::VPConstraint *TR::VPArrayInfo::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   if (other->asObjectLocation())
      {
      TR::VPObjectLocation *location = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotClassObject);
      location = (TR::VPObjectLocation *) location->intersect(other->asObjectLocation(), vp);
      if (!location) return NULL;
      return TR::VPClass::create(vp, NULL, NULL, NULL, this, location);
      }

   TR::VPArrayInfo *otherInfo = other->asArrayInfo();
   if (!otherInfo)
      return NULL;

   int32_t lowBound = _lowBound;
   int32_t highBound = _highBound;
   int32_t elementSize = _elementSize;
   if (otherInfo->_lowBound > lowBound)
      lowBound = otherInfo->_lowBound;
   if (otherInfo->_highBound < highBound)
      highBound = otherInfo->_highBound;
   if (otherInfo->_elementSize)
      {
      if (!elementSize)
         elementSize = otherInfo->_elementSize;
      else if (elementSize != otherInfo->_elementSize)
         return NULL;
      }
   if (lowBound == 0 && highBound == TR::getMaxSigned<TR::Int32>() && elementSize == 0)
      return NULL;
   return TR::VPArrayInfo::create(vp, lowBound, highBound, elementSize);
   }

TR::VPConstraint *TR::VPObjectLocation::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPObjectLocation *otherInfo = other->asObjectLocation();
   if (!otherInfo)
      return NULL;

   VPObjectLocationKind result =
      (VPObjectLocationKind)(_kind & otherInfo->_kind);

   //FIXME: since loadaddrs (or ialoads of vft-symbols) are primed
   //with a ClassObject property, we could intersect ClassObject with a HeapObject
   //Leaving this here for now, in all cases where we previously would
   //have had ClassObject (and where we now have a subset of ClassObject).
   if ((_kind == HeapObject && isKindSubset(otherInfo->_kind, ClassObject)) ||
       (isKindSubset(_kind, ClassObject) && otherInfo->_kind == HeapObject))
      result = HeapObject;

   if (result == _kind)
      return this;
   else if (result == otherInfo->_kind)
      return otherInfo;
   else if (result == (VPObjectLocationKind)0)
      return NULL;
   else
      return TR::VPObjectLocation::create(vp, result);
   }

TR::VPConstraint *TR::VPSync::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   //FIXME
   TR::VPSync *otherSync = other->asVPSync();
   if (!otherSync)
      return NULL;

   if ((syncEmitted() == TR_maybe && otherSync->syncEmitted() == TR_yes) ||
       (syncEmitted() == TR_yes && otherSync->syncEmitted() == TR_maybe))
      return TR::VPSync::create(vp, TR_no);

   if ((syncEmitted() == TR_maybe && otherSync->syncEmitted() == TR_no) ||
       (syncEmitted() == TR_no && otherSync->syncEmitted() == TR_maybe))
      return TR::VPSync::create(vp, TR_yes);

   return NULL;
   }

TR::VPConstraint *TR::VPMergedConstraints::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPConstraint *otherCur;
   ListElement<TR::VPConstraint> *otherNext;

   TR::VPMergedConstraints *otherList = other->asMergedConstraints();
   if (otherList)
      {
      otherNext = otherList->_constraints.getListHead();
      otherCur = otherNext->getData();
      otherNext = otherNext->getNextElement();
      }
   else
      {
      otherCur = other;
      otherNext = NULL;
      }

   if (_type.isInt16())
      return shortIntersect(otherCur,otherNext,vp);
   if (_type.isInt32())
      return intIntersect(otherCur, otherNext, vp);
   if (_type.isInt64())
      return longIntersect(otherCur, otherNext, vp);
   if (_type.isFloat())
      return floatIntersect(otherCur, otherNext, vp);
   if (_type.isDouble())
      return doubleIntersect(otherCur, otherNext, vp);

   return NULL;
   }

//this is the unsigned version of the
//intersect routine below
// TR::VPConstraint *TR::VPMergedConstraints::intIntersect(TR::VPIntConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp, bool isUnsigned)
//    {
//    TR_ScratchList<TR::VPConstraint>         result(vp->trMemory());
//    ListElement<TR::VPConstraint> *next = _constraints.getListHead();
//    TR::VPIntConstraint           *cur  = next->getData()->asIntConstraint();
//    ListElement<TR::VPConstraint> *lastResultEntry = NULL;

//    //TR::VPIntConstraint *otherCur = other->asIntConstraint();

//    //TR_ASSERT(cur && otherCur, "Expecting int constraints in intIntersect");

//    //if (otherCur)
//       {
//       uint32_t curLow = (uint32_t)cur->getLow();
//       uint32_t curHigh = (uint32_t)cur->getHigh();
//       uint32_t otherLow = (uint32_t)otherCur->getLow();
//       uint32_t otherHigh = (uint32_t)otherCur->getHigh();

//       next = next->getNextElement();
//       while (cur && otherCur)
//          {
//          bool skipCur      = false;
//          bool skipOtherCur = false;

//          TR_ASSERT(cur->isUnsigned() && otherCur->isUnsigned(), "Expecting unsigned int constraints");
//          // If the two current ranges do not overlap, skip the lower range and
//          // try again
//          //
//          if (curHigh < otherLow)
//             skipCur = true;
//          else if (otherHigh < curLow)
//             skipOtherCur = true;

//          else
//             {
//             // Put the intersection of the two current ranges into the result list
//             //
//             uint32_t resultLow = (curLow > otherLow) ? curLow : otherLow;
//             uint32_t resultHigh = (curHigh < otherHigh) ? curHigh : otherHigh;
//             TR::VPConstraint *constraint = TR::VPIntRange::create(vp, resultLow, resultHigh, true);
//             ///if (constraint)
//             ///   constraint->setIsUnsigned(true);
//             lastResultEntry = result.addAfter(constraint, lastResultEntry);

//             // Reduce the two current ranges. If either is exhausted, move to the
//             // next
//             //
//             if (resultHigh == TR::getMaxUnsigned<TR::Int32>())
//                break;
//             curLow = otherLow = resultHigh+1;
//             if (curLow > curHigh)
//                skipCur = true;
//             if (otherLow > otherHigh)
//                skipOtherCur = true;
//             }

//          if (skipCur)
//             {
//             if (next)
//                {
//                cur = next->getData()->asIntConstraint();
//                TR_ASSERT(cur, "Expecting int constraints in intIntersect");
//                next = next->getNextElement();
//                curLow = cur->getLow();
//                curHigh = cur->getHigh();
//                }
//             else
//                break;
//             }
//          if (skipOtherCur)
//             {
//             if (otherNext)
//                {
//                otherCur = otherNext->getData()->asIntConstraint();
//                TR_ASSERT(otherCur, "Expecting int constraints in intIntersect");
//                otherNext = otherNext->getNextElement();
//                otherLow = otherCur->getLow();
//                otherHigh = otherCur->getHigh();
//                }
//             else
//                break;
//             }
//          }

//       lastResultEntry = result.getListHead();
//       if (!lastResultEntry)
//          return NULL;

//       // If only one entry, collapse the merged list into the single entry
//       //
//       if (!lastResultEntry->getNextElement())
//          return lastResultEntry->getData();

//       return TR::VPMergedConstraints::create(vp, lastResultEntry);
//       }
//    }

TR::VPConstraint *TR::VPMergedConstraints::shortIntersect(TR::VPConstraint * other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp)
   {
   TR::VPShortConstraint *otherCur = other->asShortConstraint();

   TR_ScratchList<TR::VPConstraint>  result (vp->trMemory());
   ListElement<TR::VPConstraint>    *next = _constraints.getListHead();
   TR::VPShortConstraint            *cur  = next->getData()->asShortConstraint();
   ListElement<TR::VPConstraint>    *lastResultEntry = NULL;

   if (otherCur)
      {
      int16_t curLow = cur->getLow();
      int16_t curHigh = cur->getHigh();
      int16_t otherLow = otherCur->getLow();
      int16_t otherHigh = otherCur->getHigh();

      next = next->getNextElement();
      while (cur && otherCur)
         {
         bool skipCur      = false;
         bool skipOtherCur = false;

         // If the two current ranges do not overlap, skip the lower range and
         // try again
         //
         if (curHigh < otherLow)
            skipCur = true;
         else if (otherHigh < curLow)
            skipOtherCur = true;

         else
            {
            // Put the intersection of the two current ranges into the result list
            //
            int16_t resultLow = (curLow > otherLow) ? curLow : otherLow;
            int16_t resultHigh = (curHigh < otherHigh) ? curHigh : otherHigh;
            lastResultEntry = result.addAfter(TR::VPShortRange::create(vp, resultLow, resultHigh), lastResultEntry);

            // Reduce the two current ranges. If either is exhausted, move to the
            // next
            //
            if (resultHigh == TR::getMaxSigned<TR::Int16>())
               break;
            curLow = otherLow = resultHigh+1;
            if (curLow > curHigh)
               skipCur = true;
            if (otherLow > otherHigh)
               skipOtherCur = true;
            }

         if (skipCur)
            {
            if (next)
               {
               cur = next->getData()->asShortConstraint();
               TR_ASSERT(cur, "Expecting short constraints in shortIntersect");
               next = next->getNextElement();
               curLow = cur->getLow();
               curHigh = cur->getHigh();
               }
            else
               break;
            }
         if (skipOtherCur)
            {
            if (otherNext)
               {
               otherCur = otherNext->getData()->asShortConstraint();
               TR_ASSERT(otherCur, "Expecting short constraints in shortIntersect");
               otherNext = otherNext->getNextElement();
               otherLow = otherCur->getLow();
               otherHigh = otherCur->getHigh();
               }
            else
               break;
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry)
         return NULL;

      // If only one entry, collapse the merged list into the single entry
      //
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();

      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
    else
      {
        TR_ASSERT(false,"Intersecting with another type for short");
      }
    return NULL;
   }

TR::VPConstraint *TR::VPMergedConstraints::intIntersect(TR::VPConstraint *other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp)
   {

   TR::VPIntConstraint *otherCur = other->asIntConstraint();
   //if (otherCur && otherCur->isUnsigned())
   //   return intIntersect(otherCur, otherNext, vp, true);

   TR_ScratchList<TR::VPConstraint>         result(vp->trMemory());
   ListElement<TR::VPConstraint> *next = _constraints.getListHead();
   TR::VPIntConstraint           *cur  = next->getData()->asIntConstraint();
   ListElement<TR::VPConstraint> *lastResultEntry = NULL;


   //TR_ASSERT(cur && otherCur, "Expecting int constraints in intIntersect");

   if (otherCur)
      {
      int32_t curLow = cur->getLow();
      int32_t curHigh = cur->getHigh();
      int32_t otherLow = otherCur->getLow();
      int32_t otherHigh = otherCur->getHigh();

      next = next->getNextElement();
      while (cur && otherCur)
         {
         bool skipCur      = false;
         bool skipOtherCur = false;

         // If the two current ranges do not overlap, skip the lower range and
         // try again
         //
         if (curHigh < otherLow)
            skipCur = true;
         else if (otherHigh < curLow)
            skipOtherCur = true;

         else
            {
            // Put the intersection of the two current ranges into the result list
            //
            int32_t resultLow = (curLow > otherLow) ? curLow : otherLow;
            int32_t resultHigh = (curHigh < otherHigh) ? curHigh : otherHigh;
            lastResultEntry = result.addAfter(TR::VPIntRange::create(vp, resultLow, resultHigh), lastResultEntry);

            // Reduce the two current ranges. If either is exhausted, move to the
            // next
            //
            if (resultHigh == TR::getMaxSigned<TR::Int32>())
               break;
            curLow = otherLow = resultHigh+1;
            if (curLow > curHigh)
               skipCur = true;
            if (otherLow > otherHigh)
               skipOtherCur = true;
            }

         if (skipCur)
            {
            if (next)
               {
               cur = next->getData()->asIntConstraint();
               TR_ASSERT(cur, "Expecting int constraints in intIntersect");
               next = next->getNextElement();
               curLow = cur->getLow();
               curHigh = cur->getHigh();
               }
            else
               break;
            }
         if (skipOtherCur)
            {
            if (otherNext)
               {
               otherCur = otherNext->getData()->asIntConstraint();
               TR_ASSERT(otherCur, "Expecting int constraints in intIntersect");
               otherNext = otherNext->getNextElement();
               otherLow = otherCur->getLow();
               otherHigh = otherCur->getHigh();
               }
            else
               break;
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry)
         return NULL;

      // If only one entry, collapse the merged list into the single entry
      //
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();

      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }

   else
      {
      TR::VPLongConstraint *otherCur = other->asLongConstraint();

      next = next->getNextElement();
      while (cur && otherCur)
         {
         int32_t curLow = cur->getLow();
         int32_t curHigh = cur->getHigh();
         int64_t otherLow = otherCur->getLow();
         int64_t otherHigh = otherCur->getHigh();

         bool skipCur      = false;
         bool skipOtherCur = false;

         // If the two current ranges do not overlap, skip the lower range and
         // try again
         //
         if ((int64_t)curHigh < otherLow)
            skipCur = true;
         else if (otherHigh < (int64_t)curLow)
            skipOtherCur = true;

         else
            {
            // Put the intersection of the two current ranges into the result list
            //
            int32_t resultLow = ((int64_t)curLow > (int64_t)otherLow) ? (int32_t)curLow : (int32_t)otherLow;
            int32_t resultHigh = ((int64_t)curHigh < (int64_t)otherHigh) ? (int32_t)curHigh : (int32_t)otherHigh;
            lastResultEntry = result.addAfter(TR::VPIntRange::create(vp, resultLow, resultHigh), lastResultEntry);

            // Reduce the two current ranges. If either is exhausted, move to the
            // next
            //
            if (resultHigh == TR::getMaxSigned<TR::Int32>())
               break;
            otherLow = resultHigh+1;
            curLow = (int32_t)otherLow;
            if (curLow > curHigh)
               skipCur = true;
            if (otherLow > otherHigh)
               skipOtherCur = true;
            }

         if (skipCur)
            {
            if (next)
               {
               cur = next->getData()->asIntConstraint();
               TR_ASSERT(cur, "Expecting int constraints in intIntersect");
               next = next->getNextElement();
               curLow = cur->getLow();
               curHigh = cur->getHigh();
               }
            else
               break;
            }
         if (skipOtherCur)
            {
            if (otherNext)
               {
               otherCur = otherNext->getData()->asLongConstraint();
               //TR_ASSERT(otherCur, "Expecting int constraints in intIntersect");
               otherNext = otherNext->getNextElement();
               otherLow = otherCur->getLow();
               otherHigh = otherCur->getHigh();
               }
            else
               break;
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry)
         return NULL;

      // If only one entry, collapse the merged list into the single entry
      //
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();

      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
   }

TR::VPConstraint *TR::VPMergedConstraints::longIntersect(TR::VPConstraint *other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp)
   {
   TR_ScratchList<TR::VPConstraint>         result(vp->trMemory());
   ListElement<TR::VPConstraint> *next = _constraints.getListHead();
   TR::VPLongConstraint          *cur  = next->getData()->asLongConstraint();
   ListElement<TR::VPConstraint> *lastResultEntry = NULL;

   //TR_ASSERT(cur && otherCur, "Expecting long constraints in longIntersect");

   TR::VPLongConstraint *otherCur = other->asLongConstraint();

   if (otherCur)
      {
      int64_t curLow = cur->getLow();
      int64_t curHigh = cur->getHigh();
      int64_t otherLow = otherCur->getLow();
      int64_t otherHigh = otherCur->getHigh();

      next = next->getNextElement();
      while (cur && otherCur)
         {
         bool skipCur      = false;
         bool skipOtherCur = false;

         // If the two current ranges do not overlap, skip the lower range and
         // try again
         //
         if (curHigh < otherLow)
            skipCur = true;
         else if (otherHigh < curLow)
            skipOtherCur = true;

         else
            {
            // Put the intersection of the two current ranges into the result list
            //
            int64_t resultLow = (curLow > otherLow) ? curLow : otherLow;
            int64_t resultHigh = (curHigh < otherHigh) ? curHigh : otherHigh;
            lastResultEntry = result.addAfter(TR::VPLongRange::create(vp, resultLow, resultHigh), lastResultEntry);

            // Reduce the two current ranges. If either is exhausted, move to the
            // next
            //
            if (resultHigh == TR::getMaxSigned<TR::Int64>())
               break;
            curLow = otherLow = resultHigh+1;
            if (curLow > curHigh)
               skipCur = true;
            if (otherLow > otherHigh)
               skipOtherCur = true;
            }

         if (skipCur)
            {
            if (next)
               {
               cur = next->getData()->asLongConstraint();
               TR_ASSERT(cur, "Expecting long constraints in longIntersect");
               next = next->getNextElement();
               curLow = cur->getLow();
               curHigh = cur->getHigh();
               }
            else
               break;
            }
         if (skipOtherCur)
            {
            if (otherNext)
               {
               otherCur = otherNext->getData()->asLongConstraint();
               TR_ASSERT(otherCur, "Expecting long constraints in longIntersect");
               otherNext = otherNext->getNextElement();
               otherLow = otherCur->getLow();
               otherHigh = otherCur->getHigh();
               }
            else
               break;
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry)
         return NULL;

      // If only one entry, collapse the merged list into the single entry
      //
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();

      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }

   else
      {
      TR::VPIntConstraint *otherCur = other->asIntConstraint();
      if (otherCur == NULL)
         return NULL;
      int64_t curLow = cur->getLow();
      int64_t curHigh = cur->getHigh();
      int32_t otherLow = otherCur->getLow();
      int32_t otherHigh = otherCur->getHigh();

      next = next->getNextElement();
      while (cur && otherCur)
         {
         bool skipCur      = false;
         bool skipOtherCur = false;

         // If the two current ranges do not overlap, skip the lower range and
         // try again
         //
         if ((int64_t)curHigh < (int64_t)otherLow)
            skipCur = true;
         else if ((int64_t)otherHigh < (int64_t)curLow)
            skipOtherCur = true;

         else
            {
            // Put the intersection of the two current ranges into the result list
            //
            int64_t resultLow = ((int64_t)curLow > (int64_t)otherLow) ? curLow : otherLow;
            int64_t resultHigh = ((int64_t)curHigh < (int64_t)otherHigh) ? curHigh : otherHigh;
            lastResultEntry = result.addAfter(TR::VPLongRange::create(vp, resultLow, resultHigh), lastResultEntry);

            // Reduce the two current ranges. If either is exhausted, move to the
            // next
            //
            if (resultHigh == TR::getMaxSigned<TR::Int64>())
               break;
            curLow = resultHigh+1;
            otherLow = (int32_t)(resultHigh+1);
            if (curLow > curHigh)
               skipCur = true;
            if (otherLow > otherHigh)
               skipOtherCur = true;
            }

         if (skipCur)
            {
            if (next)
               {
               cur = next->getData()->asLongConstraint();
               TR_ASSERT(cur, "Expecting long constraints in longIntersect");
               next = next->getNextElement();
               curLow = cur->getLow();
               curHigh = cur->getHigh();
               }
            else
               break;
            }
         if (skipOtherCur)
            {
            if (otherNext)
               {
               otherCur = otherNext->getData()->asIntConstraint();
               //TR_ASSERT(otherCur, "Expecting long constraints in longIntersect");
               otherNext = otherNext->getNextElement();
               otherLow = otherCur->getLow();
               otherHigh = otherCur->getHigh();
               }
            else
               break;
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry)
         return NULL;

      // If only one entry, collapse the merged list into the single entry
      //
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();

      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
   }

   
TR::VPConstraint *TR::VPMergedConstraints::floatIntersect(TR::VPConstraint *other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp)
   {
   TR_ScratchList<TR::VPConstraint>         result(vp->trMemory());
   ListElement<TR::VPConstraint> *next = _constraints.getListHead();
   TR::VPFloatConstraint          *cur  = next->getData()->asFloatConstraint();
   ListElement<TR::VPConstraint> *lastResultEntry = NULL;

   TR::VPFloatConstraint *otherCur = other->asFloatConstraint();

   if (otherCur)
      {
      float curLow = cur->getLow();
      float curHigh = cur->getHigh();
      float otherLow = otherCur->getLow();
      float otherHigh = otherCur->getHigh();

      next = next->getNextElement();
      while (cur && otherCur)
         {
         bool skipCur      = false;
         bool skipOtherCur = false;

         // If the two current ranges do not overlap, skip the lower range and
         // try again
         //
         if (FPCompare(curHigh, otherLow) == -1)
            skipCur = true;
         else if (FPCompare(otherHigh, curLow) == -1)
            skipOtherCur = true;

         else
            {
            // Put the intersection of the two current ranges into the result list
            //
            float resultLow = (FPCompare(curLow, otherLow) == 1) ? curLow : otherLow;
            float resultHigh = (FPCompare(curHigh, otherHigh) == -1) ? curHigh : otherHigh;
            lastResultEntry = result.addAfter(TR::VPFloatRange::create(vp, resultLow, resultHigh), lastResultEntry);

            // Reduce the two current ranges. If either is exhausted, move to the
            // next
            //
            if (FPCompare(resultHigh, TR::getMaxFloat()) == 0)
               break;
            curLow = otherLow = resultHigh;
            if (FPCompare(curLow, curHigh) >= 0)
               skipCur = true;
            if (FPCompare(otherLow, otherHigh) >= 0)
               skipOtherCur = true;
            }

         if (skipCur)
            {
            if (next)
               {
               cur = next->getData()->asFloatConstraint();
               TR_ASSERT_FATAL(cur, "Expecting float constraints in floatIntersect");
               next = next->getNextElement();
               curLow = cur->getLow();
               curHigh = cur->getHigh();
               }
            else
               break;
            }
         if (skipOtherCur)
            {
            if (otherNext)
               {
               otherCur = otherNext->getData()->asFloatConstraint();
               TR_ASSERT_FATAL(otherCur, "Expecting float constraints in floatIntersect");
               otherNext = otherNext->getNextElement();
               otherLow = otherCur->getLow();
               otherHigh = otherCur->getHigh();
               }
            else
               break;
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry)
         return NULL;

      // If only one entry, collapse the merged list into the single entry
      //
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();

      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
   }


TR::VPConstraint *TR::VPMergedConstraints::doubleIntersect(TR::VPConstraint *other, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp)
   {
   TR_ScratchList<TR::VPConstraint>         result(vp->trMemory());
   ListElement<TR::VPConstraint> *next = _constraints.getListHead();
   TR::VPDoubleConstraint          *cur  = next->getData()->asDoubleConstraint();
   ListElement<TR::VPConstraint> *lastResultEntry = NULL;

   TR::VPDoubleConstraint *otherCur = other->asDoubleConstraint();

   if (otherCur)
      {
      double curLow = cur->getLow();
      double curHigh = cur->getHigh();
      double otherLow = otherCur->getLow();
      double otherHigh = otherCur->getHigh();

      next = next->getNextElement();
      while (cur && otherCur)
         {
         bool skipCur      = false;
         bool skipOtherCur = false;

         // If the two current ranges do not overlap, skip the lower range and
         // try again
         //
         if (FPCompare(curHigh, otherLow) == -1)
            skipCur = true;
         else if (FPCompare(otherHigh, curLow) == -1)
            skipOtherCur = true;

         else
            {
            // Put the intersection of the two current ranges into the result list
            //
            double resultLow = (FPCompare(curLow, otherLow) == 1) ? curLow : otherLow;
            double resultHigh = (FPCompare(curHigh, otherHigh) == -1) ? curHigh : otherHigh;
            lastResultEntry = result.addAfter(TR::VPDoubleRange::create(vp, resultLow, resultHigh), lastResultEntry);

            // Reduce the two current ranges. If either is exhausted, move to the
            // next
            //
            if (FPCompare(resultHigh, TR::getMaxDouble()) == 0)
               break;
            curLow = otherLow = resultHigh;
            if (FPCompare(curLow, curHigh) >= 0)
               skipCur = true;
            if (FPCompare(otherLow, otherHigh) >= 0)
               skipOtherCur = true;
            }

         if (skipCur)
            {
            if (next)
               {
               cur = next->getData()->asDoubleConstraint();
               TR_ASSERT_FATAL(cur, "Expecting double constraints in doubleIntersect");
               next = next->getNextElement();
               curLow = cur->getLow();
               curHigh = cur->getHigh();
               }
            else
               break;
            }
         if (skipOtherCur)
            {
            if (otherNext)
               {
               otherCur = otherNext->getData()->asDoubleConstraint();
               TR_ASSERT_FATAL(otherCur, "Expecting double constraints in doubleIntersect");
               otherNext = otherNext->getNextElement();
               otherLow = otherCur->getLow();
               otherHigh = otherCur->getHigh();
               }
            else
               break;
            }
         }

      lastResultEntry = result.getListHead();
      if (!lastResultEntry)
         return NULL;

      // If only one entry, collapse the merged list into the single entry
      //
      if (!lastResultEntry->getNextElement())
         return lastResultEntry->getData();

      return TR::VPMergedConstraints::create(vp, lastResultEntry);
      }
   }


TR::VPConstraint *TR::VPLessThanOrEqual::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPNotEqual *otherNE = other->asNotEqual();
   if (otherNE)
      {
      if (otherNE->increment() != increment())
         return this;
      TR::VPConstraint *rel = TR::VPLessThanOrEqual::create(vp, increment()-1);
      if (hasArtificialIncrement())
        rel->setHasArtificialIncrement();
      return rel;
      }
   TR::VPLessThanOrEqual *otherLE = other->asLessThanOrEqual();
   if (otherLE)
      {
      if (otherLE->increment() < increment())
         return other;
      return this;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPGreaterThanOrEqual::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPNotEqual *otherNE = other->asNotEqual();
   if (otherNE)
      {
      if (otherNE->increment() != increment())
         return this;
      TR::VPConstraint *rel = TR::VPGreaterThanOrEqual::create(vp, increment()+1);
      if (hasArtificialIncrement())
        rel->setHasArtificialIncrement();
      return rel;
      }
   TR::VPLessThanOrEqual *otherLE = other->asLessThanOrEqual();
   if (otherLE)
      {
      if (otherLE->increment() == increment())
         return TR::VPEqual::create(vp, increment());
      return this;
      }
   TR::VPGreaterThanOrEqual *otherGE = other->asGreaterThanOrEqual();
   if (otherGE)
      {
      if (otherGE->increment() > increment())
         return other;
      return this;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPNotEqual::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPNotEqual *otherNE = other->asNotEqual();
   if (otherNE)
      {
      if (otherNE->increment() == 0)
         return other;
      return this;
      }
   return NULL;
   }

TR::VPConstraint *TR::VPEqual::intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TRACER(vp, this, other);

   TR::VPNotEqual *otherNE = other->asNotEqual();
   if (otherNE)
      {
      if (otherNE->increment() != increment())
         return this;
      return NULL;
      }
   if (other->asLessThanOrEqual())
      return this;
   if (other->asGreaterThanOrEqual())
      return this;
   TR::VPEqual *otherEQ = other->asEqual();
   if (otherEQ)
      {
      if (otherEQ->increment() != increment())
         return NULL;
      return this;
      }
   return NULL;
   }

// ***************************************************************************
//
// Arithmetic operations on Value Propagation Constraints
//
// ***************************************************************************



TR::VPConstraint *TR::VPConstraint::add(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp)
   {
   return NULL;
   }

TR::VPConstraint *TR::VPConstraint::subtract(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp)
   {
   return NULL;
   }

TR::VPConstraint *TR::VPShortConstraint::add(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
    TR::VPShortConstraint *otherShort = other->asShortConstraint();
    if(!otherShort)
        return NULL;

    TR::DataType type(dt);

    if(!type.isInt16())
        return NULL;

   //Compute the lower and upper bound values, and determine whether or not the arithmetic
   //has overflowed in either case.
   bool lowOverflow;
   int16_t low  = addWithOverflow<int16_t>(getLow(), otherShort->getLow(), lowOverflow);

   bool highOverflow;
   int16_t high = addWithOverflow<int16_t>(getHigh(), otherShort->getHigh(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }

TR::VPConstraint *TR::VPIntConstraint::add(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
   // TODO - handle add and subtract for merged constraints
   //
   TR::VPIntConstraint *otherInt = other->asIntConstraint();
   if (!otherInt)
      return NULL;

   TR::DataType type(dt);

   if (!type.isInt32())
      // TODO - we don't handle overflow of 1-byte and 2-byte adds correctly
      return NULL;

   //if (type.isUnsignedInt())
   //   return add(otherInt, vp, true);

   //Compute the lower and upper bound values, and determine whether or not the arithmetic
   //has overflowed in either case.
   bool lowOverflow;
   int32_t low  = addWithOverflow<int32_t>(getLow(), otherInt->getLow(), lowOverflow);

   bool highOverflow;
   int32_t high = addWithOverflow<int32_t>(getHigh(), otherInt->getHigh(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }

TR::VPConstraint *TR::VPFloatConstraint::add(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
    TR::VPFloatConstraint *otherFloat = other->asFloatConstraint();
    if(!otherFloat)
        return NULL;

    TR::DataType type(dt);

    if(!type.isFloat())
        return NULL;

   /*Compute the lower and upper bound values, and determine whether or not the arithmetic
    *has overflowed in either case.
    */
   bool lowOverflow;
   float low  = fpAddWithOverflow(getLow(), otherFloat->getLow(), lowOverflow);

   bool highOverflow;
   float high = fpAddWithOverflow(getHigh(), otherFloat->getHigh(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }

TR::VPConstraint *TR::VPDoubleConstraint::add(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
    TR::VPDoubleConstraint *otherDouble = other->asDoubleConstraint();
    if(!otherDouble)
        return NULL;

    TR::DataType type(dt);

    if(!type.isDouble())
        return NULL;

   /*Compute the lower and upper bound values, and determine whether or not the arithmetic
    *has overflowed in either case.
    */
   bool lowOverflow;
   double low  = fpAddWithOverflow(getLow(), otherDouble->getLow(), lowOverflow);

   bool highOverflow;
   double high = fpAddWithOverflow(getHigh(), otherDouble->getHigh(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }

// // unsigned subtract operation
// TR::VPConstraint *TR::VPIntConstraint::subtract(TR::VPIntConstraint *otherInt, OMR::ValuePropagation *vp, bool Unsigned)
//    {
//    TR_ASSERT(isUnsigned() && otherInt->isUnsigned(), "Expecting unsigned constraints in subtract\n");
//    uint32_t low = (uint32_t)getLow() - (uint32_t)otherInt->getHigh();
//    uint32_t subLow = (uint32_t)getLow() - (uint32_t)otherInt->getLow();
//    uint32_t high = (uint32_t)getHigh() - (uint32_t)otherInt->getLow();
//    uint32_t subHigh = (uint32_t)getHigh() - (uint32_t)otherInt->getHigh();

//    TR::VPIntConstraint *wrapRange = NULL;
//    TR::VPConstraint *range = NULL;
//    //
//    // Check for unsigned integer arithmetic wrap
//    //
//    if ((uint32_t) otherInt->getLow() > 0 &&
//          ((high > (uint32_t)getHigh())||
//           (subHigh > (uint32_t)getHigh())))
//       {
//       if (subHigh < high)
//          high = subHigh;
//       if (subLow > low)
//          low = subLow;

//       wrapRange = TR::VPIntRange::create(vp, high, TR::getMaxUnsigned<TR::Int32>(), true);
//       range = TR::VPIntRange::create(vp, TR::getMinUnsigned<TR::Int32>(), low, true);
//       if (!range ||
//           !wrapRange)
//          return NULL;
//       range = TR::VPMergedConstraints::create(vp, range, wrapRange);
//       ///range->setIsUnsigned(true);
//       }
//    if ((uint32_t)otherInt->getHigh() > 0 &&
//        ((low > (uint32_t)getLow()) ||
//         (subLow > (uint32_t)getLow())))
//       {
//       if (range)
//          return NULL;

//       if (subHigh > high)
//          high = subHigh;
//       if (subLow < low)
//          low = subLow;

//       wrapRange = TR::VPIntRange::create(vp, low, TR::getMaxUnsigned<TR::Int32>(), true);
//       range = TR::VPIntRange::create(vp, TR::getMinUnsigned<TR::Int32>(), high, true);
//       if (!range ||
//           !wrapRange)
//          return NULL;
//       range = TR::VPMergedConstraints::create(vp, range, wrapRange);
//       ///range->setIsUnsigned(true);
//       }

//    if (!range)
//       range = TR::VPIntRange::create(vp, low, high, true);
//    return range;
//    }

TR::VPConstraint *TR::VPShortConstraint::subtract(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation * vp)
   {
   TR::VPShortConstraint *otherShort = other->asShortConstraint();

   if(!otherShort)
       return NULL;

   TR::DataType type(dt);

   if (!type.isInt16())
       return NULL;

   //Compute the lower and upper bound values, and determine whether or not the arithmetic
   //has overflowed in either case.
   bool lowOverflow;
   int16_t low  = subWithOverflow<int16_t>(getLow(), otherShort->getHigh(), lowOverflow);

   bool highOverflow;
   int16_t high = subWithOverflow<int16_t>(getHigh(), otherShort->getLow(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }

TR::VPConstraint *TR::VPIntConstraint::subtract(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
   TR::VPIntConstraint *otherInt = other->asIntConstraint();
   if (!otherInt)
      return NULL;

   TR::DataType type(dt);
   if (!type.isInt32())
      // TODO - we don't handle overflow of 1-byte and 2-byte subtracts correctly
      return NULL;

   //if (type.isUnsignedInt())
   //   return subtract(otherInt, vp, true);

   //Compute the lower and upper bound values, and determine whether or not the arithmetic
   //has overflowed in either case.
   bool lowOverflow;
   int32_t low  = subWithOverflow<int32_t>(getLow(), otherInt->getHigh(), lowOverflow);

   bool highOverflow;
   int32_t high = subWithOverflow<int32_t>(getHigh(), otherInt->getLow(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }


TR::VPConstraint *TR::VPFloatConstraint::subtract(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
   TR::VPFloatConstraint *otherFloat = other->asFloatConstraint();
   if (!otherFloat)
      return NULL;

   TR::DataType type(dt);
   if (!type.isFloat())
      return NULL;

   /*Compute the lower and upper bound values, and determine whether or not the arithmetic
    *has overflowed in either case.
    */
   bool lowOverflow;
   float low  = fpSubWithOverflow(getLow(), otherFloat->getHigh(), lowOverflow);

   bool highOverflow;
   float high = fpSubWithOverflow(getHigh(), otherFloat->getLow(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }


TR::VPConstraint *TR::VPDoubleConstraint::subtract(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
   TR::VPDoubleConstraint *otherDouble = other->asDoubleConstraint();
   if (!otherDouble)
      return NULL;

   TR::DataType type(dt);
   if (!type.isDouble())
      return NULL;

   /*Compute the lower and upper bound values, and determine whether or not the arithmetic
    *has overflowed in either case.
    */
   bool lowOverflow;
   double low  = fpSubWithOverflow(getLow(), otherDouble->getHigh(), lowOverflow);

   bool highOverflow;
   double high = fpSubWithOverflow(getHigh(), otherDouble->getLow(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }


TR::VPConstraint *TR::VPShortConstraint::getRange(int16_t low, int16_t high, bool lowCanOverflow, bool highCanOverflow, OMR::ValuePropagation * vp)
   {
   //This function returns a VP constraint range, with the appropriate setting of the canOverflow bit, for either an addition or subtraction
   //operation. The function takes the results of the lower- and upper-bound calculations for the operation, both performed in the same precision
   //arithmetic as that of the underlying operation, along with two flags indicated whether or not the operations producing these two bounds
   //overflowed. From these values, a constraint can be created, with the overflow flags being set as conservatively as possible.
   //
   //Also see VPIntConstraint::getRange, VPLongConstraint::getRange, VPFloatConstraint::getRange and VPDoubleConstraint::getRange

   if ( lowCanOverflow && highCanOverflow )
      {//Both sides overflow

      if ( ( high ^ low ) < 0 )
         // high and low have different signs, thus high has overflowed above range, low has overflowed below range, result is full range
         return NULL;

      //either both high and low have overflowed above range, or both have overflowed below range, result is guarunteed to overflow every time
      return TR::VPShortRange::create(vp, low, high, TR_yes);
      }

   else if ( lowCanOverflow || highCanOverflow )
      {
      //Only one sided has overflowed, generally a merged constraint

      if ( high >= low )
         //this would result in an overlapping merged constraint, or full range result
         return NULL;

      //disjoint merged constraint
      TR::VPConstraint* range1 = TR::VPShortRange::create(vp, TR::getMinSigned<TR::Int16>(), high, TR_yes);
      TR::VPConstraint* range2 = TR::VPShortRange::create(vp, low, TR::getMaxSigned<TR::Int16>(), TR_yes);
      return TR::VPMergedConstraints::create(vp, range1, range2);
      }

   //no overflow
   return TR::VPShortRange::create(vp, low, high, TR_no);
   }

TR::VPConstraint *TR::VPIntConstraint::getRange(int32_t low, int32_t high, bool lowCanOverflow, bool highCanOverflow, OMR::ValuePropagation *vp)
   {
   //This function returns a VP constraint range, with the appropriate setting of the canOverflow bit, for either an addition or subtraction
   //operation. The function takes the results of the lower- and upper-bound calculations for the operation, both performed in the same precision
   //arithmetic as that of the underlying operation, along with two flags indicated whether or not the operations producing these two bounds
   //overflowed. From these values, a constraint can be created, with the overflow flags being set as conservatively as possible.
   //
   //Also see VPShortConstraint::getRange, VPLongConstraint::getRange, VPFloatConstraint::getRange and VPDoubleConstraint::getRange

   if ( lowCanOverflow && highCanOverflow )
      {//Both sides overflow

      if ( ( high ^ low ) < 0 )
         // high and low have different signs, thus high has overflowed above range, low has overflowed below range, result is full range
         return NULL;

      //either both high and low have overflowed above range, or both have overflowed below range, result is guarunteed to overflow every time
      return TR::VPIntRange::create(vp, low, high, TR_yes);
      }

   else if ( lowCanOverflow || highCanOverflow )
      {
      //Only one sided has overflowed, generally a merged constraint

      if ( high >= low )
         //this would result in an overlapping merged constraint, or full range result
         return NULL;

      //disjoint merged constraint
      TR::VPConstraint* range1 = TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), high, TR_yes);
      TR::VPConstraint* range2 = TR::VPIntRange::create(vp, low, TR::getMaxSigned<TR::Int32>(), TR_yes);
      return TR::VPMergedConstraints::create(vp, range1, range2);
      }

   //no overflow
   return TR::VPIntRange::create(vp, low, high, TR_no);
   }


TR::VPConstraint *TR::VPFloatConstraint::getRange(float low, float high, bool lowCanOverflow, bool highCanOverflow, OMR::ValuePropagation *vp)
   {
   //This function returns a VP constraint range, with the appropriate setting of the canOverflow bit, for either an addition or subtraction
   //operation. The function takes the results of the lower- and upper-bound calculations for the operation, both performed in the same precision
   //arithmetic as that of the underlying operation, along with two flags indicated whether or not the operations producing these two bounds
   //overflowed. From these values, a constraint can be created, with the overflow flags being set as conservatively as possible.
   //
   //Also see VPShortConstraint::getRange, VPIntConstraint::getRange, VPLongConstraint::getRange and VPDoubleConstraint::getRange

   if ( lowCanOverflow || highCanOverflow )
      /*If either side overflows, we return a NULL constraint for floating point.
      *TO-DO implement correct handler for floating point overflow after arithmatic operation
      */
      return NULL;

   //no overflow
   return TR::VPFloatRange::create(vp, low, high, TR_no);
   }


TR::VPConstraint *TR::VPDoubleConstraint::getRange(double low, double high, bool lowCanOverflow, bool highCanOverflow, OMR::ValuePropagation *vp)
   {
   //This function returns a VP constraint range, with the appropriate setting of the canOverflow bit, for either an addition or subtraction
   //operation. The function takes the results of the lower- and upper-bound calculations for the operation, both performed in the same precision
   //arithmetic as that of the underlying operation, along with two flags indicated whether or not the operations producing these two bounds
   //overflowed. From these values, a constraint can be created, with the overflow flags being set as conservatively as possible.
   //
   //Also see VPShortConstraint::getRange, VPIntConstraint::getRange, VPLongConstraint::getRange and VPDoubleConstraint::getRange

   if ( lowCanOverflow || highCanOverflow )
      /*If either side overflows, we return a NULL constraint for floating point.
      *TO-DO implement correct handler for floating point overflow after arithmatic operation
      */
      return NULL;

   //no overflow
   return TR::VPDoubleRange::create(vp, low, high, TR_no);
   }


TR::VPConstraint *TR::VPLongConstraint::add(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
   // TODO - handle add and subtract for merged constraints
   //
   TR::VPLongConstraint *otherLong = other->asLongConstraint();
   if (!otherLong)
      return NULL;

   TR::DataType type(dt);
   if (!type.isInt64())
      return NULL;

   //Compute the lower and upper bound values, and determine whether or not the arithmetic
   //has overflowed in either case.
   bool lowOverflow;
   int64_t low  = addWithOverflow<int64_t>(getLow(), otherLong->getLow(), lowOverflow);

   bool highOverflow;
   int64_t high = addWithOverflow<int64_t>(getHigh(), otherLong->getHigh(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }

TR::VPConstraint *TR::VPLongConstraint::subtract(TR::VPConstraint *other, TR::DataType dt, OMR::ValuePropagation *vp)
   {
   TR::VPLongConstraint *otherLong = other->asLongConstraint();
   if (!otherLong)
      return NULL;

   TR::DataType type(dt);
   if (!type.isInt64())
      return NULL;

   //Compute the lower and upper bound values, and determine whether or not the arithmetic
   //has overflowed in either case.
   bool lowOverflow;
   int64_t low  = subWithOverflow<int64_t>(getLow(), otherLong->getHigh(), lowOverflow);

   bool highOverflow;
   int64_t high = subWithOverflow<int64_t>(getHigh(), otherLong->getLow(), highOverflow);

   return getRange(low, high, lowOverflow, highOverflow, vp);
   }

TR::VPConstraint *TR::VPLongConstraint::getRange(int64_t low, int64_t high, bool lowCanOverflow, bool highCanOverflow, OMR::ValuePropagation *vp)
   {
   //This function returns a VP constraint range, with the appropriate setting of the canOverflow bit, for either an addition or subtraction
   //operation. The function takes the results of the lower- and upper-bound calculations for the operation, both performed in the same precision
   //arithmetic as that of the underlying operation, along with two flags indicated whether or not the operations producing these two bounds
   //overflowed. From these values, a constraint can be created, with the overflow flags being set as conservatively as possible.
   //
   //Also see VPShortConstraint::getRange and VPIntConstraint::getRange

   if ( lowCanOverflow && highCanOverflow )
      {//Both sides overflow

      if ( ( high ^ low ) < 0 )
         // high and low have different signs, thus high has overflowed above range, low has overflowed below range, result is full range
         return NULL;

      //either both high and low have overflowed above range, or both have overflowed below range, result is guarunteed to overflow every time
      return TR::VPLongRange::create(vp, low, high, TR_yes);
      }

   else if ( lowCanOverflow || highCanOverflow )
      {
      //Only one sided has overflowed, generally a merged constraint

      if ( high >= low )
         //this would result in an overlapping merged constraint, or full range result
         return NULL;

      //disjoint merged constraint
      TR::VPConstraint* range1 = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), high, TR_yes);
      TR::VPConstraint* range2 = TR::VPLongRange::create(vp, low, TR::getMaxSigned<TR::Int64>(), TR_yes);
      return TR::VPMergedConstraints::create(vp, range1, range2);
      }

   //no overflow
   return TR::VPLongRange::create(vp, low, high, TR_no);
   }

// ***************************************************************************
//
// COmparison operations on Value Propagation Constraints
//
// ***************************************************************************

bool TR::VPConstraint::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return false;
   }

bool TR::VPShortConst::mustBeEqual(TR::VPConstraint * other, OMR::ValuePropagation *vp)
   {
   TR::VPShortConst * otherConst = other->asShortConst();
   if (isUnsigned() && otherConst && otherConst->isUnsigned())
       return ((uint16_t)otherConst->getShort() == (uint16_t)getShort());
   return otherConst && otherConst->getShort() == getShort();
   }

bool TR::VPIntConst::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPIntConst *otherConst = other->asIntConst();
   if (isUnsigned() && otherConst && otherConst->isUnsigned())
      return ((uint32_t)otherConst->getInt() == (uint32_t)getInt());
   return otherConst && otherConst->getInt() == getInt();
   }

bool TR::VPLongConst::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPLongConst *otherConst = other->asLongConst();
   return otherConst && otherConst->getLong() == getLong();
   }

bool TR::VPFloatConst::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPFloatConst *otherConst = other->asFloatConst();
   return otherConst && otherConst->getFloat() == getFloat();
   }

bool TR::VPDoubleConst::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPDoubleConst *otherConst = other->asDoubleConst();
   return otherConst && otherConst->getDouble() == getDouble();
   }

bool TR::VPClass::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (isNullObject() && other->isNullObject())
      return true;

   if (getKnownObject() && other->getKnownObject() && isNonNullObject() && other->isNonNullObject())
      return getKnownObject()->getIndex() == other->getKnownObject()->getIndex();

   TR::VPClass *otherClass = NULL;
   if (other)
      otherClass = other->asClass();
   if (!_preexistence &&
       !_arrayInfo && _type &&
       _type->isFixedClass() && isNonNullObject() &&
       other && otherClass &&
       !otherClass->getArrayInfo() &&
       !otherClass->isPreexistentObject() &&
       otherClass->getClassType() && otherClass->getClassType()->isFixedClass() && otherClass->isNonNullObject() &&
       (isClassObject() == TR_yes) &&
       (other->isClassObject() == TR_yes))
      {
      if (_type->asFixedClass()->getClass() == otherClass->getClassType()->asFixedClass()->getClass())
         return true;
      }

   return false;
   }

bool TR::VPNullObject::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return other->isNullObject();
   }

bool TR::VPConstString::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (other->getKnownObject())
      return other->mustBeEqual(this, vp);
   else
      return false;
   }

bool TR::VPKnownObject::mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   // An expression with a Known Object constraint could be null.  Since there
   // are two possible values, we can't generally prove we mustBeEqual to
   // another expression unless we have isNonNullObject info too, and that is
   // handled by TR::VPClass, so there's little we can do here.
   //
   return Super::mustBeEqual(other, vp);
   }

bool TR::VPKnownObject::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   // An expression with a Known Object constraint could be null.  To have any
   // hope of proving we're notEqual to some other expression, its constraint
   // must preclude null.
   //
   if (other->isNonNullObject())
      {
      if (other->getKnownObject() && getIndex() != other->getKnownObject()->getIndex())
         return true;
      }

   return Super::mustBeNotEqual(other, vp);
   }

bool TR::VPConstString::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (other->getKnownObject())
      return other->getKnownObject()->mustBeNotEqual(this, vp);
   else
      return false;
   }

bool TR::VPConstraint::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (isNullObject())
      {
      if (other->isNonNullObject())
         return true;
      }
   else if (isNonNullObject())
      {
      if (other->isNullObject())
         return true;
      }
   return false;
   }
bool TR::VPShortConstraint::mustBeNotEqual(TR::VPConstraint * other, OMR::ValuePropagation *vp)
   {
   TR::VPShortConstraint *otherShort = other->asShortConstraint();
   if(otherShort)
      {
      if (isUnsigned() && otherShort->isUnsigned())
          return (((uint16_t)getHigh() < (uint16_t)otherShort->getLow()) ||
                  ((uint16_t)getLow()  > (uint16_t)otherShort->getHigh()));
      else
          return getHigh() < otherShort->getLow() || getLow() > otherShort->getHigh();
      }

   TR::VPMergedConstraints *otherList = other->asMergedShortConstraints();
   if (otherList)
      {
      ListIterator <TR::VPConstraint> iter(otherList->getList());
      for (TR::VPConstraint * c = iter.getFirst(); c; c=iter.getNext())
          {
          if(!mustBeNotEqual(c, vp))
             return false;
          }
      return true;
      }
   return false;
   }

bool TR::VPIntConstraint::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPIntConstraint *otherInt = other->asIntConstraint();
   if (otherInt)
      {
      if (isUnsigned() && otherInt->isUnsigned())
         return (((uint32_t)getHigh() < (uint32_t)otherInt->getLow()) ||
                  ((uint32_t)getLow() > (uint32_t)otherInt->getHigh()));
      else
         return getHigh() < otherInt->getLow() || getLow() > otherInt->getHigh();
      }
   TR::VPMergedConstraints *otherList = other->asMergedIntConstraints();
   if (otherList)
      {
      // Must be not equal to each item in the list to be not equal to the list
      //
      ListIterator<TR::VPConstraint> iter(otherList->getList());
      for (TR::VPConstraint *c = iter.getFirst(); c; c = iter.getNext())
         {
         if (!mustBeNotEqual(c, vp))
            return false;
         }
      return true;
      }
   return false;
   }

bool TR::VPLongConstraint::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPLongConstraint *otherLong = other->asLongConstraint();
   if (otherLong)
      return getHigh() < otherLong->getLow() || getLow() > otherLong->getHigh();
   TR::VPMergedConstraints *otherList = other->asMergedLongConstraints();
   if (otherList)
      {
      // Must be not equal to each item in the list to be not equal to the list
      //
      ListIterator<TR::VPConstraint> iter(otherList->getList());
      for (TR::VPConstraint *c = iter.getFirst(); c; c = iter.getNext())
         {
         if (!mustBeNotEqual(c, vp))
            return false;
         }
      return true;
      }
   return false;
   }


/* Check if `this` and `other` are mutaully exclusive */
bool TR::VPFloatConstraint::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPFloatConstraint *otherFloat = other->asFloatConstraint();
   if (otherFloat)
      return FPCompare(getHigh(), otherFloat->getLow()) == -1 || 
             FPCompare(getLow(), otherFloat->getHigh()) == 1;
   TR::VPMergedConstraints *otherList = other->asMergedFloatConstraints();
   if (otherList)
      {
      /* Must be not equal to each item in the list to be not equal to the list */
      ListIterator<TR::VPConstraint> iter(otherList->getList());
      for (TR::VPConstraint *c = iter.getFirst(); c; c = iter.getNext())
         {
         if (!mustBeNotEqual(c, vp))
            return false;
         }
      return true;
      }
   return false;
   }


/* Check if `this` and `other` are mutaully exclusive */
bool TR::VPDoubleConstraint::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   TR::VPDoubleConstraint *otherDouble = other->asDoubleConstraint();
   if (otherDouble)
      return FPCompare(getHigh(), otherDouble->getLow()) == -1 || 
             FPCompare(getLow(), otherDouble->getHigh()) == 1;
   TR::VPMergedConstraints *otherList = other->asMergedDoubleConstraints();
   if (otherList)
      {
      /* Must be not equal to each item in the list to be not equal to the list */
      ListIterator<TR::VPConstraint> iter(otherList->getList());
      for (TR::VPConstraint *c = iter.getFirst(); c; c = iter.getNext())
         {
         if (!mustBeNotEqual(c, vp))
            return false;
         }
      return true;
      }
   return false;
   }


bool TR::VPMergedConstraints::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (!other->asMergedConstraints())
      return other->mustBeNotEqual(this, vp);

   // Every item in this list must be not equal to the other list
   //
   ListIterator<TR::VPConstraint> iter(getList());
   for (TR::VPConstraint *c = iter.getFirst(); c; c = iter.getNext())
      {
      if (!c->mustBeNotEqual(other, vp))
         return false;
      }
   return true;
   }


//FIXME: this is too conservative, can do more for non-fixed objects
bool TR::VPClass::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (isNullObject() && other->isNonNullObject())
      return true;
   if (isNonNullObject() && other->isNullObject())
      return true;

   if (getKnownObject() && other->getKnownObject() && isNonNullObject() && other->isNonNullObject())
      return getKnownObject()->getIndex() != other->getKnownObject()->getIndex();

   TR::VPClass *otherClass = NULL;
   if (other)
      otherClass = other->asClass();
  if (!_preexistence &&
       !_arrayInfo && _type &&
       _type->isFixedClass() && isNonNullObject() &&
       other && otherClass &&
       !otherClass->getArrayInfo() &&
       !otherClass->isPreexistentObject() &&
       otherClass->getClassType() && otherClass->getClassType()->isFixedClass() && otherClass->isNonNullObject() &&
       (isClassObject() == TR_yes) &&
       (other->isClassObject() == TR_yes))
      {
      if (_type->asFixedClass()->getClass() != otherClass->getClassType()->asFixedClass()->getClass())
         return true;
      }

   return false;
   }

bool TR::VPNullObject::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return other->isNonNullObject();
   }

bool TR::VPNonNullObject::mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return other->isNullObject();
   }

bool TR::VPConstraint::mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return false;
   }

bool TR::VPShortConstraint::mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (isUnsigned() && other->isUnsigned())
       return ((uint16_t)getHigh() < (uint16_t)other->getLowShort());
   return getHigh() < other->getLowShort();
   }

bool TR::VPIntConstraint::mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (isUnsigned() && other->isUnsigned())
      return ((uint32_t)getHigh() < (uint32_t)other->getLowInt());
   return getHigh() < other->getLowInt();
   }

bool TR::VPLongConstraint::mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return getHigh() < other->getLowLong();
   }

bool TR::VPFloatConstraint::mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return FPCompare(getHigh(), other->getLowFloat()) == -1;
   }

bool TR::VPDoubleConstraint::mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return FPCompare(getHigh(), other->getLowDouble()) == -1;
   }

bool TR::VPMergedConstraints::mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (_type.isInt16())
      {
      if (_constraints.getLastElement()->getData()->isUnsigned())
          return ((uint16_t)getHighShort() < (uint16_t)other->getLowShort());
      return getHighShort() < other->getLowShort();
      }

   if (_type.isInt64())
      return getHighLong() < other->getLowLong();

   if (_constraints.getLastElement()->getData()->isUnsigned())
      return ((uint32_t)getHighInt() < (uint32_t)other->getLowInt());
   return getHighInt() < other->getLowInt();
   }

bool TR::VPConstraint::mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return false;
   }

bool TR::VPShortConstraint::mustBeLessThanOrEqual(TR::VPConstraint * other, OMR::ValuePropagation *vp)
   {
   if (isUnsigned() && other->isUnsigned())
        return ((uint16_t)getHigh() <= (uint16_t)other->getLowShort());
   return getHigh() <= other->getLowShort();
   }

bool TR::VPIntConstraint::mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (isUnsigned() && other->isUnsigned())
      return ((uint32_t)getHigh() <= (uint32_t)other->getLowInt());
   return getHigh() <= other->getLowInt();
   }

bool TR::VPLongConstraint::mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   return getHigh() <= other->getLowLong();
   }
   
bool TR::VPFloatConstraint::mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   int32_t result = FPCompare(getHigh(),other->getLowFloat());
   return result <= 0;
   }

bool TR::VPDoubleConstraint::mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   int32_t result = FPCompare(getHigh(),other->getLowDouble());
   return result <= 0;
   }

bool TR::VPMergedConstraints::mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   if (_type.isInt64())
      return getHighLong() <= other->getLowLong();
   if (_constraints.getLastElement()->getData()->isUnsigned())
      return ((uint32_t)getHighInt() <= (uint32_t)other->getLowInt());
   return getHighInt() <= other->getLowInt();
   }

bool TR::VPConstraint::mustBeEqual()
   {
   return false;
   }

bool TR::VPEqual::mustBeEqual()
   {
   return increment() == 0;
   }

bool TR::VPConstraint::mustBeNotEqual()
   {
   return false;
   }

bool TR::VPLessThanOrEqual::mustBeNotEqual()
   {
   return increment() < 0;
   }

bool TR::VPGreaterThanOrEqual::mustBeNotEqual()
   {
   return increment() > 0;
   }

bool TR::VPEqual::mustBeNotEqual()
   {
   return increment() != 0;
   }

bool TR::VPNotEqual::mustBeNotEqual()
   {
   return increment() == 0;
   }

bool TR::VPConstraint::mustBeLessThan()
   {
   return false;
   }

bool TR::VPLessThanOrEqual::mustBeLessThan()
   {
   if ((increment() == 0) || hasArtificialIncrement())
      return (increment() < 0);
   return false;
   }

bool TR::VPEqual::mustBeLessThan()
   {
   return false; //increment() < 0;
   }

bool TR::VPConstraint::mustBeLessThanOrEqual()
   {
   return false;
   }

bool TR::VPLessThanOrEqual::mustBeLessThanOrEqual()
   {
   if ((increment() == 0) || hasArtificialIncrement())
      return (increment() <= 0);
   return false;
   }

bool TR::VPEqual::mustBeLessThanOrEqual()
   {
     return false; //increment() <= 0;
   }

bool TR::VPConstraint::mustBeGreaterThan()
   {
   return false;
   }

bool TR::VPGreaterThanOrEqual::mustBeGreaterThan()
   {
   if ((increment() == 0) || hasArtificialIncrement())
      return increment() > 0;
   return false;
   }

bool TR::VPEqual::mustBeGreaterThan()
   {
     return false; //increment() > 0;
   }

bool TR::VPConstraint::mustBeGreaterThanOrEqual()
   {
   return false;
   }

bool TR::VPGreaterThanOrEqual::mustBeGreaterThanOrEqual()
   {
   if ((increment() == 0) || hasArtificialIncrement())
      return (increment() >= 0);
   return false;
   }

bool TR::VPEqual::mustBeGreaterThanOrEqual()
   {
   return false; //increment() >= 0;
   }

// ***************************************************************************
//
// Propagation of absolute constraints to relative constraints and
// propagation of relative constraints.
//
// ***************************************************************************

TR::VPConstraint *TR::VPRelation::propagateAbsoluteConstraint(TR::VPConstraint *constraint, int32_t relative, OMR::ValuePropagation *vp)
   {
   // Default propagation is to do nothing
   //
   return NULL;
   }

TR::VPConstraint *TR::VPLessThanOrEqual::propagateAbsoluteConstraint(TR::VPConstraint *constraint, int32_t relative, OMR::ValuePropagation *vp)
   {
   // x <= y + I and x == (M to N)    ==> y == ((M-I) to TR::getMaxSigned<TR::Int32>())
   //
   if (vp->trace())
      {
      traceMsg(vp->comp(), "      Propagating V <= value %d %+d and V is ", relative, increment());
      constraint->print(vp->comp(), vp->comp()->getOutFile());
      }

   if (constraint->asLongConstraint())
      {
      int64_t oldBound = constraint->getLowLong();
      int64_t newBound = oldBound - increment();
      if (increment() < 0)
         return NULL;

      if (newBound > oldBound)
         return NULL;

      constraint = TR::VPLongRange::create(vp, newBound, TR::getMaxSigned<TR::Int64>()-increment());
      }
   else
      {
      int32_t oldBound = constraint->getLowInt();
      int32_t newBound = oldBound - increment();
      if (increment() < 0)
         return NULL;

      if (newBound > oldBound)
         return NULL;

      constraint = TR::VPIntRange::create(vp, newBound, TR::getMaxSigned<TR::Int32>()-increment());
      }
   if (vp->trace())
      {
      if (constraint)
         {
         traceMsg(vp->comp(), " ... value %d is ", relative);
         constraint->print(vp->comp(), vp->comp()->getOutFile());
         }
      traceMsg(vp->comp(), "\n");
      }

   return constraint;
   }

TR::VPConstraint *TR::VPGreaterThanOrEqual::propagateAbsoluteConstraint(TR::VPConstraint *constraint, int32_t relative, OMR::ValuePropagation *vp)
   {
   // x >= y + I and x == (M to N)    ==> y == (TR::getMinSigned<TR::Int32>() to (M-I))
   //
   if (vp->trace())
      {
      traceMsg(vp->comp(), "      Propagating V >= value %d %+d and V is ", relative, increment());
      constraint->print(vp->comp(), vp->comp()->getOutFile());
      }


   if (constraint->asLongConstraint())
      {
      int64_t oldBound = constraint->getHighLong();
      int64_t newBound = oldBound - increment();
      if (increment() > 0)
         return NULL;

      if (newBound < oldBound)
         return NULL;

      constraint = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>() - increment(), newBound);
      }
   else
      {
      int32_t oldBound = constraint->getHighInt();
      int32_t newBound = oldBound - increment();
      if (increment() > 0)
         return NULL;

      if (newBound < oldBound)
         return NULL;

      constraint = TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>() - increment(), newBound);
      }
   if (vp->trace())
      {
      if (constraint)
         {
         traceMsg(vp->comp(), " ... value %d is ", relative);
         constraint->print(vp->comp(), vp->comp()->getOutFile());
         }
      traceMsg(vp->comp(), "\n");
      }

   return constraint;
   }

TR::VPConstraint *TR::VPEqual::propagateAbsoluteConstraint(TR::VPConstraint *constraint, int32_t relative, OMR::ValuePropagation *vp)
   {
   // x == y + I and x == (M to N)    ==> y == ((M-I) to N-I))
   //
   if (vp->trace())
      {
      traceMsg(vp->comp(), "      Propagating V == value %d %+d and V is ", relative, increment());
      constraint->print(vp->comp(), vp->comp()->getOutFile());
      }

   if (increment() != 0)
      {
      if (constraint->asLongConstraint())
         constraint = constraint->asLongConstraint()->subtract(TR::VPLongConst::create(vp, increment()), TR::Int64, vp);
      else if (constraint->asIntConstraint())
         {
         constraint = constraint->asIntConstraint()->subtract(TR::VPIntConst::create(vp, increment()), TR::Int32, vp);
         }
      else
         constraint = NULL;
      }
   if (vp->trace())
      {
      if (constraint)
         {
         traceMsg(vp->comp(), " ... value %d is ", relative);
         constraint->print(vp->comp(), vp->comp()->getOutFile());
         }
      traceMsg(vp->comp(), "\n");
      }

   return constraint;
   }

TR::VPConstraint *TR::VPNotEqual::propagateAbsoluteConstraint(TR::VPConstraint *constraint, int32_t relative, OMR::ValuePropagation *vp)
   {
   // x != y + I and x == N           ==> y != (N-I)
   //
   if (vp->trace())
      {
      traceMsg(vp->comp(), "      Propagating V != value %d %+d and V is ", relative, increment());
      constraint->print(vp->comp(), vp->comp()->getOutFile());
      }

   TR::VPConstraint *newConstraint = NULL;
   if (constraint->asLongConst())
      {
      int64_t excludedValue = constraint->getLowLong() - increment();
      if (excludedValue != TR::getMinSigned<TR::Int64>())
         newConstraint = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), excludedValue-1);
      if (excludedValue != TR::getMaxSigned<TR::Int64>())
         {
         if (newConstraint)
            newConstraint = newConstraint->merge(TR::VPLongRange::create(vp, excludedValue+1, TR::getMaxSigned<TR::Int64>()), vp);
         else
            newConstraint = TR::VPLongRange::create(vp, excludedValue+1, TR::getMaxSigned<TR::Int64>());
         }
      }
   else if (constraint->asIntConst())
      {
      int32_t excludedValue = constraint->getLowInt() - increment();
      // if (constraint->isUnsigned())
//          {
//          bool isNegative = (excludedValue < TR::getMinUnsigned<TR::Int32>());
//          if (!isNegative && excludedValue != TR::getMinUnsigned<TR::Int32>())
//             {
//             newConstraint = TR::VPIntRange::create(vp, TR::getMinUnsigned<TR::Int32>(), excludedValue-1, true);
//             }
//          if (!isNegative && ((uint32_t)excludedValue != TR::getMaxUnsigned<TR::Int32>()))
//             {
//             if (newConstraint)
//                {
//                TR::VPConstraint *exConstraint = TR::VPIntRange::create(vp, excludedValue+1,
//                                                                      TR::getMaxUnsigned<TR::Int32>(), true);
//                newConstraint = newConstraint->merge(exConstraint, vp);
//                }
//             else
//                {
//                newConstraint = TR::VPIntRange::create(vp, excludedValue+1, TR::getMaxUnsigned<TR::Int32>(), true);
//                }
//             }
//          }
//       else
//         {
         if (excludedValue != TR::getMinSigned<TR::Int32>())
            newConstraint = TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), excludedValue-1);
         if (excludedValue != TR::getMaxSigned<TR::Int32>())
            {
            if (newConstraint)
               newConstraint = newConstraint->merge(TR::VPIntRange::create(vp, excludedValue+1, TR::getMaxSigned<TR::Int32>()), vp);
            else
               newConstraint = TR::VPIntRange::create(vp, excludedValue+1, TR::getMaxSigned<TR::Int32>());
            }
//         }
      }

   if (vp->trace())
      {
      if (newConstraint)
         {
         traceMsg(vp->comp(), " ... value %d is ", relative);
         newConstraint->print(vp->comp(), vp->comp()->getOutFile());
         }
      traceMsg(vp->comp(), "\n");
      }

   return newConstraint;
   }

TR::VPConstraint *TR::VPLessThanOrEqual::propagateRelativeConstraint(TR::VPRelation *other, int32_t relative, int32_t otherRelative, OMR::ValuePropagation *vp)
   {
   // x <= y + M and x >= z + N    ==> y >= z + (N-M)
   //
   if (!other->asGreaterThanOrEqual() && !other->asEqual())
      return NULL;

   TR::VPConstraint *constraint;
   int32_t newIncr = other->increment() - increment();

   // newIncr cannot be TR::getMinSigned<TR::Int32>(), because when we try to negate TR::getMinSigned<TR::Int32>(), we get TR::getMinSigned<TR::Int32>() again
   if (newIncr == TR::getMinSigned<TR::Int32>())
      return NULL;

   if (increment() >= 0)
      {
      if (newIncr > other->increment())
         return NULL;
      }
   else
      {
      if (newIncr < other->increment())
         return NULL;
      }
   constraint = TR::VPGreaterThanOrEqual::create(vp, newIncr);
   if (newIncr == other->increment())
      {
      if (other->hasArtificialIncrement())
        constraint->setHasArtificialIncrement();
      }
   else if (newIncr == -1*increment())
      {
      if (hasArtificialIncrement())
        constraint->setHasArtificialIncrement();
      }

   if (vp->trace())
      {
      traceMsg(vp->comp(), "      Propagating V <= value %d %+d and V >= value %d %+d", relative, increment(), otherRelative, other->increment());
      traceMsg(vp->comp(), " ... value %d >= value %d %+d\n", relative, otherRelative, newIncr);
      }
   return constraint;
   }

TR::VPConstraint *TR::VPGreaterThanOrEqual::propagateRelativeConstraint(TR::VPRelation *other, int32_t relative, int32_t otherRelative, OMR::ValuePropagation *vp)
   {
   // x >= y + M and x <= z + N    ==> y <= z + (N-M)
   //
   if (!other->asLessThanOrEqual() && !other->asEqual())
      return NULL;

   TR::VPConstraint *constraint;
   int32_t newIncr = other->increment() - increment();

   // newIncr cannot be TR::getMinSigned<TR::Int32>(), because when we try to negate TR::getMinSigned<TR::Int32>(), we get TR::getMinSigned<TR::Int32>() again
   if (newIncr == TR::getMinSigned<TR::Int32>())
      return NULL;

   if (increment() >= 0)
      {
      if (newIncr > other->increment())
         return NULL;
      }
   else
      {
      if (newIncr < other->increment())
         return NULL;
      }
   constraint = TR::VPLessThanOrEqual::create(vp, newIncr);
   if (newIncr == other->increment())
      {
      if (other->hasArtificialIncrement())
        constraint->setHasArtificialIncrement();
      }
   else if (newIncr == -1*increment())
      {
      if (hasArtificialIncrement())
        constraint->setHasArtificialIncrement();
      }

   if (vp->trace())
      {
      traceMsg(vp->comp(), "      Propagating V >= value %d %+d and V <= value %d %+d", relative, increment(), otherRelative, other->increment());
      traceMsg(vp->comp(), " ... value %d <= value %d %+d\n", relative, otherRelative, newIncr);
      }
   return constraint;
   }

TR::VPConstraint *TR::VPEqual::propagateRelativeConstraint(TR::VPRelation *other, int32_t relative, int32_t otherRelative, OMR::ValuePropagation *vp)
   {
   // x == y + M and x <= z + N    ==> y <= z + (N-M)
   // x == y + M and x >= z + N    ==> y >= z + (N-M)
   // x == y + M and x != z + N    ==> y != z + (N-M)
   // x == y + M and x == z + N    ==> y == z + (N-M)
   //
   TR::VPConstraint *constraint;
   int32_t newIncr = other->increment() - increment();

   // newIncr cannot be TR::getMinSigned<TR::Int32>(), because when we try to negate TR::getMinSigned<TR::Int32>(), we get TR::getMinSigned<TR::Int32>() again
   if (newIncr == TR::getMinSigned<TR::Int32>())
      return NULL;

   if (increment() >= 0)
      {
      if (newIncr > other->increment())
         return NULL;
      }
   else
      {
      if (newIncr < other->increment())
         return NULL;
      }
   if (other->asLessThanOrEqual())
      {
      constraint = TR::VPLessThanOrEqual::create(vp, newIncr);
      if (newIncr == other->increment())
         {
         if (other->hasArtificialIncrement())
            constraint->setHasArtificialIncrement();
         }
      else if (newIncr == -1*increment())
         {
         if (hasArtificialIncrement())
            constraint->setHasArtificialIncrement();
         }

      if (vp->trace())
         {
         traceMsg(vp->comp(), "      Propagating V == value %d %+d and V <= value %d %+d", relative, increment(), otherRelative, other->increment());
         traceMsg(vp->comp(), " ... value %d <= value %d %+d\n", relative, otherRelative, newIncr);
         }
      }
   else if (other->asGreaterThanOrEqual())
      {
      constraint = TR::VPGreaterThanOrEqual::create(vp, newIncr);
      if (newIncr == other->increment())
         {
         if (other->hasArtificialIncrement())
            constraint->setHasArtificialIncrement();
         }

      if (vp->trace())
         {
         traceMsg(vp->comp(), "      Propagating V == value %d %+d and V >= value %d %+d", relative, increment(), otherRelative, other->increment());
         traceMsg(vp->comp(), " ... value %d >= value %d %+d\n", relative, otherRelative, newIncr);
         }
      }
   else if (other->asNotEqual())
      {
      constraint = TR::VPNotEqual::create(vp, newIncr);
      if (vp->trace())
         {
         traceMsg(vp->comp(), "      Propagating V == value %d %+d and V != value %d %+d", relative, increment(), otherRelative, other->increment());
         traceMsg(vp->comp(), " ... value %d != value %d %+d\n", relative, otherRelative, newIncr);
         }
      }
   else
      {
      TR_ASSERT(other->asEqual(), "assertion failure");
      constraint = TR::VPEqual::create(vp, newIncr);
      if (vp->trace())
         {
         traceMsg(vp->comp(), "      Propagating V == value %d %+d and V == value %d %+d", relative, increment(), otherRelative, other->increment());
         traceMsg(vp->comp(), " ... value %d == value %d %+d\n", relative, otherRelative, newIncr);
         }
      }
   return constraint;
   }

TR::VPConstraint *TR::VPNotEqual::propagateRelativeConstraint(TR::VPRelation *other, int32_t relative, int32_t otherRelative, OMR::ValuePropagation *vp)
   {
   // x != y + M and x == z + N    ==> y != z + (N-M)
   //
   if (!other->asEqual())
      return NULL;

   TR::VPConstraint *constraint;
   int32_t newIncr = other->increment() - increment();

   // newIncr cannot be TR::getMinSigned<TR::Int32>(), because when we try to negate TR::getMinSigned<TR::Int32>(), we get TR::getMinSigned<TR::Int32>() again
   if (newIncr == TR::getMinSigned<TR::Int32>())
      return NULL;

   if (increment() >= 0)
      {
      if (newIncr > other->increment())
         return NULL;
      }
   else
      {
      if (newIncr < other->increment())
         return NULL;
      }
   constraint = TR::VPNotEqual::create(vp, newIncr);
   if (vp->trace())
      {
      traceMsg(vp->comp(), "      Propagating V != value %d %+d and V == value %d %+d", relative, increment(), otherRelative, other->increment());
      traceMsg(vp->comp(), " ... value %d != value %d %+d\n", relative, otherRelative, newIncr);
      }
   return constraint;
   }



// ***************************************************************************
//
// Print methods for Value Propagation Constraints
//
// ***************************************************************************


void TR::VPConstraint::print(OMR::ValuePropagation *vp)
   {
   print(vp->comp(), vp->comp()->getOutFile());
   }

void TR::VPConstraint::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "unknown absolute constraint");
   }

void TR::VPConstraint::print(TR::Compilation *comp, TR::FILE *outFile, int32_t relative)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "unknown constraint relative to value number %d", relative);
   }

void TR::VPShortConst::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
       return;
   if (isUnsigned())
      trfprintf(outFile, "%u US ", getLow());
   else
      trfprintf(outFile, "%d S ", getLow());

   }


void TR::VPShortRange::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
       return;

   if (isUnsigned())
      {
       if ((uint16_t)getLow() == TR::getMinUnsigned<TR::Int16>())
           trfprintf(outFile, "(TR::getMinUnsigned<TR::Int16>() ");
       else
           trfprintf(outFile, "(%u ", getLow());

       if ((uint16_t)getHigh() == TR::getMaxUnsigned<TR::Int16>())
           trfprintf(outFile, "to TR::getMaxUnsigned<TR::Int16>())US");
       else
           trfprintf(outFile, "to %u)US",getHigh());
      }
   else
      {
      if (getLow() == TR::getMinSigned<TR::Int16>())
         trfprintf(outFile, "(TR::getMinSigned<TR::Int16>() ");
      else
         trfprintf(outFile, "(%d ", getLow());
      if (getHigh() == TR::getMaxSigned<TR::Int16>())
         trfprintf(outFile, "to TR::getMaxSigned<TR::Int16>())S");
      else
         trfprintf(outFile, "to %d)S", getHigh());
      }
   }

void TR::VPIntConst::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   if (isUnsigned())
      trfprintf(outFile, "%u UI ", getLow());
   else
      trfprintf(outFile, "%d I ", getLow());
   }

void TR::VPIntRange::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   if (isUnsigned())
      {
      if ((uint32_t)getLow() == TR::getMinUnsigned<TR::Int32>())
         trfprintf(outFile, "(TR::getMinUnsigned<TR::Int32>() ");
      else
         trfprintf(outFile, "(%u ", getLow());
      if ((uint32_t)getHigh() == TR::getMaxUnsigned<TR::Int32>())
         trfprintf(outFile, "to TR::getMaxUnsigned<TR::Int32>())UI");
      else
         trfprintf(outFile, "to %u)UI", getHigh());
      }
   else
      {
      if (getLow() == TR::getMinSigned<TR::Int32>())
         trfprintf(outFile, "(TR::getMinSigned<TR::Int32>() ");
      else
         trfprintf(outFile, "(%d ", getLow());
      if (getHigh() == TR::getMaxSigned<TR::Int32>())
         trfprintf(outFile, "to TR::getMaxSigned<TR::Int32>())I");
      else
         trfprintf(outFile, "to %d)I", getHigh());
      }
   }

void TR::VPLongConst::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   if (isUnsigned())
      trfprintf(outFile, UINT64_PRINTF_FORMAT " UL ", getUnsignedLong());
   else
      trfprintf(outFile, INT64_PRINTF_FORMAT " L ", getLong());
   }

void TR::VPLongRange::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   if (isUnsigned())
      {
      if (getLow() == 0ULL)
         trfprintf(outFile, "(MIN_ULONG ");
      else
         trfprintf(outFile, "(" UINT64_PRINTF_FORMAT " ", (uint64_t)getLow());
      if (getHigh() == (uint64_t)(-1))
         trfprintf(outFile, "to MAX_ULONG)UL");
      else
         trfprintf(outFile, "to " UINT64_PRINTF_FORMAT ")UL", (uint64_t)getHigh());
      }
   else
      {
      if (getLow() == TR::getMinSigned<TR::Int64>())
         trfprintf(outFile, "(TR::getMinSigned<TR::Int64>() ");
      else
         trfprintf(outFile, "(" INT64_PRINTF_FORMAT " ", getLow());
      if (getHigh() == TR::getMaxSigned<TR::Int64>())
         trfprintf(outFile, "to TR::getMaxSigned<TR::Int64>())L");
      else
         trfprintf(outFile, "to " INT64_PRINTF_FORMAT ")L", getHigh());
      }

   }


void TR::VPFloatConst::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
       return;
   trfprintf(outFile, "%f F ", getLow());
   }


void TR::VPDoubleConst::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
       return;
   trfprintf(outFile, "%d D ", getLow());
   }

void TR::VPFloatRange::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   if (FPCompare(getLow(), TR::getMinFloat()) == 0)
         trfprintf(outFile, "(TR::getMinFloat() ");
   else
      trfprintf(outFile, "(%f ", getLow());
   if (FPCompare(getHigh(), TR::getMaxFloat()) == 0)
      trfprintf(outFile, "to TR::getMaxFloat())F");
   else
      trfprintf(outFile, "to %f)F", getHigh());
   }

void TR::VPDoubleRange::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   if (FPCompare(getLow(), TR::getMinDouble()) == 0)
         trfprintf(outFile, "(TR::getMinDouble() ");
   else
      trfprintf(outFile, "(%d ", getLow());
   if (FPCompare(getHigh(), TR::getMaxDouble()) == 0)
      trfprintf(outFile, "to TR::getMaxDouble())D");
   else
      trfprintf(outFile, "to %d)D", getHigh());
   }

void TR::VPClass::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   if (_type)
      _type->print(comp, outFile);
   if (getKnownObject() && !isNonNullObject())
      trfprintf(outFile, " (maybe NULL)");
   if (_presence)
      _presence->print(comp, outFile);
   if (_arrayInfo)
      _arrayInfo->print(comp, outFile);
   if (_location)
      _location->print(comp, outFile);
   }

void TR::VPResolvedClass::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   int32_t len = _len;
   const char *sig = _sig;
   if (isSpecialClass((uintptr_t)_class))
      {
      sig = "<special>";
      len = strlen(sig);
      }

   trfprintf(outFile, "class %.*s", len, sig);
   }

void TR::VPFixedClass::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "fixed ");
   TR::VPResolvedClass::print(comp, outFile);
   }

static uint16_t format[] = {'C','S',':','"','%','.','*','s','"',0};
void TR::VPConstString::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   trfprintf(outFile, "constant string: \"");
#ifdef J9_PROJECT_SPECIFIC
      {
      TR::VMAccessCriticalSection vpConstStringPrintCriticalSection(comp,
                                                                     TR::VMAccessCriticalSection::tryToAcquireVMAccess);
      if (vpConstStringPrintCriticalSection.hasVMAccess())
         {
         uintptr_t stringStaticAddr = (uintptr_t)_symRef->getSymbol()->castToStaticSymbol()->getStaticAddress();
         uintptr_t string = comp->fej9()->getStaticReferenceFieldAtAddress(stringStaticAddr);
         int32_t len = comp->fej9()->getStringLength(string);
         for (int32_t i = 0; i < len; ++i)
            trfprintf(outFile, "%c", TR::Compiler->cls.getStringCharacter(comp, string, i));
         trfprintf(outFile, "\" ");
         }
      else
         trfprintf(outFile, " <could not print as no fe access> \" ");
      }
#endif
   }

void TR::VPKnownObject::print(TR::Compilation * comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   trfprintf(outFile, "known object obj%d ", _index);
   TR::VPFixedClass::print(comp, outFile);
   }

void TR::VPUnresolvedClass::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   int32_t methodLen  = _method->nameLength();
   char   *methodName = _method->nameChars();
   trfprintf(outFile, "unresolved class %.*s in method %.*s", _len, _sig, methodLen, methodName);
   }

void TR::VPNullObject::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, " (NULL)");
   }

void TR::VPNonNullObject::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, " (non-NULL)");
   }

void TR::VPPreexistentObject::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, " (pre-existent)");
   }

void TR::VPArrayInfo::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   if (_lowBound > 0 || _highBound < TR::getMaxSigned<TR::Int32>())
      trfprintf(outFile, " (min bound %d, max bound %d)", _lowBound, _highBound);
   if (_elementSize > 0)
      trfprintf(outFile, " (array element size %d)", _elementSize);
   }

void TR::VPObjectLocation::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   const int nkinds = 4;
   static VPObjectLocationKind kinds[nkinds] = {  HeapObject ,  StackObject ,  JavaLangClassObject ,  J9ClassObject  };
   static const char * const      names[nkinds] = { "HeapObject", "StackObject", "JavaLangClassObject", "J9ClassObject" };
   trfprintf(outFile, " {");
   bool first = true;
   for (int i = 0; i < nkinds; i++)
      {
      if ((_kind & kinds[i]) != 0)
         {
         trfprintf(outFile, "%s%s", first ? "" : ",", names[i]);
         first = false;
         }
      }
   trfprintf(outFile, "}");
   }

void TR::VPMergedConstraints::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "{");
   ListElement<TR::VPConstraint> *p;
   for (p = _constraints.getListHead(); p; p = p->getNextElement())
      {
      p->getData()->print(comp, outFile);
      if (p->getNextElement())
         trfprintf(outFile, ", ");
      }
   trfprintf(outFile, "}");
   }

void TR::VPUnreachablePath::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "*** Unreachable Path ***");
   }

void TR::VPSync::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "sync has %s been emitted", (syncEmitted() == TR_yes) ? "" : "not");
   }

void TR::VPLessThanOrEqual::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "less than or equal to another value number");
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

void TR::VPLessThanOrEqual::print(TR::Compilation *comp, TR::FILE *outFile, int32_t relative)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "less than or equal to value number %d", relative);
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

void TR::VPGreaterThanOrEqual::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "greater than or equal to another value number");
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

void TR::VPGreaterThanOrEqual::print(TR::Compilation *comp, TR::FILE *outFile, int32_t relative)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "greater than or equal to value number %d", relative);
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

void TR::VPEqual::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "equal to another value number");
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

void TR::VPEqual::print(TR::Compilation *comp, TR::FILE *outFile, int32_t relative)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "equal to value number %d", relative);
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

void TR::VPNotEqual::print(TR::Compilation *comp, TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "not equal to another value number");
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

void TR::VPNotEqual::print(TR::Compilation *comp, TR::FILE *outFile, int32_t relative)
   {
   if (outFile == NULL)
      return;
   trfprintf(outFile, "not equal to value number %d", relative);
   if (increment() > 0)
      trfprintf(outFile," + %d", increment());
   else if (increment() < 0)
      trfprintf(outFile," - %d", -increment());
   }

// ***************************************************************************
//
// Name methods for Value Propagation Constraints
//
// ***************************************************************************

const char *TR::VPShortConst::name()                 { return "ShortConst";           }
const char *TR::VPShortRange::name()                 { return "ShortRange";           }
const char *TR::VPIntConst::name()                   { return "IntConst";             }
const char *TR::VPIntRange::name()                   { return "IntRange";             }
const char *TR::VPLongConst::name()                  { return "LongConst";            }
const char *TR::VPLongRange::name()                  { return "LongRange";            }
const char *TR::VPFloatConst::name()                 { return "FloatConst";           }
const char *TR::VPFloatRange::name()                 { return "FloatRange";           }
const char *TR::VPDoubleConst::name()                { return "DoubleConst";          }
const char *TR::VPDoubleRange::name()                { return "DoubleRange";          }
const char *TR::VPClass::name()                      { return "Class";                }
const char *TR::VPResolvedClass::name()              { return "ResolvedClass";        }
const char *TR::VPFixedClass::name()                 { return "FixedClass";           }
const char *TR::VPConstString::name()                { return "ConstString";          }
const char *TR::VPUnresolvedClass::name()            { return "UnresolvedClass";      }
const char *TR::VPNullObject::name()                 { return "NullObject";           }
const char *TR::VPNonNullObject::name()              { return "NonNullObject";        }
const char *TR::VPPreexistentObject::name()          { return "PreexistentObject";    }
const char *TR::VPArrayInfo::name()                  { return "ArrayInfo";            }
const char *TR::VPObjectLocation::name()             { return "ObjectLocation";       }
const char *TR::VPKnownObject::name()                { return "KnownObject";          }
const char *TR::VPMergedConstraints::name()          { return "MergedConstraints";    }
const char *TR::VPUnreachablePath::name()            { return "UnreachablePath";      }
const char *TR::VPSync::name()                       { return "Sync";                 }
const char *TR::VPLessThanOrEqual::name()            { return "LessThanOrEqual";      }
const char *TR::VPGreaterThanOrEqual::name()         { return "GreaterThanOrEqual";   }
const char *TR::VPEqual::name()                      { return "Equal";                }
const char *TR::VPNotEqual::name()                   { return "NotEqual";             }

// ***************************************************************************
//
// Tracer
//
// ***************************************************************************

TR::VPConstraint::Tracer::Tracer(OMR::ValuePropagation *vpArg, TR::VPConstraint *self, TR::VPConstraint *other, const char *name)
   :_vp(vpArg), _self(self), _other(other), _name(name)
   {
   if (comp()->getOption(TR_TraceVPConstraints))
      {
      traceMsg(comp(), "{{{ %s.%s\n", _self->name(), _name);
      traceMsg(comp(), "  self: ");
      _self->print(vp());
      traceMsg(comp(), "\n  other: ");
      _other->print(vp());
      traceMsg(comp(), "\n");
      }
   }

TR::VPConstraint::Tracer::~Tracer()
   {
   if (comp()->getOption(TR_TraceVPConstraints))
      {
      traceMsg(comp(), "%s.%s }}}\n", _self->name(), _name);
      }
   }

TR::Compilation *TR::VPConstraint::Tracer::comp()
   {
   return vp()->comp();
   }
