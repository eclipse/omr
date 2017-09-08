/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
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

#ifndef omrceetbck_h
#define omrceetbck_h

#include <leawi.h>
#include <setjmp.h>
#include <ceeedcct.h>
#include "edcwccwi.h"
#include "omrport.h"

#pragma linkage(ceetbck, OS_UPSTACK)
#pragma map(ceetbck, "CEETBCK")

#if	!defined(J9ZOS39064)
void ceetbck(
		_POINTER* dsaptr,							/* in */
		_INT4* dsa_format,							/* inout */
		_POINTER* caaptr,							/* in */
		_INT4* member_id,							/* out */
		char* program_unit_name,					/* out */
		_INT4* program_unit_name_length,			/* inout */
		_INT4* program_unit_address,				/* out */
		_INT4* call_instruction_address,			/* inout */
		char* entry_name,							/* out */
		_INT4* entry_name_length,					/* inout */
		_INT4* entry_address,						/* out */
		_INT4* callers_call_instruction_address,	/* out */
		_POINTER* callers_dsaptr,					/* out */
		_INT4* callers_dsa_format,					/* out */
		char* statement_id,							/* out */
		_INT4* statement_id_length,					/* inout */
		_POINTER* cibptr,							/* out */
		_INT4* main_program,						/* out */
		struct _FEEDBACK* fc);						/* out */
#endif

#endif /* omrceetbck_h */
