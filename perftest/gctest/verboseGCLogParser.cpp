/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

#include <string.h>
#include <algorithm>
#include <vector>
#include <iterator>
#include <numeric>
#include <stdio.h>

#include "pugixml.hpp"

#include "omr.h"
#include "omrport.h"
#include "omrthread.h"

const char* XPATH_GET_ALL_MARK_TIME = "/verbosegc/gc-op[@type='mark']";
const char* XPATH_GET_ALL_SWEEP_TIME = "/verbosegc/gc-op[@type='sweep']";
const char* XPATH_GET_ALL_EXPAND_TIME = "/verbosegc/heap-resize[@type='expand']";
const char* XPATH_GET_TOTAL_GC_TIME = "/verbosegc/gc-end[@type='global']";
const char* SRC_DIR = "./";
const char* VERBOSE_GC_FILE_PREFIX = "VerboseGC";

double getAvg(std::vector<double> v);
void analyze(char* fileName, OMRPortLibrary portLibrary);

int main(void)
{
	int32_t totalFiles = 0;
	intptr_t rc = 0;
	char resultBuffer[128];
	uintptr_t rcFile;
	uintptr_t handle;
	OMRPortLibrary portLibrary;

	rc = omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT);
	if (0 != rc) {
		fprintf(stderr, "omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT) failed, rc=%d\n", (int)rc);
		return -1;
	}

	rc = omrport_init_library(&portLibrary, sizeof(OMRPortLibrary));
	if (0 != rc) {
		fprintf(stderr, "omrport_init_library(&portLibrary, sizeof(OMRPortLibrary)), rc=%d\n", (int)rc);
		return -1;
	}

	OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);

	rcFile = handle = omrfile_findfirst(SRC_DIR, resultBuffer);

	if(rcFile == (uintptr_t)-1) {
		fprintf(stderr, "omrfile_findfirst(SRC_DIR, resultBuffer), return code=%d\n", (int)rcFile);
		return -1;
	}

	while ((uintptr_t)-1 != rcFile) {
		if (strncmp(resultBuffer, VERBOSE_GC_FILE_PREFIX, strlen(VERBOSE_GC_FILE_PREFIX)) == 0) {
			analyze(resultBuffer, portLibrary);
			totalFiles++;
			/* Clean up verbose log file */
			omrfile_unlink(resultBuffer);
		}
		rcFile = omrfile_findnext(handle, resultBuffer);
	}
	if (handle != (uintptr_t)-1) {
		omrfile_findclose(handle);
	}

	if(totalFiles < 1) {
		omrtty_printf("Failed to find any verbose GC file to process!\n\n");
	}

	portLibrary.port_shutdown_library(&portLibrary);
	omrthread_detach(NULL);
}

double
getAvg(std::vector<double> v)
{
	double sum = std::accumulate(v.begin(), v.end(), 0.0);
	double avg = sum / v.size();
	return avg;
}

void
analyze(char* fileName, OMRPortLibrary portLibrary)
{
	std::vector<double> mark_values;
	std::vector<double> sweep_values;
	std::vector<double> expand_values;
	std::vector<double> gcduration_values;

	pugi::xpath_node_set markTimes;
	pugi::xpath_node_set sweepTimes;
	pugi::xpath_node_set expandTimes;
	pugi::xpath_node_set gcTimes;

	double maxMark = 0;
	double minMark = 0;
	double avgMark = 0;

	double maxSweep = 0;
	double minSweep = 0;
	double avgSweep = 0;

	double maxExpand = 0;
	double minExpand = 0;
	double avgExpand = 0;

	double maxGCDuration = 0;
	double minGCDuration = 0;
	double avgGCDuration = 0;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(fileName);

	OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);
	if(!result) {
		omrtty_printf("Error loading file : %s\n", fileName);
		return;
	} else {
		omrtty_printf("\nResults for : %s\n",fileName);
	}

	markTimes = doc.select_nodes(XPATH_GET_ALL_MARK_TIME);
	for (pugi::xpath_node_set::const_iterator it = markTimes.begin(); it != markTimes.end(); ++it) {
	    pugi::xpath_node node = *it;
	    double value = node.node().attribute("timems").as_double();
	    mark_values.push_back(value);
	}

	sweepTimes = doc.select_nodes(XPATH_GET_ALL_SWEEP_TIME);
	for (pugi::xpath_node_set::const_iterator it = sweepTimes.begin(); it != sweepTimes.end(); ++it) {
	    pugi::xpath_node node = *it;
	    double value = node.node().attribute("timems").as_double();
	    sweep_values.push_back(value);
	}

	expandTimes = doc.select_nodes(XPATH_GET_ALL_EXPAND_TIME);
	for (pugi::xpath_node_set::const_iterator it = expandTimes.begin(); it != expandTimes.end(); ++it) {
	    pugi::xpath_node node = *it;
	    double value = node.node().attribute("timems").as_double();
	    expand_values.push_back(value);
	}

	gcTimes = doc.select_nodes(XPATH_GET_TOTAL_GC_TIME);
	for (pugi::xpath_node_set::const_iterator it = gcTimes.begin(); it != gcTimes.end(); ++it) {
	    pugi::xpath_node node = *it;
	    double value = node.node().attribute("durationms").as_double();
	    gcduration_values.push_back(value);
	}

	if (!mark_values.empty()) {
		maxMark = *std::max_element(mark_values.begin(), mark_values.end());
		minMark = *std::min_element(mark_values.begin(), mark_values.end());
		avgMark = getAvg(mark_values);
	}

	if (!sweep_values.empty()) {
		maxSweep = *std::max_element(sweep_values.begin(), sweep_values.end());
		minSweep = *std::min_element(sweep_values.begin(), sweep_values.end());
		avgSweep = getAvg(sweep_values);
	}

	if (!expand_values.empty()) {
		maxExpand = *std::max_element(expand_values.begin(), expand_values.end());
		minExpand = *std::min_element(expand_values.begin(), expand_values.end());
		avgExpand = getAvg(expand_values);
	}

	if (!gcduration_values.empty()) {
		maxGCDuration = *std::max_element(gcduration_values.begin(), gcduration_values.end());
		minGCDuration = *std::min_element(gcduration_values.begin(), gcduration_values.end());
		avgGCDuration = getAvg(gcduration_values);
	}

	omrtty_printf("            Mark           Sweep          Expand        GCDuration\n");
	omrtty_printf("-------------------------------------------------------------------\n");
	omrtty_printf("Max     : %f        %f        %f        %f\n",
						maxMark, maxSweep, maxExpand, maxGCDuration);

	omrtty_printf("Min     : %f        %f        %f        %f\n",
								minMark, minSweep, minExpand, minGCDuration);

	omrtty_printf("Average : %f        %f        %f        %f\n\n",
								avgMark, avgSweep, avgExpand, avgGCDuration);
}
