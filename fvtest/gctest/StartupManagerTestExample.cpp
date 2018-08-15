/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
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
					bool isGencon = (0 == j9_cmdla_stricmp(attr.value(), "gencon"));
					bool isEvacuator = (0 == j9_cmdla_stricmp(attr.value(), "evacuator"));
#if defined(OMR_GC_MODRON_SCAVENGER)
					extensions->scavengerEnabled = isGencon | isEvacuator;
					extensions->evacuatorEnabled = isEvacuator;
					extensions->scavengerAlignHotFields &= !isEvacuator;
#else
					if (isGencon || isEvacuator) {
						gcTestEnv->log(LEVEL_ERROR, "WARNING: GCPolicy ignored, gencon and evacuator policies require OMR_GC_MODRON_SCAVENGER (see configure_common.mk)\n");
					}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
					if (!isGencon && !isEvacuator && (0 != j9_cmdla_stricmp(attr.value(), "optavgpause"))) {
						gcTestEnv->log(LEVEL_ERROR, "Failed: Unrecognized GC policy (expected gencon or evacuator or optavgpause): %s\n", attr.value());
						result = false;
					}
				} else if (0 == strcmp(attr.name(), "concurrentMark")) {
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
					extensions->concurrentMark = (0 == j9_cmdla_stricmp(attr.value(), "true"));
#else
					gcTestEnv->log(LEVEL_ERROR, "WARNING: concurrentMark=true ignored, requires OMR_GC_MODRON_CONCURRENT_MARK (see configure_common.mk)\n");
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK)*/
#if defined(OMR_GC_MODRON_SCAVENGER)
				} else if (0 == strcmp(attr.name(), "forceBackOut")) {
					extensions->fvtest_forceScavengerBackout = (0 == j9_cmdla_stricmp(attr.value(), "true"));
				} else if (0 == strcmp(attr.name(), "forcePoisonEvacuate")) {
					extensions->fvtest_forcePoisonEvacuate = (0 == j9_cmdla_stricmp(attr.value(), "true"));
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
				} else if ((0 == strcmp(attr.name(), "verboseLog")) || (0 == strcmp(attr.name(), "numOfFiles")) || (0 == strcmp(attr.name(), "numOfCycles")) || (0 == strcmp(attr.name(), "sizeUnit"))) {
				} else {
					gcTestEnv->log(LEVEL_ERROR, "Failed: Unrecognized option: %s\n", attr.name());
					result = false;
				}
			}
#if defined(OMR_GC_MODRON_SCAVENGER)
			extensions->fvtest_forceScavengerBackout &= extensions->scavengerEnabled;
			extensions->fvtest_forcePoisonEvacuate &= extensions->scavengerEnabled;
#endif /* OMR_GC_MODRON_SCAVENGER */
		}
	}
	return result;
}
