/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016
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

#include "NamespaceUDT.hpp"

NamespaceUDT::NamespaceUDT(unsigned int lineNumber)
	: UDT(NAMESPACE, 0, lineNumber)
{
}

NamespaceUDT::~NamespaceUDT()
{
	for (size_t i = 0; i < _subUDTs.size(); i += 1) {
		delete(_subUDTs[i]);
	}
	_subUDTs.clear();
}

bool
NamespaceUDT::equal(Type const& type, set<Type const*> *checked) const
{
	bool ret = false;
	if (checked->find(this) != checked->end()) {
		ret = true;
	} else {
		checked->insert(this);
		NamespaceUDT const *ns = dynamic_cast<NamespaceUDT const *>(&type);
		if (NULL != ns) {
			bool subUDTsEqual = _subUDTs.size() == ns->_subUDTs.size();
			if (subUDTsEqual) {
				for (size_t i = 0; i < _subUDTs.size(); i += 1) {
					if ((_subUDTs[i] != ns->_subUDTs[i])
						&& !(*_subUDTs[i] == *ns->_subUDTs[i])
					) {
						subUDTsEqual = false;
						break;
					}
				}
			}

			bool macrosEqual = _macros.size() == ns->_macros.size() && subUDTsEqual;
			if (macrosEqual) {
				for (size_t i = 0; i < _macros.size(); i += 1) {
					if ((_macros[i].getValue() != ((Macro)ns->_macros[i]).getValue())
						|| (_macros[i]._name != ns->_macros[i]._name)
					) {
						macrosEqual = false;
						break;
					}
				}
			}

			ret = (UDT::equal(type, checked))
				&& (subUDTsEqual)
				&& (macrosEqual);
		}
	}
	return ret;
}

void
NamespaceUDT::replaceType(Type *typeToReplace, Type *replaceWith)
{
	UDT::replaceType(typeToReplace, replaceWith);
	UDT *udtFrom = dynamic_cast<UDT *>(typeToReplace);
	UDT *udtTo = dynamic_cast<UDT *>(replaceWith);
	for (size_t i = 0; i < _subUDTs.size(); i += 1) {
		if (_subUDTs[i] == udtFrom) {
			_subUDTs[i] = udtTo;
		} else {
			_subUDTs[i]->replaceType(typeToReplace, replaceWith);
		}
	}
}

DDR_RC
NamespaceUDT::scanChildInfo(Scanner *scanner, void *data)
{
	return scanner->dispatchScanChildInfo(this, data);
}

DDR_RC
NamespaceUDT::enumerateType(BlobGenerator *blobGenerator, bool addFieldsOnly)
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
NamespaceUDT::buildBlob(BlobGenerator *blobGenerator, bool addFieldsOnly, string prefix)
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
NamespaceUDT::printToSuperset(SupersetGenerator *supersetGenerator, bool addFieldsOnly, string prefix = "")
{
	return supersetGenerator->dispatchPrintToSuperset(this, addFieldsOnly, prefix);
}
