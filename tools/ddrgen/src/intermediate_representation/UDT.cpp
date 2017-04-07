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

#include "UDT.hpp"

#include "config.hpp"

#if defined(AIXPPC)
UDT::UDT(SymbolType symbolType, size_t size, unsigned int lineNumber)
	: Type(symbolType, size), _outerUDT(NULL), _lineNumber(lineNumber), _isDuplicate(false)
#else /* defined(AIXPPC) */
UDT::UDT(SymbolType symbolType, size_t size, unsigned int lineNumber)
	: Type(symbolType, size), _outerUDT(NULL), _lineNumber(lineNumber)
#endif /* !defined(AIXPPC) */
{
}

UDT::~UDT()
{
}

bool
UDT::equal(Type const& type, set<Type const*> *checked) const
{
	bool ret = false;
	if (checked->find(this) != checked->end()) {
		ret = true;
	} else {
		checked->insert(this);
		UDT const *udt = dynamic_cast<UDT const *>(&type);
		if (NULL == udt) {
			ret = false;
		} else {
			/* Do not check if the outer UDT is equal when comparing sub UDTs,
			 * otherwise an infinite loop will be created.
			 */
			ret = (Type::equal(type, checked))
				&& ((_outerUDT == udt->_outerUDT) || (*_outerUDT == *udt->_outerUDT))
				&& (_lineNumber == udt->_lineNumber);
		}
	}
	return ret;
}

void
UDT::replaceType(Type *typeToReplace, Type *replaceWith)
{
	Type::replaceType(typeToReplace, replaceWith);
	if (_outerUDT == typeToReplace) {
		UDT *udt = dynamic_cast<UDT *>(replaceWith);
		_outerUDT = udt;
	}
}

string
UDT::getFullName()
{
	return ((NULL == _outerUDT) ? "" : _outerUDT->getFullName() + "::") + _name;
}
