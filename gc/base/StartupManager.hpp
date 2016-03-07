/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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


#if !defined(MM_OPTIONSMANAGER_HPP_)
#define MM_OPTIONSMANAGER_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "omrcomp.h"

#include "Base.hpp"

class MM_EnvironmentBase;
class MM_GCExtensionsBase;
class MM_ConfigurationLanguageInterface;
class MM_CollectorLanguageInterface;
class MM_Configuration;
class MM_VerboseManagerBase;

class MM_StartupManager
{
	/*
	 * Data members
	 */
private:
	char *verboseFileName;

protected:
	OMR_VM *omrVM;
	uintptr_t defaultMinHeapSize;
	uintptr_t defaultMaxHeapSize;

public:
	
	/*
	 * Function members
	 */

private:
	void tearDown(void);

protected:
	/* We want to be able to use this C++ class without a custom allocator
	 * or stdlibc++, so we force stack allocation to ensure cleanup occurs. */
	static void * operator new(size_t); /* Since we override delete, prevent heap allocation. */
	static void * operator new [] (size_t);
	void operator delete(void *p) {}; /* Needed to link without stdlibc++ */

	uintptr_t getUDATAValue(char *option, uintptr_t *outputValue);
	bool getUDATAMemoryValue(char *option, uintptr_t *convertedValue);
	virtual bool handleOption(MM_GCExtensionsBase *extensions, char *option);
	virtual char * getOptions(void) { return NULL; }
	virtual bool parseLanguageOptions(MM_GCExtensionsBase *extensions) { return true; };
	bool parseGcOptions(MM_GCExtensionsBase *extensions);

public:
	virtual MM_ConfigurationLanguageInterface * createConfigurationLanguageInterface(MM_EnvironmentBase *env) = 0;
	virtual MM_Configuration * createConfiguration(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface *cli);
	virtual MM_CollectorLanguageInterface * createCollectorLanguageInterface(MM_EnvironmentBase *env) = 0;
	virtual MM_VerboseManagerBase * createVerboseManager(MM_EnvironmentBase* env) { return NULL; }

	bool loadGcOptions(MM_GCExtensionsBase *extensions);

	bool isVerboseEnabled(void);
	char * getVerboseFileName(void);

	virtual ~MM_StartupManager() { tearDown(); }

	MM_StartupManager(OMR_VM *omrVM, uintptr_t defaultMinHeapSize, uintptr_t defaultMaxHeapSize)
		: verboseFileName(NULL)
		, omrVM(omrVM)
		, defaultMinHeapSize(defaultMinHeapSize)
		, defaultMaxHeapSize(defaultMaxHeapSize)
	{
	}
};

#endif /* MM_OPTIONSMANAGER_HPP_ */
