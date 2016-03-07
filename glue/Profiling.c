/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016
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

