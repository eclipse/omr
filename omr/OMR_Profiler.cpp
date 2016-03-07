/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014
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

#include "omrprofiler.h"
#include "ut_omrti.h"

void
omr_ras_sampleStackTraceStart(OMR_VMThread *omrVMThread, const void *methodKey)
{
	Trc_OMRPROF_MethodSampleStart(omrVMThread, methodKey);
}

void
omr_ras_sampleStackTraceContinue(OMR_VMThread *omrVMThread, const void *methodKey)
{
	Trc_OMRPROF_MethodSampleContinue(omrVMThread, methodKey);
}

BOOLEAN
omr_ras_sampleStackEnabled(void)
{
	return (TrcEnabled_Trc_OMRPROF_MethodSampleStart || TrcEnabled_Trc_OMRPROF_MethodSampleContinue);
}
