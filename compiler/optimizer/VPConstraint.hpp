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

#ifndef VPCONSTRAINT_INCL
#define VPCONSTRAINT_INCL

#include <stdint.h>
#include <string.h>
#include "env/KnownObjectTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include <cmath>

class TR_FrontEnd;
class TR_OpaqueClassBlock;
class TR_ResolvedMethod;
namespace TR { class VPArrayInfo; }
namespace TR { class VPClassPresence; }
namespace TR { class VPClassType; }
namespace TR { class VPConstString; }
namespace TR { class VPKnownObject; }
namespace TR { class VPObjectLocation; }
namespace TR { class VPPreexistentObject; }
namespace OMR { class ValuePropagation; }
namespace TR { class VP_BCDValue; }
namespace TR { class VP_BCDSign; }

namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class SymbolReference; }

#define VP_HASH_TABLE_SIZE 251

#define VP_UNDEFINED_PRECISION -1

namespace TR {

// ***************************************************************************
//
// Value propagation constraints. A constraint is a property that puts some
// limit on a value.
//
// ***************************************************************************

class VPConstraint
   {
   public:
   TR_ALLOC(TR_Memory::ValuePropagation)

   VPConstraint(int32_t p) : _mergePriority(p) {_unsignedType = false;}

   virtual class VPShortConstraint   *asShortConstraint();
   virtual class VPShortConst        *asShortConst();
   virtual class VPShortRange        *asShortRange();
   virtual class VPIntConstraint     *asIntConstraint();
   virtual class VPIntConst          *asIntConst();
   virtual class VPIntRange          *asIntRange();
   virtual class VPLongConstraint    *asLongConstraint();
   virtual class VPLongConst         *asLongConst();
   virtual class VPLongRange         *asLongRange();
   virtual class VPFloatConstraint   *asFloatConstraint();
   virtual class VPFloatConst        *asFloatConst();
   virtual class VPFloatRange        *asFloatRange();
   virtual class VPDoubleConstraint  *asDoubleConstraint();
   virtual class VPDoubleConst       *asDoubleConst();
   virtual class VPDoubleRange       *asDoubleRange();
   virtual class VP_BCDValue         *asBCDValue();
   virtual class VP_BCDSign          *asBCDSign();
   virtual class VPClass             *asClass();
   virtual class VPClassType         *asClassType();
   virtual class VPResolvedClass     *asResolvedClass();
   virtual class VPFixedClass        *asFixedClass();
   virtual class VPKnownObject       *asKnownObject();
   virtual class VPConstString       *asConstString();
   virtual class VPUnresolvedClass   *asUnresolvedClass();
     //virtual class TR::VPImplementedInterface *asImplementedInterface();
   virtual class VPClassPresence     *asClassPresence();
   virtual class VPNullObject        *asNullObject();
   virtual class VPNonNullObject     *asNonNullObject();
   virtual class VPPreexistentObject *asPreexistentObject();
   virtual class VPArrayInfo         *asArrayInfo();
   virtual class VPObjectLocation    *asObjectLocation();
   virtual class VPMergedConstraints *asMergedConstraints();
   virtual class VPMergedConstraints *asMergedShortConstraints();
   virtual class VPMergedConstraints *asMergedIntConstraints();
   virtual class VPMergedConstraints *asMergedLongConstraints();
   virtual class VPMergedConstraints *asMergedFloatConstraints();
   virtual class VPMergedConstraints *asMergedDoubleConstraints();
   virtual class VPUnreachablePath   *asUnreachablePath();
   virtual class VPSync              *asVPSync();
   virtual class VPRelation          *asRelation();
   virtual class VPLessThanOrEqual   *asLessThanOrEqual();
   virtual class VPGreaterThanOrEqual*asGreaterThanOrEqual();
   virtual class VPEqual             *asEqual();
   virtual class VPNotEqual          *asNotEqual();

   virtual const char *name()=0;

   static TR::VPConstraint *create(OMR::ValuePropagation *vp, const char *sig, int32_t len, TR_ResolvedMethod *method, bool isFixedClass);
   static bool isSpecialClass(uintptr_t klass);


