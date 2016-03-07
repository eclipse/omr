/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#if !defined(MIXEDOBJECTMODEL_HPP_)
#define MIXEDOBJECTMODEL_HPP_

class MM_GCExtensionsBase;

/**
 * Provides information for mixed objects.
 * @ingroup GC_Base
 */
class GC_MixedObjectModel
{
	/*
	 * Function members
	 */
private:
protected:
public:
	bool
	initialize(MM_GCExtensionsBase *extensions)
	{
		return true;
	}

	void tearDown(MM_GCExtensionsBase *extensions) {}
};

#endif /* MIXEDOBJECTMODEL_HPP_ */
