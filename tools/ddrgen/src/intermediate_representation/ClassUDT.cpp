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

#include "ClassUDT.hpp"

ClassUDT::ClassUDT(size_t size, bool isClass, unsigned int lineNumber)
	: ClassType(isClass ? CLASS : STRUCT, size, lineNumber), _superClass(NULL), _isClass(isClass)
{
}

ClassUDT::~ClassUDT() {};

bool
ClassUDT::equal(Type const& type, set<Type const*> *checked) const
{
	bool ret = false;
	if (checked->find(this) != checked->end()) {
		ret = true;
	} else {
		checked->insert(this);
		ClassUDT const *classUDT = dynamic_cast<ClassUDT const *>(&type);
		if (NULL != classUDT) {
			ret = (ClassType::equal(type, checked))
				&& (( NULL == _superClass) == (NULL == classUDT->_superClass))
				&& ((_superClass == classUDT->_superClass) || (*_superClass == *classUDT->_superClass));
		}
	}
	return ret;
}

void
ClassUDT::replaceType(Type *typeToReplace, Type *replaceWith)
{
	ClassType::replaceType(typeToReplace, replaceWith);

	ClassUDT *classUDT = dynamic_cast<ClassUDT *>(replaceWith);
	if (_superClass == typeToReplace) {
		_superClass = classUDT;
	}
}

DDR_RC
ClassUDT::scanChildInfo(Scanner *scanner, void *data)
{
	return scanner->dispatchScanChildInfo(this, data);
}

string
ClassUDT::getSymbolTypeName()
{
	return _isClass ? "class" : "struct";
}

DDR_RC
ClassUDT::enumerateType(BlobGenerator *blobGenerator, bool addFieldsOnly)
{
#if defined(AIXPPC)
	if (!_isDuplicate) {
		return blobGenerator->dispatchEnumerateType(this, addFieldsOnly);
	} else {
		return DDR_RC_OK;
	}
#else /* defined(AIXPPC) */
	return blobGenerator->dispatchEnumerateType(this, addFieldsOnly);
#endif /* !defined(AIXPPC) */
}

DDR_RC
ClassUDT::buildBlob(BlobGenerator *blobGenerator, bool addFieldsOnly, string prefix)
{
#if defined(AIXPPC)
	if (!_isDuplicate) {
		return blobGenerator->dispatchBuildBlob(this, addFieldsOnly, prefix);
	} else {
		return DDR_RC_OK;
	}
#else /* defined(AIXPPC) */
	return blobGenerator->dispatchBuildBlob(this, addFieldsOnly, prefix);
#endif /* !defined(AIXPPC) */
}

DDR_RC
ClassUDT::printToSuperset(SupersetGenerator *supersetGenerator, bool addFieldsOnly, string prefix = "")
{
	return supersetGenerator->dispatchPrintToSuperset(this, addFieldsOnly, prefix);
}