   TR::VPConstraint *merge(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   TR::VPConstraint *intersect(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   // Arithmetic operations
   //
   virtual TR::VPConstraint *add(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *subtract(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);
   virtual TR_YesNoMaybe canOverflow();
   virtual void setCanOverflow(TR_YesNoMaybe v) {}

   // Absolute comparisons
   //
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   // Relative comparisons
   //
   virtual bool mustBeEqual();
   virtual bool mustBeNotEqual();
   virtual bool mustBeLessThan();
   virtual bool mustBeLessThanOrEqual();
   virtual bool mustBeGreaterThan();
   virtual bool mustBeGreaterThanOrEqual();

   // Integer information
   //
   virtual int16_t getLowShort();
   virtual int16_t getHighShort();
   virtual int32_t getLowInt();
   virtual int32_t getHighInt();
   virtual int64_t getLowLong();
   virtual int64_t getHighLong();
   virtual float getLowFloat();
   virtual float getHighFloat();
   virtual double getLowDouble();
   virtual double getHighDouble();


   virtual uint16_t getUnsignedLowShort();
   virtual uint16_t getUnsignedHighShort();
   virtual uint32_t getUnsignedLowInt();
   virtual uint32_t getUnsignedHighInt();
   virtual uint64_t getUnsignedLowLong();
   virtual uint64_t getUnsignedHighLong();



   // Class information
   //
   virtual bool isNullObject();
   virtual bool isNonNullObject();
   virtual bool isPreexistentObject();
   virtual TR_OpaqueClassBlock *getClass();
   virtual bool isFixedClass();
   virtual bool isConstString();
   virtual TR::VPClassType *getClassType();
   virtual TR::VPClassPresence *getClassPresence();
   virtual TR::VPPreexistentObject *getPreexistence();
   virtual TR::VPObjectLocation *getObjectLocation();
   virtual TR::VPConstString *getConstString();
   virtual TR::VPKnownObject *getKnownObject();
   virtual TR::VPArrayInfo *getArrayInfo();
   virtual const char *getClassSignature(int32_t &len);

   virtual TR_YesNoMaybe isStackObject();
   virtual TR_YesNoMaybe isHeapObject();
   virtual TR_YesNoMaybe isClassObject();
   virtual TR_YesNoMaybe isJavaLangClassObject();
   virtual TR_YesNoMaybe isJ9ClassObject();

   // Merge priorities, used in merge and intersect methods. The method called
   // is the one belonging to the highest priority
   //
     /*
   enum MergePriorities
      {
      EqualPriority             = 16,
      GreaterThanOrEqualPriority= 15,
      LessThanOrEqualPriority   = 14,
      NotEqualPriority          = 13,
      MergedConstraintPriority  = 12,
      IntPriority               = 11,
      LongPriority              = 10,
      ClassPriority             = 9,
      ConstStringPriority       = 8,
      FixedClassPriority        = 7,
      ResolvedClassPriority     = 6,
      UnresolvedClassPriority   = 5,
      ClassPresencePriority     = 4,
      ClassPreexistencePriority = 3,
      ArrayInfoPriority         = 2,
      ObjectLocationPriority    = 1,
      ImplementsInterfacePriority = 0,
      highestPriority
      };
     */

   enum MergePriorities
      {
      EqualPriority             = 20,
      GreaterThanOrEqualPriority= 19,
      LessThanOrEqualPriority   = 18,
      NotEqualPriority          = 17,
      MergedConstraintPriority  = 16,
      FPConstraintPriority      = 15,
      ShortPriority             = 14,
      IntPriority               = 13,
      LongPriority              = 12,
      BCDPriority               = 11,
      ClassPriority             = 10,
      KnownObjectPriority       = 9,
      ConstStringPriority       = 8,
      FixedClassPriority        = 7,
      ResolvedClassPriority     = 6,
      UnresolvedClassPriority   = 5,
      ClassPresencePriority     = 4,
      ClassPreexistencePriority = 3,
      ArrayInfoPriority         = 2,
      ObjectLocationPriority    = 1,
      SyncPriority              = 0,
      highestPriority
      };

   int32_t priority() {return (_mergePriority & 0x7fffffff);}

   void setHasArtificialIncrement() { (_mergePriority = _mergePriority | 0x80000000); }
   bool hasArtificialIncrement() { return ((_mergePriority & 0x80000000) != 0); }

   void print(OMR::ValuePropagation *vp);
   virtual void print(TR::Compilation *, TR::FILE *);
   virtual void print(TR::Compilation *, TR::FILE *, int32_t relative);

   bool isUnsigned() { return _unsignedType; }
   void setIsUnsigned(bool b) { TR_ASSERT(0 == 1, "I should eventually remove this property from here"); _unsignedType = b; } //FIXME//CRITICAL

   virtual int32_t getPrecision() {return TR_MAX_DECIMAL_PRECISION;}

   protected:
   bool _unsignedType;

   class Tracer {
      public:
      Tracer(OMR::ValuePropagation *vp, TR::VPConstraint *self, TR::VPConstraint *other, const char *name);
      ~Tracer();

      OMR::ValuePropagation *vp(){ return _vp; }
      TR::Compilation      *comp();

      protected:
      OMR::ValuePropagation *_vp;
      TR::VPConstraint     *_self;
      TR::VPConstraint     *_other;
      const char          *_name;
   };

   // The following {add,sub}WithOverflow require that the build (C++) compiler
   // use two's complement (virtually guaranteed), and that type_t be a signed
   // integer type.

   template<typename type_t> type_t addWithOverflow(type_t a, type_t b, bool& overflow)
      {
      // Cast to uintmax_t to avoid undefined behaviour on signed overflow.
      //
      // The uintmax_t-typed operands are sign-extended from a and b. They
      // won't undergo integer promotions, because they're already as large as
      // possible. And they're already at a common type, so they won't undergo
      // the "usual arithmetic conversions" either. Therefore this will do
      // unsigned (modular) arithmitic. Converting back to type_t produces the
      // expected two's complement result.
      type_t sum = uintmax_t(a) + uintmax_t(b);

      //The overflow flag is set when the arithmetic used to compute the sum overflows. This happens exactly
      //when both operand have the same sign and the resulting value has the opposite sign to this.
      //A non-negative result from xoring two numbers indicates that their sign bits are either both
      //set, or both unset, meaning they have the same sign, while a negative result from xoring two
      //numbers means that exactly one of them has its sign bit set, meaning the signs of the two numbers
      //differ.
      overflow = ( a ^ b ) >= 0 && ( a ^ sum ) < 0;

      return sum;
      }

   template<typename type_t> type_t subWithOverflow(type_t a, type_t b, bool& overflow)
      {
      // About uintmax_t, see addWithOverflow above.
      type_t diff = uintmax_t(a) - uintmax_t(b);

      //The overflow flag is set when the arithmetic used to compute the difference overflows. This happens exactly
      //when both operand bounds have a differing sign and the resulting value has the same sign as the first operand.
      //A non-negative result from xoring two numbers indicates that their sign bits are either both
      //set, or both unset, meaning they have the same sign, while a negative result from xoring two
      //numbers means that exactly one of them has its sign bit set, meaning the signs of the two numbers
      //differ.
      overflow = ( a ^ b ) < 0 && ( a ^ diff ) < 0;

      return diff;
      }
      
   float fpAddWithOverflow(float a, float b, bool& overflow);
   float fpSubWithOverflow(float a, float b, bool& overflow);
   double fpAddWithOverflow(double a, double b, bool& overflow);
   double fpSubWithOverflow(double a, double b, bool& overflow);

   private:
   int32_t _mergePriority;
   };

#define TRACER(vp, self, other) Tracer _tracer_(vp, self, other, __func__)

class VPShortConstraint: public TR::VPConstraint
   {
   public:
   VPShortConstraint(int16_t v) : TR::VPConstraint(ShortPriority) {_low=v; _overflow = TR_no;}
   virtual TR::VPShortConstraint * asShortConstraint();

   int16_t getShort() {return _low; }
   int16_t getLow() {return _low; }
   virtual int16_t  getHigh() = 0;
   virtual int16_t  getLowShort();
   virtual int16_t  getHighShort();

   virtual TR::VPConstraint * merge1 ( TR::VPConstraint * other, OMR::ValuePropagation * vp);
   virtual TR::VPConstraint * intersect1 (TR::VPConstraint * other, OMR::ValuePropagation * vp);

   virtual bool mustBeNotEqual (TR::VPConstraint * other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThan (TR::VPConstraint * other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThanOrEqual (TR::VPConstraint * other, OMR::ValuePropagation *vp);

   virtual TR::VPConstraint *add(TR::VPConstraint * other, TR::DataType type, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *subtract(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation * vp);

   virtual TR_YesNoMaybe canOverflow() {return _overflow;}
   virtual void setCanOverflow(TR_YesNoMaybe v) {_overflow = v;}

   virtual int32_t getPrecision() {return getPrecisionFromValue(TR::getMaxSigned<TR::Int16>());}

   protected:
   int16_t _low;
   TR_YesNoMaybe _overflow;

   private:
   TR::VPConstraint *getRange(int16_t, int16_t, bool, bool, OMR::ValuePropagation * vp);
   };

class VPShortConst : public TR::VPShortConstraint
   {
   public:
   VPShortConst(int16_t v) : TR::VPShortConstraint(v) {}
   static TR::VPShortConst *create(OMR::ValuePropagation *vp, int16_t v);
   static TR::VPConstraint *createExclusion(OMR::ValuePropagation *vp, int16_t v);
   // unsigned createExclusion
   //static TR::VPConstraint *createExclusion(OMR::ValuePropagation *vp, int32_t v);
   virtual TR::VPShortConst *asShortConst();
   virtual int16_t getHigh() {return _low;}
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   virtual int32_t getPrecision() {return getPrecisionFromValue(_low);}
   };

class VPShortRange : public TR::VPShortConstraint
   {
   public:
   VPShortRange(int16_t low, int16_t high) : TR::VPShortConstraint(low), _high(high) {}
   static TR::VPShortConstraint *create(OMR::ValuePropagation *vp, int16_t low, int16_t high, TR_YesNoMaybe canOverflow = TR_no);
   static TR::VPShortConstraint *create(OMR::ValuePropagation *vp);
   static TR::VPShortConstraint *createWithPrecision(OMR::ValuePropagation *vp, int32_t precision, bool isNonNegative = false);
   virtual TR::VPShortRange *asShortRange();
   virtual int16_t getHigh() {return _high;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   virtual int32_t getPrecision() {return getPrecisionFromRange(_low, _high);}

   private:
   int16_t _high;
   };

class VPIntConstraint : public TR::VPConstraint
   {
   public:
   VPIntConstraint(int32_t v) : TR::VPConstraint(IntPriority) {_low = v; _overflow = TR_no;}
   virtual TR::VPIntConstraint *asIntConstraint();

   int32_t getInt() {return _low;}
   int32_t getLow() {return _low;}
   virtual int32_t getHigh() = 0;
   virtual int32_t getLowInt();
   virtual int32_t getHighInt();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   // unsigned intMerge
   //TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp, bool isUnsigned);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   // unsigned intIntersect
   //TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp, bool isUnsigned);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual TR::VPConstraint *add(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *subtract(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);
   // unsigned add
   //TR::VPConstraint *add(TR::VPIntConstraint *other, OMR::ValuePropagation *vp, bool isUnsigned);
   // unsigned subtract
   //TR::VPConstraint *subtract(TR::VPIntConstraint *other, OMR::ValuePropagation *vp, bool isUnsigned);

   virtual TR_YesNoMaybe canOverflow() {return _overflow;}
   virtual void setCanOverflow(TR_YesNoMaybe v) {_overflow = v;}

   virtual int32_t getPrecision() {return getPrecisionFromValue(TR::getMaxSigned<TR::Int32>());}

   protected:
   int32_t _low;
   TR_YesNoMaybe _overflow;

   private:
   TR::VPConstraint *getRange(int32_t, int32_t, bool, bool, OMR::ValuePropagation *vp );
   };

class VPIntConst : public TR::VPIntConstraint
   {
   public:
   VPIntConst(int32_t v) : TR::VPIntConstraint(v) {}
   static TR::VPIntConst *create(OMR::ValuePropagation *vp, int32_t v);
   static TR::VPConstraint *createExclusion(OMR::ValuePropagation *vp, int32_t v);
   // unsigned createExclusion
   //static TR::VPConstraint *createExclusion(OMR::ValuePropagation *vp, int32_t v);
   virtual TR::VPIntConst *asIntConst();
   virtual int32_t getHigh() {return _low;}
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   virtual int32_t getPrecision() {return getPrecisionFromValue(_low);}
   };

class VPIntRange : public TR::VPIntConstraint
   {
   public:
   VPIntRange(int32_t low, int32_t high) : TR::VPIntConstraint(low), _high(high) {}
   //static TR::VPIntConstraint *create(OMR::ValuePropagation *vp, int32_t low, int32_t high);
   static TR::VPIntConstraint *create(OMR::ValuePropagation *vp, int32_t low, int32_t high, TR_YesNoMaybe canOverflow = TR_no);
   static TR::VPIntConstraint *create(OMR::ValuePropagation *vp, TR::DataTypes dt, TR_YesNoMaybe isUnsigned); // Takes a TR::DataTypes instead of TR::DataType to work around ambiguous overloads
   static TR::VPIntConstraint *createWithPrecision(OMR::ValuePropagation *vp, TR::DataType dt, int32_t precision, TR_YesNoMaybe isUnsigned, bool isNonNegative = false);
   virtual TR::VPIntRange *asIntRange();
   virtual int32_t getHigh() {return _high;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   virtual int32_t getPrecision() {return getPrecisionFromRange(_low, _high);}

   private:
   int32_t _high;
   };

class VPLongConstraint : public TR::VPConstraint
   {
   public:
   VPLongConstraint(int64_t v) : TR::VPConstraint(LongPriority) {_low = v; _overflow = TR_no;}
   virtual TR::VPLongConstraint *asLongConstraint();

   int64_t getLong() {return _low;}
   uint64_t getUnsignedLong() {return (uint64_t)_low; }
   int64_t getLow() {return _low;}
   virtual int64_t getHigh() = 0;
   virtual int64_t getLowLong();
   virtual int64_t getHighLong();


   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual TR::VPConstraint *add(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *subtract(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);

   virtual TR_YesNoMaybe canOverflow() {return _overflow;}
   virtual void setCanOverflow(TR_YesNoMaybe v) {_overflow = v;}

   virtual int32_t getPrecision() {return getPrecisionFromValue(TR::getMaxSigned<TR::Int64>());}

   protected:
   int64_t _low;
   TR_YesNoMaybe _overflow;
   private:
   TR::VPConstraint *getRange(int64_t, int64_t, bool, bool, OMR::ValuePropagation *vp );

   };

class VPLongConst : public TR::VPLongConstraint
   {
   public:
   VPLongConst(int64_t v) : TR::VPLongConstraint(v) {}
   static TR::VPLongConst *create(OMR::ValuePropagation *vp, int64_t v);
   static TR::VPConstraint *createExclusion(OMR::ValuePropagation *vp, int64_t v);
   virtual TR::VPLongConst *asLongConst();
   virtual int64_t getHigh() {return _low;}
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   virtual int32_t getPrecision() {return getPrecisionFromValue(_low);}
   };

class VPLongRange : public TR::VPLongConstraint
   {
   public:
   VPLongRange(int64_t low, int64_t high) : TR::VPLongConstraint(low), _high(high) {}
   static TR::VPLongConstraint *create(OMR::ValuePropagation *vp, int64_t low, int64_t high, bool powerOfTwo = false, TR_YesNoMaybe canOverflow = TR_no);
   virtual TR::VPLongRange *asLongRange();
   virtual int64_t getHigh() {return _high;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   void setIsPowerOfTwo() {_isPowerOfTwo = true;}
   bool getIsPowerOfTwo() {return _isPowerOfTwo;}

   virtual int32_t getPrecision() {return getPrecisionFromRange(_low, _high);}

   private:
   int64_t _high;
   bool    _isPowerOfTwo;
   };


/* Floating Point Constraints */
class VPFloatConstraint : public TR::VPConstraint
   {
        /*
         * Data members
         */
   protected:
   float _low;
   TR_YesNoMaybe _overflow;

        /*
         * Function members
         */
   private:
   TR::VPConstraint *getRange(float, float, bool, bool, OMR::ValuePropagation * vp);

   public:
   VPFloatConstraint(float v) : TR::VPConstraint(FPConstraintPriority) {_low = v;}
   virtual TR::VPFloatConstraint *asFloatConstraint();

   float getFloat() {return _low;}
   float getLow() {return _low;}
   virtual float getHigh() = 0;
   virtual float getLowFloat();
   virtual float getHighFloat();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual TR::VPConstraint *add(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *subtract(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);

   virtual TR_YesNoMaybe canOverflow() {return _overflow;}
   virtual void setCanOverflow(TR_YesNoMaybe v) {_overflow = v;}
   };


class VPDoubleConstraint : public TR::VPConstraint
   {
        /*
         * Data members
         */
   protected:
   double _low;
   TR_YesNoMaybe _overflow;

        /*
         * Function members
         */
   private:
   TR::VPConstraint *getRange(double, double, bool, bool, OMR::ValuePropagation * vp);

   public:
   VPDoubleConstraint(double v) : TR::VPConstraint(FPConstraintPriority) {_low = v;}
   virtual TR::VPDoubleConstraint *asDoubleConstraint();

   double getDouble() {return _low;}
   double getLow() {return _low;}
   virtual double getHigh() = 0;
   virtual double getLowDouble();
   virtual double getHighDouble();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual TR::VPConstraint *add(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *subtract(TR::VPConstraint *other, TR::DataType type, OMR::ValuePropagation *vp);

   virtual TR_YesNoMaybe canOverflow() {return _overflow;}
   virtual void setCanOverflow(TR_YesNoMaybe v) {_overflow = v;}
   };


class VPFloatConst : public TR::VPFloatConstraint
   {
   public:
   VPFloatConst(float v) : TR::VPFloatConstraint(v) {}
   static TR::VPFloatConst *create(OMR::ValuePropagation *vp, float v);
   static TR::VPConstraint *createExclusion(OMR::ValuePropagation *vp, float v);
   virtual TR::VPFloatConst *asFloatConst();
   virtual float getHigh() {return _low;}
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };


class VPDoubleConst : public TR::VPDoubleConstraint
   {
   public:
   VPDoubleConst(double v) : TR::VPDoubleConstraint(v) {}
   static TR::VPDoubleConst *create(OMR::ValuePropagation *vp, double v);
   static TR::VPConstraint *createExclusion(OMR::ValuePropagation *vp, double v);
   virtual TR::VPDoubleConst *asDoubleConst();
   virtual double getHigh() {return _low;}
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };

class VPFloatRange : public TR::VPFloatConstraint
   {
        /*
         * Data members
         */
   private:
   float _high;

        /*
         * Function members
         */
   public:
   VPFloatRange(float low, float high) : TR::VPFloatConstraint(low), _high(high) {}
   static TR::VPFloatConstraint *create(OMR::ValuePropagation *vp, float low, float high, TR_YesNoMaybe canOverflow = TR_no);
   virtual TR::VPFloatRange *asFloatRange();
   virtual float getHigh() {return _high;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };


class VPDoubleRange : public TR::VPDoubleConstraint
   {
        /*
         * Data members
         */
   private:
   double _high;

        /*
         * Function members
         */
   public:
   VPDoubleRange(double low, double high) : TR::VPDoubleConstraint(low), _high(high) {}
   static TR::VPDoubleConstraint *create(OMR::ValuePropagation *vp, double low, double high, TR_YesNoMaybe canOverflow = TR_no);
   virtual TR::VPDoubleRange *asDoubleRange();
   virtual double getHigh() {return _high;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };


class VPClass : public TR::VPConstraint
   {
   public:
   VPClass(TR::VPClassType *type, TR::VPClassPresence *presence, TR::VPPreexistentObject *preexistence, TR::VPArrayInfo *arrayInfo, TR::VPObjectLocation *location)
      : TR::VPConstraint(ClassPriority), _type(type), _presence(presence),
        _preexistence(preexistence), _arrayInfo(arrayInfo), _location(location)
      {}
   static TR::VPConstraint *create(OMR::ValuePropagation *vp, TR::VPClassType *type, TR::VPClassPresence *presence, TR::VPPreexistentObject *preexistence, TR::VPArrayInfo *arrayInfo, TR::VPObjectLocation *location);
   virtual TR::VPClass *asClass();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   void typeIntersect(TR::VPClassPresence* &presence, TR::VPClassType* &type, TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool isNullObject();
   virtual bool isNonNullObject();
   virtual bool isPreexistentObject();
   virtual TR_OpaqueClassBlock *getClass();
   virtual bool isFixedClass();
   virtual bool isConstString();
   virtual TR::VPClassType *getClassType();
   virtual TR::VPArrayInfo *getArrayInfo();
   virtual TR::VPClassPresence *getClassPresence();
   virtual TR::VPPreexistentObject *getPreexistence();
   virtual TR::VPObjectLocation *getObjectLocation();
   virtual TR::VPConstString *getConstString();
   virtual TR::VPKnownObject *getKnownObject();

   virtual TR_YesNoMaybe isStackObject();
   virtual TR_YesNoMaybe isHeapObject();
   virtual TR_YesNoMaybe isClassObject();
   virtual TR_YesNoMaybe isJavaLangClassObject();
   virtual TR_YesNoMaybe isJ9ClassObject();

   virtual const char *getClassSignature(int32_t &len);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   private:
   TR::VPClassType         *_type;  // Conditional on presence.  "If the reference is non-null, it has this type info"
   TR::VPClassPresence     *_presence;
   TR::VPPreexistentObject *_preexistence;
   TR::VPArrayInfo         *_arrayInfo;
   TR::VPObjectLocation    *_location;
   };

class VPClassType : public TR::VPConstraint
   {
   public:
   VPClassType(int32_t p) : TR::VPConstraint(p) {}
   static TR::VPClassType *create(OMR::ValuePropagation *vp, TR::SymbolReference *symRef, bool isFixedClass, bool isPointerToClass = false);
   static TR::VPClassType *create(OMR::ValuePropagation *vp, const char *sig, int32_t len, TR_ResolvedMethod *method, bool isFixed, TR_OpaqueClassBlock *j9class = 0);
   virtual TR::VPClassType *asClassType();

   virtual const char *getClassSignature(int32_t &len) = 0;
   virtual TR::VPClassType *getClassType();
   virtual TR::VPClassType *getArrayClass(OMR::ValuePropagation *vp) = 0;

   virtual bool isReferenceArray(TR::Compilation *) = 0;
   virtual bool isPrimitiveArray(TR::Compilation *) = 0;

   virtual bool isJavaLangObject(OMR::ValuePropagation *vp) {return (_len == 18 && !strncmp(_sig, "Ljava/lang/Object;", 18));}

   virtual bool isCloneableOrSerializable();


   TR::DataType getPrimitiveArrayDataType();

   virtual TR_YesNoMaybe isClassObject();
   virtual TR_YesNoMaybe isJavaLangClassObject();
   virtual TR_YesNoMaybe isJ9ClassObject();
   virtual TR_YesNoMaybe isArray();

   TR::VPClassType *classTypesCompatible(TR::VPClassType * otherType, OMR::ValuePropagation *vp);

   protected:
   const char *_sig;
   int32_t _len;
   };

class VPResolvedClass : public TR::VPClassType
   {
   public:
   VPResolvedClass(TR_OpaqueClassBlock *klass, TR::Compilation *);
   VPResolvedClass(TR_OpaqueClassBlock *klass, TR::Compilation *, int32_t p);
   static TR::VPResolvedClass *create(OMR::ValuePropagation *vp, TR_OpaqueClassBlock *klass);
   virtual TR::VPResolvedClass *asResolvedClass();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual const char *getClassSignature(int32_t &len);

   virtual TR_OpaqueClassBlock *getClass();
   virtual TR::VPClassType *getArrayClass(OMR::ValuePropagation *vp);

   virtual bool isReferenceArray(TR::Compilation *);
   virtual bool isPrimitiveArray(TR::Compilation *);
   bool isJavaLangObject(OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   protected:
   TR_OpaqueClassBlock *_class;
   };

class VPFixedClass : public TR::VPResolvedClass
   {
   public:
   VPFixedClass(TR_OpaqueClassBlock *klass, TR::Compilation * comp, int32_t p = FixedClassPriority) : TR::VPResolvedClass(klass, comp, p) {}
   static TR::VPFixedClass *create(OMR::ValuePropagation *vp, TR_OpaqueClassBlock *klass);
   virtual TR::VPFixedClass *asFixedClass();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool isFixedClass();
   virtual TR::VPClassType *getArrayClass(OMR::ValuePropagation *vp);

   virtual TR_YesNoMaybe isArray();

   virtual bool hasMoreThanFixedClassInfo(){ return false; }

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };

class VPConstString : public TR::VPFixedClass
   {
   protected:
   VPConstString(TR_OpaqueClassBlock *klass, TR::Compilation * comp, TR::SymbolReference *symRef)
      : TR::VPFixedClass(klass, comp, ConstStringPriority), _symRef(symRef) {}
   public:
   static TR::VPConstString *create(OMR::ValuePropagation *vp, TR::SymbolReference *symRef);
   virtual TR::VPConstString *asConstString();
   virtual TR::VPConstString *getConstString(){ return this; }
   virtual bool isConstString();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   uint16_t  charAt(int32_t index, TR::Compilation *);
   bool      getFieldByName(TR::SymbolReference *fieldRef, void* &result, TR::Compilation *);
   TR::SymbolReference* getSymRef() {return _symRef;}

   virtual bool hasMoreThanFixedClassInfo(){ return true; }

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   protected:
   TR::SymbolReference *_symRef;
   };

class VPUnresolvedClass : public TR::VPClassType
   {
   public:
   VPUnresolvedClass(const char *sig, int32_t len, TR_ResolvedMethod *method)
      : TR::VPClassType(UnresolvedClassPriority), _method(method)
      {_sig = sig; _len = len; _definiteType = false;}
   static TR::VPUnresolvedClass *create(OMR::ValuePropagation *vp, const char *sig, int32_t len, TR_ResolvedMethod *method);
   virtual TR::VPUnresolvedClass *asUnresolvedClass();

   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual const char *getClassSignature(int32_t &len);
   virtual TR::VPClassType *getArrayClass(OMR::ValuePropagation *vp);

   TR_ResolvedMethod *getOwningMethod() { return _method; }

   virtual bool isReferenceArray(TR::Compilation *);
   virtual bool isPrimitiveArray(TR::Compilation *);

   bool isDefiniteType()  {return _definiteType;}
   void setDefiniteType(bool b) {_definiteType = b;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   TR_ResolvedMethod *getMethod() {return _method; }

   private:
   TR_ResolvedMethod *_method;
   bool _definiteType;
   };

class VPClassPresence : public TR::VPConstraint
   {
   public:
   VPClassPresence() : TR::VPConstraint(ClassPresencePriority) {}
   virtual TR::VPClassPresence *asClassPresence();
   virtual TR::VPClassPresence *getClassPresence();
   };

class VPNullObject : public TR::VPClassPresence
   {
   public:
   static TR::VPNullObject *create(OMR::ValuePropagation *vp);
   virtual TR::VPNullObject *asNullObject();

   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool isNullObject();

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };

class VPNonNullObject : public TR::VPClassPresence
   {
   public:
   static TR::VPNonNullObject *create(OMR::ValuePropagation *vp);
   virtual TR::VPNonNullObject *asNonNullObject();

   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool isNonNullObject();

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };

class VPPreexistentObject : public TR::VPConstraint
   {
   public:
   VPPreexistentObject(TR_OpaqueClassBlock *c = NULL) : TR::VPConstraint(ClassPreexistencePriority) { _assumptionClass = c;}
   static TR::VPPreexistentObject *create(OMR::ValuePropagation *vp, TR_OpaqueClassBlock *c = NULL);
   virtual TR::VPPreexistentObject *asPreexistentObject();

   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool isPreexistentObject();

   virtual TR::VPPreexistentObject *getPreexistence();

   TR_OpaqueClassBlock *getAssumptionClass() { return _assumptionClass; }

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   TR_OpaqueClassBlock *_assumptionClass;
   };

class VPArrayInfo : public TR::VPConstraint
   {
   public:
   VPArrayInfo(int32_t lowBound, int32_t highBound, int32_t elementSize)
      : TR::VPConstraint(ArrayInfoPriority), _lowBound(lowBound), _highBound(highBound), _elementSize(elementSize)
      {}
   static TR::VPArrayInfo *create(OMR::ValuePropagation *vp, int32_t lowBound, int32_t highBound, int32_t elementSize);
   static TR::VPArrayInfo *create(OMR::ValuePropagation *vp, char *sig);
   virtual TR::VPArrayInfo *asArrayInfo();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPArrayInfo *getArrayInfo();

   int32_t lowBound()  {return _lowBound;}
   int32_t highBound() {return _highBound;}
   int32_t elementSize() {return _elementSize;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   private:
   int32_t  _lowBound;
   int32_t  _highBound;
   int32_t  _elementSize;
   };

/** Constrains the referent (if any) of an address to a set of "locations".
 *
 * The atomic locations are HeapObject, StackObject, JavaLangClassObject, and
 * J9ClassObject. The sets named in VPObjectLocationKind fall into the
 * following lattice structure.
 *
 * \verbatim
                              top
                (implicit in lack of constraint)
                               |
              +----------------+---------------+
              |                |               |
      NotClassObject     NotStackObject   NotHeapObject
              |     \    /          \    /     |
              |      \  /            \  /      |
              |       \/              \/       |
              |       /\              /\       |
              |      /  \            /  \      |
              |     /    \          /    \     |
          HeapObject      StackObject     ClassObject
              |                |            /      \
              |                |           /        \
              |                |  J9ClassObject   JavaLangClassObject
              |                |       |                  |
              +----------------+-------+------------------+
                               |
                            bottom
                   (either intersection failure
                    or     TR::VPUnreachablePath)
   \endverbatim
 *
 * Note that top and bottom are not represented by instances of this class.
 *
 * In general we could have other arbitrary sets of the primitive locations,
 * e.g. {StackObject, J9ClassObject}, but currently nothing should create them.
 *
 * \par Warning: interaction with TR::VPClassType
 *
 * A location constraint that requires the referent to be a representation of a
 * class (i.e. JavaLangClassObject, J9ClassObject, or ClassObject) alters the
 * interpretation of any TR::VPClassType constraint beside it in an enclosing
 * TR::VPClass. In this case the TR::VPClassType constrains the \em represented
 * class. rather than the class of which the referent is an instance.
 */
class VPObjectLocation : public TR::VPConstraint
   {
   public:
   enum VPObjectLocationKind
      {
      HeapObject          = 1 << 0,
      StackObject         = 1 << 1,
      JavaLangClassObject = 1 << 2,
      J9ClassObject       = 1 << 3,
      ClassObject         = JavaLangClassObject | J9ClassObject,
      NotHeapObject       = StackObject | ClassObject,
      NotStackObject      = HeapObject | ClassObject,
      NotClassObject      = HeapObject | StackObject,
      };

   static  TR::VPObjectLocation *create(OMR::ValuePropagation *vp, VPObjectLocationKind kind);

   virtual TR::VPObjectLocation *asObjectLocation();
   virtual TR::VPObjectLocation *getObjectLocation();
   virtual TR::VPConstraint     *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint     *merge1    (TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual TR_YesNoMaybe isStackObject();
   virtual TR_YesNoMaybe isHeapObject();
   virtual TR_YesNoMaybe isClassObject();
   virtual TR_YesNoMaybe isJavaLangClassObject();
   virtual TR_YesNoMaybe isJ9ClassObject();

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   private:
   VPObjectLocation(VPObjectLocationKind kind) : TR::VPConstraint(ObjectLocationPriority), _kind(kind) {}

   TR_YesNoMaybe isWithin(VPObjectLocationKind area);
   static bool isKindSubset(VPObjectLocationKind x, VPObjectLocationKind y)
      {
      return (x & ~y) == 0;
      }

   VPObjectLocationKind _kind;
   };

class VPKnownObject : public TR::VPFixedClass
   {
   // IMPORTANT NOTE: Just like with class constraints, TR::VPKnownObject tells
   // you "if this reference is non-null, it points at object X".  Consumers of
   // TR::VPKnownObject must bear in mind that NULL is always a possibility.
   //
   // This kind of conditional constraint is subtle, but we do it this way
   // because it allows us to achieve better optimization.  Usages of
   // potentially null object references are practically always preceded by
   // null checks, and in these cases, constraints that are conditional on
   // non-nullness allow us to retain information that would be lost if the
   // constraints were required to be unconditional.
   //
   // We practically always have a TR::VPClass constraint that contains both
   // Known Object and isNonNullObject info, and together, these tell us
   // reliably that a given reference definitely points at a particular object.
   //
   // (There are shades of Deductive Reaching Definitions here.  This technique
   // allows us to preserve information across control merges that would
   // otherwise be lost.)

   typedef TR::VPFixedClass Super;

   protected:
   VPKnownObject(TR_OpaqueClassBlock *klass, TR::Compilation * comp, TR::KnownObjectTable::Index index, bool isJavaLangClass)
      : TR::VPFixedClass(klass, comp, KnownObjectPriority), _index(index), _isJavaLangClass(isJavaLangClass) {}

   static TR::VPKnownObject *create(OMR::ValuePropagation *vp, TR::KnownObjectTable::Index index, bool isJavaLangClass);

   public:
   static TR::VPKnownObject *create(OMR::ValuePropagation *vp, TR::KnownObjectTable::Index index)
      {
      return create(vp, index, false);
      }

   static TR::VPKnownObject *createForJavaLangClass(OMR::ValuePropagation *vp, TR::KnownObjectTable::Index index)
      {
      return create(vp, index, true);
      }

   virtual TR::VPKnownObject *asKnownObject();
   virtual TR::VPKnownObject *getKnownObject(){ return this; }

   TR::KnownObjectTable::Index getIndex(){ return _index; }

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual TR_YesNoMaybe isJavaLangClassObject();
   virtual bool isArrayWithConstantElements(TR::Compilation * comp);

   virtual bool mustBeEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool hasMoreThanFixedClassInfo(){ return true; }

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   protected:
   TR::KnownObjectTable::Index _index;
   bool _isJavaLangClass;
   };


/*
class VPImplementedInterface : public TR::VPConstraint
   {
   public:

   static  TR::VPImplementedInterface *create(OMR::ValuePropagation *vp, char *sig, int32_t len, TR_ResolvedMethod *method);

   VPImplementedInterface(char *sig, int32_t len, TR_ResolvedMethod *method)
      : _method(method)
      {_sig = sig; _len = len;}

   virtual TR::VPImplementedInterface *asImplementedInterface();

   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *merge1    (TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual char *getInterfaceSignature(int32_t &len);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   private:
   TR_ResolvedMethod *_method;
   char *_sig;
   int32_t _len;
   };
*/






class VPMergedConstraints : public TR::VPConstraint
   {
   public:
   VPMergedConstraints(ListElement<TR::VPConstraint> *first, TR_Memory * m)
      : TR::VPConstraint(MergedConstraintPriority),
        _constraints(m)
      {
          _constraints.setListHead(first);
          if (first)
            {
            if (first->getData()->asShortConstraint())
               _type = TR::Int16;
            else if (first->getData()->asIntConstraint())
               _type = TR::Int32;
            else if (first->getData()->asLongConstraint())
               _type = TR::Int64;
            else if (first->getData()->asFloatConstraint())
               _type = TR::Float;
            else if (first->getData()->asDoubleConstraint())
               _type = TR::Double;
            }
      }
   static TR::VPMergedConstraints *create(OMR::ValuePropagation *vp, TR::VPConstraint *first, TR::VPConstraint *second);
   static TR::VPMergedConstraints *create(OMR::ValuePropagation *vp, ListElement<TR::VPConstraint> *list);
   virtual TR::VPMergedConstraints *asMergedConstraints();
   virtual TR::VPMergedConstraints *asMergedShortConstraints();
   virtual TR::VPMergedConstraints *asMergedIntConstraints();
   virtual TR::VPMergedConstraints *asMergedLongConstraints();
   virtual TR::VPMergedConstraints *asMergedFloatConstraints();
   virtual TR::VPMergedConstraints *asMergedDoubleConstraints();

   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeNotEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThan(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual bool mustBeLessThanOrEqual(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual int16_t getLowShort();
   virtual int16_t getHighShort();
   virtual int32_t getLowInt();
   virtual int32_t getHighInt();
   virtual int64_t getLowLong();
   virtual int64_t getHighLong();
   virtual float getLowFloat();
   virtual float getHighFloat();
   virtual double getLowDouble();
   virtual double getHighDouble();

   virtual uint16_t getUnsignedLowShort();
   virtual uint16_t getUnsignedHighShort();
   virtual uint32_t getUnsignedLowInt();
   virtual uint32_t getUnsignedHighInt();
   virtual uint64_t getUnsignedLowLong();
   virtual uint64_t getUnsignedHighLong();


   TR_ScratchList<TR::VPConstraint> *getList() {return &_constraints;}
   TR::DataType                     getType() {return _type;}

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   private:
   TR::VPConstraint *shortMerge(TR::VPConstraint * otherCur, ListElement<TR::VPConstraint> * otherNext, OMR::ValuePropagation * vp);
   TR::VPConstraint *shortIntersect(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> * otherNext, OMR::ValuePropagation * vp);

   // unsigned intMerge
   //   TR::VPConstraint *intMerge(TR::VPIntConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp, bool isUnsigned);
   TR::VPConstraint *intMerge(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);
   TR::VPConstraint *intIntersect(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);

   // unsigned intIntersect
   //TR::VPConstraint *intIntersect(TR::VPIntConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp, bool isUnsigned);
   TR::VPConstraint *longMerge(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);
   TR::VPConstraint *longIntersect(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);

   TR::VPConstraint *floatMerge(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);
   TR::VPConstraint *floatIntersect(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);

   TR::VPConstraint *doubleMerge(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);
   TR::VPConstraint *doubleIntersect(TR::VPConstraint *otherCur, ListElement<TR::VPConstraint> *otherNext, OMR::ValuePropagation *vp);

   TR_ScratchList<TR::VPConstraint> _constraints;
   TR::DataType                    _type;
   };

class VPUnreachablePath : public TR::VPConstraint
   {
   public:
   VPUnreachablePath() : TR::VPConstraint(0) {}
   static TR::VPUnreachablePath *create(OMR::ValuePropagation *vp);
   virtual TR::VPUnreachablePath *asUnreachablePath();

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();
   };

// breaks handcuff rule
class VPSync : public TR::VPConstraint
   {
   public:
   VPSync(TR_YesNoMaybe v) : TR::VPConstraint(0), _syncEmitted(v) {}
   static TR::VPSync *create(OMR::ValuePropagation *vp, TR_YesNoMaybe v);
   virtual TR::VPSync *asVPSync();
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   TR_YesNoMaybe syncEmitted() { return _syncEmitted; }
   ////void setSyncEmitted(TR_YesNoMaybe v) { _syncEmitted = v; }


   virtual void print(TR::Compilation *, TR::FILE *);
   virtual const char *name();

   private:
   TR_YesNoMaybe _syncEmitted;
   };

class VPRelation : public TR::VPConstraint
   {
   public:

   VPRelation(int32_t incr, int32_t p)
      : TR::VPConstraint(p), _increment(incr)
      {}
   virtual TR::VPRelation *asRelation();
   int32_t      increment() {return _increment;}

   virtual TR::VPConstraint *propagateAbsoluteConstraint(
                               TR::VPConstraint *constraint,
                               int32_t relative,
                               OMR::ValuePropagation *vp);
   virtual TR::VPConstraint*propagateRelativeConstraint(
                               TR::VPRelation *other,
                               int32_t relative, int32_t otherRelative,
                               OMR::ValuePropagation *vp) = 0;

   virtual TR::VPRelation *getComplement(OMR::ValuePropagation *vp) = 0;

   protected:
   int32_t      _increment;
   };

class VPLessThanOrEqual : public TR::VPRelation
   {
   public:

   VPLessThanOrEqual(int32_t incr)
      : TR::VPRelation(incr, LessThanOrEqualPriority)
      {}
   virtual TR::VPLessThanOrEqual *asLessThanOrEqual();
   static TR::VPLessThanOrEqual *create(OMR::ValuePropagation *vp, int32_t incr);
   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool mustBeNotEqual();
   virtual bool mustBeLessThan();
   virtual bool mustBeLessThanOrEqual();

   virtual TR::VPConstraint *propagateAbsoluteConstraint(
                               TR::VPConstraint *constraint,
                               int32_t relative,
                               OMR::ValuePropagation *vp);
   virtual TR::VPConstraint*propagateRelativeConstraint(
                               TR::VPRelation *other,
                               int32_t relative, int32_t otherRelative,
                               OMR::ValuePropagation *vp);

   virtual TR::VPRelation *getComplement(OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual void print(TR::Compilation *, TR::FILE *, int32_t relative);
   virtual const char *name();
   };

class VPGreaterThanOrEqual : public TR::VPRelation
   {
   public:

   VPGreaterThanOrEqual(int32_t incr)
      : TR::VPRelation(incr, GreaterThanOrEqualPriority)
      {}
   virtual TR::VPGreaterThanOrEqual *asGreaterThanOrEqual();
   static TR::VPGreaterThanOrEqual *create(OMR::ValuePropagation *vp, int32_t incr);
   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool mustBeNotEqual();
   virtual bool mustBeGreaterThan();
   virtual bool mustBeGreaterThanOrEqual();

   virtual TR::VPConstraint *propagateAbsoluteConstraint(
                               TR::VPConstraint *constraint,
                               int32_t relative,
                               OMR::ValuePropagation *vp);
   virtual TR::VPConstraint*propagateRelativeConstraint(
                               TR::VPRelation *other,
                               int32_t relative, int32_t otherRelative,
                               OMR::ValuePropagation *vp);

   virtual TR::VPRelation *getComplement(OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual void print(TR::Compilation *, TR::FILE *, int32_t relative);
   virtual const char *name();
   };

class VPEqual : public TR::VPRelation
   {
   public:

   VPEqual(int32_t incr)
      : TR::VPRelation(incr, EqualPriority)
      {}
   virtual TR::VPEqual *asEqual();
   static TR::VPEqual *create(OMR::ValuePropagation *vp, int32_t incr);
   virtual TR::VPConstraint *merge1(TR::VPConstraint *other, OMR::ValuePropagation *vp);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool mustBeEqual();
   virtual bool mustBeNotEqual();
   virtual bool mustBeLessThan();
   virtual bool mustBeLessThanOrEqual();
   virtual bool mustBeGreaterThan();
   virtual bool mustBeGreaterThanOrEqual();

   virtual TR::VPConstraint *propagateAbsoluteConstraint(
                               TR::VPConstraint *constraint,
                               int32_t relative,
                               OMR::ValuePropagation *vp);
   virtual TR::VPConstraint*propagateRelativeConstraint(
                               TR::VPRelation *other,
                               int32_t relative, int32_t otherRelative,
                               OMR::ValuePropagation *vp);

   virtual TR::VPRelation *getComplement(OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual void print(TR::Compilation *, TR::FILE *, int32_t relative);
   virtual const char *name();
   };

class VPNotEqual : public TR::VPRelation
   {
   public:

   VPNotEqual(int32_t incr)
      : TR::VPRelation(incr, NotEqualPriority)
      {}
   virtual TR::VPNotEqual *asNotEqual();
   static TR::VPNotEqual *create(OMR::ValuePropagation *vp, int32_t incr);
   virtual TR::VPConstraint *intersect1(TR::VPConstraint *other, OMR::ValuePropagation *vp);

   virtual bool mustBeNotEqual();

   virtual TR::VPConstraint *propagateAbsoluteConstraint(
                               TR::VPConstraint *constraint,
                               int32_t relative,
                               OMR::ValuePropagation *vp);
   virtual TR::VPConstraint*propagateRelativeConstraint(
                               TR::VPRelation *other,
                               int32_t relative, int32_t otherRelative,
                               OMR::ValuePropagation *vp);

   virtual TR::VPRelation *getComplement(OMR::ValuePropagation *vp);

   virtual void print(TR::Compilation *, TR::FILE *);
   virtual void print(TR::Compilation *, TR::FILE *, int32_t relative);
   virtual const char *name();
   };

}

#endif
