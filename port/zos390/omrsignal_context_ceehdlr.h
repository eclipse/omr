/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef omrsignal_context_ceehdlr_h
#define omrsignal_context_ceehdlr_h

#include "omrport.h"
#include "omr__le_api.h"
#include "leconditionhandler.h"

#define MAX_NAME 256

typedef struct {
	_FEEDBACK *fc;
	_CEECIB *cib;
	uint16_t messageNumber;
	char *facilityID;
#if !defined(J9ZOS39064)
	char program_unit_name[MAX_NAME];
	_INT4 program_unit_address;
	char entry_name[MAX_NAME];
	_INT4 entry_address;
#endif
	uint32_t portLibrarySignalType;
	void *handlerAddress;
	void *handlerAddress2;
} J9LEConditionInfo;

uint32_t infoForFPR_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForGPR_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForVR_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForControl_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForModule_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value);

#endif /* omrsignal_context_ceehdlr_h */
