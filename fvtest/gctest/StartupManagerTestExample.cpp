/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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


#include "StartupManagerTestExample.hpp"
#include "GCExtensionsBase.hpp"
#include <string.h>
#include "pugixml.hpp"

bool
MM_StartupManagerTestExample::parseLanguageOptions(MM_GCExtensionsBase *extensions)
{
	bool result = true;
	pugi::xml_document doc;
	pugi::xml_parse_result parseResult = doc.load_file(_configFile);
	if (!parseResult) {
		gcTestEnv->log(LEVEL_ERROR, "Failed to load test configuration file (%s) with error description: %s.\n", _configFile, parseResult.description());
		result = false;
	} else {
		/* parse options */
		pugi::xpath_node option = doc.select_node("/gc-config/option");

		int32_t unitSize = 1;
		const char *unit = option.node().attribute("sizeUnit").value();
		if (0 != strcmp(unit, "")) {
			if (0 == j9_cmdla_stricmp(unit, "B")) {
				unitSize = 1;
			} else if (0 == j9_cmdla_stricmp(unit, "KB")) {
				unitSize = 1024;
			} else if (0 == j9_cmdla_stricmp(unit, "MB")) {
				unitSize = 1024 * 1024;
			} else if (0 == j9_cmdla_stricmp(unit, "GB")) {
				unitSize = 1024 * 1024 * 1024;
			} else {
				result = false;
				gcTestEnv->log(LEVEL_ERROR, "Unrecognized size unit: %s.\n", unit);
			}
		}

		if (result) {
			for (pugi::xml_attribute attr = option.node().first_attribute(); attr; attr = attr.next_attribute()) {
				if (0 == strcmp(attr.name(), "memoryMax")) {
					extensions->memoryMax = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "initialMemorySize")) {
					extensions->initialMemorySize = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "minNewSpaceSize")) {
					extensions->minNewSpaceSize = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "newSpaceSize")) {
					extensions->newSpaceSize = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "maxNewSpaceSize")) {
					extensions->maxNewSpaceSize = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "minOldSpaceSize")) {
					extensions->minOldSpaceSize = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "oldSpaceSize")) {
					extensions->oldSpaceSize = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "maxOldSpaceSize")) {
					extensions->maxOldSpaceSize = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "allocationIncrement")) {
					extensions->allocationIncrement = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "fixedAllocationIncrement")) {
					extensions->fixedAllocationIncrement = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "lowMinimum")) {
					extensions->lowMinimum = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "allowMergedSpaces")) {
					extensions->allowMergedSpaces = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "maxSizeDefaultMemorySpace")) {
					extensions->maxSizeDefaultMemorySpace = atoi(attr.value()) * unitSize;
				} else if (0 == strcmp(attr.name(), "gcthreadCount")) {
					/* TODO: support multi-thread GC*/
				} else if (0 == strcmp(attr.name(), "GCPolicy")) {
#if defined(OMR_GC_MODRON_SCAVENGER)
					extensions->scavengerEnabled = (0 == j9_cmdla_stricmp(attr.value(), "gencon"));
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
					extensions->concurrentMark = extensions->scavengerEnabled || (0 == j9_cmdla_stricmp(attr.value(), "optavgpause"));
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK)*/
				} else if (0 == strcmp(attr.name(), "forceBackOut")) {
					extensions->fvtest_forceScavengerBackout = (0 == j9_cmdla_stricmp(attr.value(), "true"));
				} else if (0 == strcmp(attr.name(), "forcePoisonEvacuate")) {
					extensions->fvtest_forcePoisonEvacuate = (0 == j9_cmdla_stricmp(attr.value(), "true"));
				} else if ((0 == strcmp(attr.name(), "verboseLog")) || (0 == strcmp(attr.name(), "numOfFiles")) || (0 == strcmp(attr.name(), "numOfCycles")) || (0 == strcmp(attr.name(), "sizeUnit"))) {
				} else {
					result = false;
					gcTestEnv->log(LEVEL_ERROR, "Failed: Unrecognized option: %s\n", attr.name());
					break;
				}
			}
			extensions->fvtest_forceScavengerBackout &= extensions->scavengerEnabled;
			extensions->fvtest_forcePoisonEvacuate &= extensions->scavengerEnabled;
		}
	}
	return result;
}
