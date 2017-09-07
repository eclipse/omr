/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
