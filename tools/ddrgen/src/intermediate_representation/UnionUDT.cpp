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

#include "UnionUDT.hpp"

UnionUDT::UnionUDT(size_t size, unsigned int lineNumber)
	: ClassType(UNION, size, lineNumber)
{
}

UnionUDT::~UnionUDT() {};

DDR_RC
UnionUDT::scanChildInfo(Scanner *scanner, void *data)
{
	return scanner->dispatchScanChildInfo(this, data);
}

DDR_RC
UnionUDT::enumerateType(BlobGenerator *blobGenerator, bool addFieldsOnly)
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
UnionUDT::buildBlob(BlobGenerator *blobGenerator, bool addFieldsOnly, string prefix)
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
UnionUDT::printToSuperset(SupersetGenerator *supersetGenerator, bool addFieldsOnly, string prefix = "")
{
	return supersetGenerator->dispatchPrintToSuperset(this, addFieldsOnly, prefix);
}
