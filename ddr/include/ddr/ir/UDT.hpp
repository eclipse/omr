/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef UDT_HPP
#define UDT_HPP

#include "ddr/config.hpp"

#include <vector>

#include "ddr/ir/Members.hpp"
#include "ddr/ir/Macro.hpp"
#include "ddr/ir/Type.hpp"

class UDT : public Type
{
public:
	NamespaceUDT *_outerNamespace;
	unsigned int _lineNumber;

	UDT(size_t size, unsigned int lineNumber = 0);
	virtual ~UDT();

	virtual string getFullName();
	virtual void checkDuplicate(Symbol_IR *ir);
	virtual NamespaceUDT * getNamespace();

	bool operator==(Type const & rhs) const;
	virtual bool compareToUDT(UDT const &) const;
};

#endif /* UDT_HPP */
