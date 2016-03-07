/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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
