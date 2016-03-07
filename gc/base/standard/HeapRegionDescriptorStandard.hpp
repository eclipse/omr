/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

#if !defined(HEAPREGIONDESCRIPTORSTANDARD_HPP)
#define HEAPREGIONDESCRIPTORSTANDARD_HPP

#include "omrcfg.h"

#include "EnvironmentStandard.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionDescriptorTypes.hpp"

class MM_EnvironmentBase;
class MM_GCExtensionsBase;

class MM_HeapRegionDescriptorStandard : public MM_HeapRegionDescriptor {

public:
	MM_HeapRegionDescriptorStandardExtension _heapRegionDescriptionExtension;
protected:
private:

public:
	MM_HeapRegionDescriptorStandard(MM_EnvironmentStandard *env, void *lowAddress, void *highAddress);
	
	bool initialize(MM_EnvironmentBase *envBase, MM_HeapRegionManager *regionManager);
	void tearDown(MM_EnvironmentBase *env);
	
	static bool initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress);
	static void destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor);

protected:
private:
	
};

#endif /* HEAPREGIONDESCRIPTORSTANDARD_HPP */
