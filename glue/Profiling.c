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

#include "omr.h"
#include "omrprofiler.h"

void ex_omr_checkSampleStack(OMR_VMThread *omrVMThread, const void *context);
void ex_omr_insertMethodEntryInMethodDictionary(OMR_VM *omrVM, const void *method);

/**
 * Get the number of method properties
 * @return Number of method properties
 */
int
OMR_Glue_GetMethodDictionaryPropertyNum(void)
{
#error implement number of method properties
}

/**
 * Get the method property names
 * @return Method property names
 */
const char * const *
OMR_Glue_GetMethodDictionaryPropertyNames(void)
{
#error implement method property names
}

void
ex_omr_checkSampleStack(OMR_VMThread *omrVMThread, const void *context)
{
	/* Please see example/glue/Profiling.c for a sample implementation. */
}

void
ex_omr_insertMethodEntryInMethodDictionary(OMR_VM *omrVM, const void *method)
{
	/* Please see example/glue/Profiling.c for a sample implementation. */
}

