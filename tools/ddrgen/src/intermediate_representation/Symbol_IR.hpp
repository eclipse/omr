/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#ifndef SYMBOL_IR_HPP
#define SYMBOL_IR_HPP

#include <vector>

#include "omrport.h"

#include "config.hpp"
#include "Type.hpp"

using std::vector;

class Symbol_IR {
public:
	vector<Type *> _types;

	~Symbol_IR();

	DDR_RC applyOverrideList(OMRPortLibrary *portLibrary, const char *overrideFiles);
	DDR_RC computeOffsets();
	DDR_RC removeDuplicates();

private:
	DDR_RC applyOverrides(OMRPortLibrary *portLibrary, const char *overrideFile);
	DDR_RC computeFieldOffsets(Type *type);
};

#endif /* SYMBOL_IR_HPP */
