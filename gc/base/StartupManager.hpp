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

#if defined(OMR_GC)
#include "GCExtensionsBase.hpp"
#include "ConfigurationFlat.hpp"
#endif /* OMR_GC */


#define OMR_GC_BUFFER_SIZE 256
#define OMR_XMS "-Xms"
#define OMR_XMS_LENGTH 4
#define OMR_XMX "-Xmx"
#define OMR_XMX_LENGTH 4
#if defined(OMR_GC_MODRON_COMPACTION)
#define OMR_XCOMPACTGC "-Xcompactgc"
#define OMR_XCOMPACTGC_LENGTH 11
#endif /* OMR_GC_MODRON_COMPACTION */
#if defined(OMR_GC_MODRON_SCAVENGER)
#define OMR_XGCPOLICY "-Xgcpolicy:"
#define OMR_XGCPOLICY_LENGTH 11
#define OMR_GCPOLICY_GENCON "gencon"
#define OMR_GCPOLICY_GENCON_LENGTH 6
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#define OMR_XVERBOSEGCLOG "-Xverbosegclog:"
#define OMR_XVERBOSEGCLOG_LENGTH 15
#define OMR_XGCBUFFERED_LOGGING "-Xgc:bufferedLogging"
#define OMR_XGCBUFFERED_LOGGING_LENGTH 20
#define OMR_XGCTHREADS "-Xgcthreads"
#define OMR_XGCTHREADS_LENGTH 11

class MM_EnvironmentBase;
//class MM_GCExtensionsBase;
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
	//virtual bool handleOption(MM_GCExtensionsBase *extensions, char *option);
	virtual bool
	handleOption(MM_GCExtensionsBase *extensions, char *option)
	{
		OMRPORT_ACCESS_FROM_OMRVM(extensions->getOmrVM());
		bool result = true;
		if (0 == strncmp(option, OMR_XMS, OMR_XMS_LENGTH)) {
			uintptr_t value = 0;
			if (!getUDATAMemoryValue(option + OMR_XMS_LENGTH, &value)) {
				result = false;
			} else {
				extensions->initialMemorySize = value;
				extensions->minOldSpaceSize = value;
				extensions->oldSpaceSize = value;
			}
		} else if (0 == strncmp(option, OMR_XMX, OMR_XMX_LENGTH)) {
			uintptr_t value = 0;
			if (!getUDATAMemoryValue(option + OMR_XMX_LENGTH, &value)) {
				result = false;
			} else {
				extensions->maxOldSpaceSize = value;
				extensions->memoryMax = value;
				extensions->maxSizeDefaultMemorySpace = value;
			}
		}
	#if defined(OMR_GC_MODRON_COMPACTION)
		else if (0 == strncmp(option, OMR_XCOMPACTGC, OMR_XCOMPACTGC_LENGTH)) {
			extensions->noCompactOnGlobalGC = 0;
			extensions->compactOnGlobalGC = 0;
			extensions->nocompactOnSystemGC = 0;
			extensions->compactOnSystemGC = 0;
		}
	#endif /* OMR_GC_MODRON_COMPACTION */
		else if (0 == strncmp(option, OMR_XVERBOSEGCLOG, OMR_XVERBOSEGCLOG_LENGTH)) {
			verboseFileName = (char *) omrmem_allocate_memory(strlen(option+OMR_XVERBOSEGCLOG_LENGTH)+1, OMRMEM_CATEGORY_MM);
			if (NULL == verboseFileName) {
				result = false;
			} else {
				strcpy(verboseFileName, option + OMR_XVERBOSEGCLOG_LENGTH);
			}
		}
		else if (0 == strncmp(option, OMR_XGCBUFFERED_LOGGING, OMR_XGCBUFFERED_LOGGING_LENGTH)) {
			extensions->bufferedLogging = true;
		}
	#if defined(OMR_GC_MORDON_SCAVENGER)
		else if (0 == strncmp(option, OMR_XGCPOLICY, OMR_XGCPOLICY_LENGTH)) {
			char *gcpolicy = option + OMR_XGCPOLICY_LENGTH;
			if (0 == strncmp(gcpolicy, OMR_GCPOLICY_GENCON, OMR_GCPOLICY_GENCON_LENGTH)) {
				/* this is disabled by default -- enable scavenger here */
				extensions->scavengerEnabled = true;
			} else {
				result = false;
			}
		}
	#endif /* defined(OMR_GC_MORDON_SCAVENGER) */
		else if (0 == strncmp(option, OMR_XGCTHREADS, OMR_XGCTHREADS_LENGTH)) {
			uintptr_t forcedThreadCount = 0;
			if (0 >= getUDATAValue(option + OMR_XGCTHREADS_LENGTH, &forcedThreadCount)) {
				result = false;
			} else {
				extensions->gcThreadCount = forcedThreadCount;
				extensions->gcThreadCountForced = true;
			}
		} else {
			/* unknown option */
			result = false;
		}

		return result;
	}
	virtual char * getOptions(void) { return NULL; }
	virtual bool parseLanguageOptions(MM_GCExtensionsBase *extensions) { return true; };
	bool parseGcOptions(MM_GCExtensionsBase *extensions);

public:
	virtual MM_ConfigurationLanguageInterface * createConfigurationLanguageInterface(MM_EnvironmentBase *env) = 0;
	//virtual MM_Configuration * createConfiguration(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface *cli);
virtual
	MM_Configuration *
	createConfiguration(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface *cli)
	{
		/* When multiple configurations are supported, this call can return different
		 * concrete implementations based on arguments previously parsed by handleOption().
		 */
		return MM_ConfigurationFlat::newInstance(env, cli);
	}
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
