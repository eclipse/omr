/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#if !defined(MM_OPTIONSMANAGER_HPP_)
#define MM_OPTIONSMANAGER_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "omrcomp.h"

#include "Base.hpp"

class MM_EnvironmentBase;
class MM_GCExtensionsBase;
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
	virtual MM_Configuration * createConfiguration(MM_EnvironmentBase *env);
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
