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

#ifndef CONFIGURATIONLANGUAGEINTERFACEIMPL_HPP_
#define CONFIGURATIONLANGUAGEINTERFACEIMPL_HPP_

#include "omr.h"

#include "ConfigurationLanguageInterface.hpp"
#include "EnvironmentBase.hpp"

/**
 * Class representing a collector language interface.  This implements the API between the OMR
 * functionality and the language being implemented.
 * @ingroup GC_Base
 */
class MM_ConfigurationLanguageInterfaceImpl : public MM_ConfigurationLanguageInterface {
private:
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	MM_ConfigurationLanguageInterfaceImpl()
		: MM_ConfigurationLanguageInterface()
	{
		_typeId = __FUNCTION__;
	}

public:
	static MM_ConfigurationLanguageInterfaceImpl *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	virtual bool initializeEnvironment(MM_EnvironmentBase* env);

	virtual MM_ObjectAllocationInterface* createObjectAllocationInterface(MM_EnvironmentBase* env);

	virtual MM_EnvironmentLanguageInterface *createEnvironmentLanguageInterface(MM_EnvironmentBase *env);

	virtual bool initializeArrayletLeafSize(MM_EnvironmentBase* env);
	virtual void initializeWriteBarrierType(MM_EnvironmentBase* env, uintptr_t configWriteBarrierType);
	virtual void initializeAllocationType(MM_EnvironmentBase* env, uintptr_t configGcAllocationType);

	virtual uintptr_t internalGetWriteBarrierType(MM_EnvironmentBase* env);
	virtual uintptr_t internalGetAllocationType(MM_EnvironmentBase* env);

	virtual MM_GlobalCollector* createGlobalCollector(MM_EnvironmentBase* env);
	void initializeSaltData(MM_EnvironmentBase* env, uintptr_t index, uint32_t startValue);
	
	virtual uintptr_t getMaxGCThreadCount(void) { return 1; }
	
};

#endif /* CONFIGURATIONLANGUAGEINTERFACEIMPL_HPP_ */
