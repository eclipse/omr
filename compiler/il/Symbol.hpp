/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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

/**
 * \page Symbols Symbols
 *
 * A Symbol is a name for a program object.
 *
 * In OMR, symbols have three main attributes:
 *
 * - A datatype
 * - A size
 * - A name
 *
 * Symbols are further divided into four subclasses:
 *
 * -# \a RegisterMapped symbols are those that can be mapped to registers.
 * -# \a Static symbols are those that can have an address computed at compile
 *       time.
 * -# \a Method symbols name executable code pieces.
 * -# \a Label symbols give names to locations in code.
 *
 * Symbol creation is done through factory methods.
 *
 * See \ref symbolcreation for details.
 *
 */

#ifndef TR_SYMBOL_INCL
#define TR_SYMBOL_INCL

#include "il/symbol/OMRSymbol.hpp"

#include <stdint.h>          // for uint32_t
#include "il/DataTypes.hpp"  // for DataTypes

namespace TR
{

class OMR_EXTENSIBLE Symbol : public OMR::SymbolConnector
   {

public:

   Symbol() :
      OMR::SymbolConnector() {}

   Symbol(TR::DataType d) :
      OMR::SymbolConnector(d) {}

   Symbol(TR::DataType d, uint32_t s) :
      OMR::SymbolConnector(d,s) {}

   };

}

#include "il/Symbol_inlines.hpp"

#endif
