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
 ******************************************************************************/

#ifndef CONFIGURATIONLANGUAGEINTERFACE_HPP_
#define CONFIGURATIONLANGUAGEINTERFACE_HPP_

#include "BaseVirtual.hpp"
#include "omrcomp.h"

class MM_ArrayAllocationModel;
class MM_EnvironmentBase;
class MM_EnvironmentLanguageInterface;
class MM_GlobalCollector;
class MM_MixedObjectAllocationModel;
class MM_ObjectAllocationInterface;

/**
 * Class representing a configuration language interface. This defines the API between the OMR
 * functionality and the language being implemented.
 */
class MM_ConfigurationLanguageInterface : public MM_BaseVirtual {
private:
protected:
	MM_ConfigurationLanguageInterface()
		: MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	}

public:

	virtual void kill(MM_EnvironmentBase *env) = 0;

	virtual bool initializeEnvironment(MM_EnvironmentBase* env) = 0;

	virtual MM_ObjectAllocationInterface* createObjectAllocationInterface(MM_EnvironmentBase* env) = 0;

	virtual MM_EnvironmentLanguageInterface *createEnvironmentLanguageInterface(MM_EnvironmentBase *env) = 0;

	virtual bool initializeArrayletLeafSize(MM_EnvironmentBase* env) = 0;
	virtual void initializeWriteBarrierType(MM_EnvironmentBase* env, uintptr_t configWriteBarrierType) = 0;
	virtual void initializeAllocationType(MM_EnvironmentBase* env, uintptr_t configGcAllocationType) = 0;
	virtual bool initializeSizeClasses(MM_EnvironmentBase* env) { return true; }
	virtual uintptr_t internalGetWriteBarrierType(MM_EnvironmentBase* env) = 0;
	virtual uintptr_t internalGetAllocationType(MM_EnvironmentBase* env) = 0;

	virtual MM_GlobalCollector* createGlobalCollector(MM_EnvironmentBase* env) = 0;

	virtual uintptr_t getEnvPoolNumElements(void) { return 0; }
	virtual uintptr_t getEnvPoolFlags(void) { return 0; }

#define MAXIMUM_DEFAULT_NUMBER_OF_GC_THREADS 64

	virtual uintptr_t getMaxGCThreadCount(void) { return MAXIMUM_DEFAULT_NUMBER_OF_GC_THREADS; }

};

#endif /* CONFIGURATIONLANGUAGEINTERFACE_HPP_ */
