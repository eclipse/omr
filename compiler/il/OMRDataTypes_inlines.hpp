/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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

#ifndef OMR_DATATYPES_INLINES_INCL
#define OMR_DATATYPES_INLINES_INCL

#include "il/OMRDataTypes.hpp"

TR::DataType*
OMR::DataType::self()
   {
   return static_cast<TR::DataType*>(this);
   }

const TR::DataType*
OMR::DataType::self() const
   {
   return static_cast<const TR::DataType*>(this);
   }

OMR::DataType::operator int()
   {
   return self()->getDataType();
   }

bool
OMR::DataType::isInt8()
   {
   return self()->getDataType() == TR::Int8;
   }

bool
OMR::DataType::isInt16()
   {
   return self()->getDataType() == TR::Int16;
   }

bool
OMR::DataType::isInt32()
   {
   return self()->getDataType() == TR::Int32;
   }

bool
OMR::DataType::isInt64()
   {
   return self()->getDataType() == TR::Int64;
   }

bool
OMR::DataType::isIntegral()
   {
   return self()->isInt8() || self()->isInt16() || self()->isInt32() || self()->isInt64();
   }

bool
OMR::DataType::isFloatingPoint()
   {
   return self()->isBFPorHFP();
   }

bool
OMR::DataType::isVector()
   {
   return self()->getDataType() == TR::VectorInt8 || self()->getDataType() == TR::VectorInt16 || self()->getDataType() == TR::VectorInt32 || self()->getDataType() == TR::VectorInt64 ||
                        self()->getDataType() == TR::VectorFloat || self()->getDataType() == TR::VectorDouble;
   }

bool
OMR::DataType::isBFPorHFP()
   {
   return self()->getDataType() == TR::Float || self()->getDataType() == TR::Double;
   }

bool
OMR::DataType::isDouble()
   {
   return self()->getDataType() == TR::Double;
   }

bool
OMR::DataType::isAddress()
   {
   return self()->getDataType() == TR::Address;
   }

bool
OMR::DataType::isAggregate()
   {
   return self()->getDataType() == TR::Aggregate;
   }

TR::DataType&
OMR::DataType::operator=(const TR::DataType& rhs)
   {
   _type = rhs._type;
   return *(static_cast<TR::DataType *>(this));
   }

TR::DataType&
OMR::DataType::operator=(TR::DataTypes rhs)
   {
   _type = rhs;
   return *(static_cast<TR::DataType *>(this));
   }

bool
OMR::DataType::operator==(const TR::DataType& rhs)
   {
   return _type == rhs._type;
   }

bool
OMR::DataType::operator==(TR::DataTypes rhs)
   {
   return _type == rhs;
   }

bool
OMR::DataType::operator!=(const TR::DataType& rhs)
   {
   return _type != rhs._type;
   }

bool
OMR::DataType::operator!=(TR::DataTypes rhs)
   {
   return _type != rhs;
   }

bool
OMR::DataType::operator<=(const TR::DataType& rhs)
   {
   return _type <= rhs._type;
   }

bool
OMR::DataType::operator<=(TR::DataTypes rhs)
   {
   return _type <= rhs;
   }

bool		
OMR::DataType::operator<(const TR::DataType& rhs)		
   {		
   return _type < rhs._type;		
   }		
		
bool		
OMR::DataType::operator<(TR::DataTypes rhs)		
   {		
   return _type < rhs;		
   }		
		
bool		
OMR::DataType::operator>=(const TR::DataType& rhs)		
   {		
   return _type >= rhs._type;		
   }		
		
bool		
OMR::DataType::operator>=(TR::DataTypes rhs)		
   {		
   return _type >= rhs;		
   }		
		
bool		
OMR::DataType::operator>(const TR::DataType& rhs)		
   {		
   return _type > rhs._type;		
   }		
		
bool		
OMR::DataType::operator>(TR::DataTypes rhs)		
   {		
   return _type > rhs;		
   }		

#endif // OMR_DATATYPES_INLINES_INCL
