/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#ifndef MM_OMRGCSTARTUPAPI_HPP_
#define MM_OMRGCSTARTUPAPI_HPP_

#include "omr.h"
#include "objectdescription.h"
#include "omrcomp.h"
#include "j9nongenerated.h"

class MM_StartupManager;

/* Initialization & Shutdown API */

omr_error_t OMR_GC_InitializeHeap(OMR_VM* omrVM, MM_StartupManager *manager);

omr_error_t OMR_GC_IntializeHeapAndCollector(OMR_VM* omrVM, MM_StartupManager *manager);

omr_error_t OMR_GC_InitializeDispatcherThreads(OMR_VMThread* omrVMThread);

omr_error_t OMR_GC_ShutdownDispatcherThreads(OMR_VMThread* omrVMThread);

omr_error_t OMR_GC_InitializeCollector(OMR_VMThread* omrVMThread);

omr_error_t OMR_GC_ShutdownCollector(OMR_VMThread* omrVMThread);

omr_error_t OMR_GC_ShutdownHeap(OMR_VM* omrVM);

#endif /* MM_OMRGCSTARTUPAPI_HPP_ */
