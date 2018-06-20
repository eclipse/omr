/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

#include "omr.h"
#include "omrvm.h"

omr_error_t
OMR_Glue_BindCurrentThread(OMR_VM *omrVM, const char *threadName, OMR_VMThread **omrVMThread)
{
	omr_error_t rc = OMR_ERROR_NONE;

	OMR_VMThread *currentThread = omr_vmthread_getCurrent(omrVM);
	if (NULL == currentThread) { /* not already attached */
		omrthread_t self = NULL;
		if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
			void *languageThread = NULL;
			OMR_VMThread *newOMRThread = NULL;

			rc = OMR_Glue_AllocLanguageThread(omrVM->_language_vm, &languageThread);
			if (OMR_ERROR_NONE != rc) {
				goto done;
			}

			rc = OMR_Thread_FirstInit(omrVM, self, languageThread, &newOMRThread, threadName);
			if (OMR_ERROR_NONE != rc) {
				OMR_Glue_FreeLanguageThread(languageThread);
				goto done;
			}

			rc = OMR_Glue_LinkLanguageThreadToOMRThread(languageThread, newOMRThread);
			if (OMR_ERROR_NONE != rc) {
				OMR_Thread_LastFree(newOMRThread);
				OMR_Glue_FreeLanguageThread(languageThread);
				goto done;
			}
			/* success */
			*omrVMThread = newOMRThread;
done:
			if (OMR_ERROR_NONE != rc) {
				/* error cleanup */
				omrthread_detach(self);
			}
		} else {
			rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
		}

	} else { /* already attached */
		omr_vmthread_reattach(currentThread, threadName);
		*omrVMThread = currentThread;
	}
	return rc;
}

omr_error_t
OMR_Glue_UnbindCurrentThread(OMR_VMThread *omrVMThread)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL == omrVMThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else {
		omrthread_t self = omrVMThread->_os_thread;

		if (1 == omrVMThread->_attachCount) {
			/* cache vmThread members so that we can use them after the vmThread is destroyed */
			void *languageThread = omrVMThread->_language_vmthread;

			rc = OMR_Thread_LastFree(omrVMThread);
			if (OMR_ERROR_NONE != rc) {
				goto done;
			}
			rc = OMR_Glue_FreeLanguageThread(languageThread);
			if (OMR_ERROR_NONE != rc) {
				goto done;
			}
			omrthread_detach(self);
		} else {
			omr_vmthread_redetach(omrVMThread);
		}
	}
done:
	return rc;
}

omr_error_t
OMR_Glue_AllocLanguageThread(void *languageVM, void **languageThread)
{
	*languageThread = NULL;
	return OMR_ERROR_NONE;
}

omr_error_t
OMR_Glue_FreeLanguageThread(void *languageThread)
{
	return OMR_ERROR_NONE;
}

omr_error_t
OMR_Glue_LinkLanguageThreadToOMRThread(void *languageThread, OMR_VMThread *omrVMThread)
{
	return OMR_ERROR_NONE;
}
