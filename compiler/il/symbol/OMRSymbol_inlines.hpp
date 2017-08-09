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

#ifndef OMR_SYMBOL_INLINES_INCL
#define OMR_SYMBOL_INLINES_INCL

#include "il/symbol/OMRSymbol.hpp"

/**
 * Downcast to concrete instance
 *
 * This method is defined out-of-line because it requires a complete definition
 * of OMR::Symbol which is unavailable at the time the class OMR::Symbol is
 * being defined.
 *
 * This must be a static_cast or else the downcast will be unsafe.
 */
TR::Symbol *
OMR::Symbol::self()
   {
   return static_cast<TR::Symbol *>(this);
   }

#endif // OMR_SYMBOL_INLINES_INCL
