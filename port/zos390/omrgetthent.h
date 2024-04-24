/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(OMRGETTHENT_H_)
#define OMRGETTHENT_H_

#include "omrutil.h"

#define PGTHA_ACCESS_CURRENT 1
#define PGTHA_FLAG_PROCESS_DATA 0x80

#pragma pack(push, 1)

typedef struct pgtha {
	/* Start of PGTHACONTINUE group. */
	unsigned int pid;
	unsigned char thid[8];
	/* FIRST, CURRENT or NEXT. */
	unsigned char accesspid;
	/* FIRST, CURRENT, NEXT or LAST. */
	unsigned char accessthid;
	/* End of PGTHACONTINUE group. */
	unsigned short asid;
	unsigned char loginname[8];
	/* Only THREAD and PTAG checked if ACCESSPID==CURRENT & ACCESSTHID==NEXT. */
	unsigned char flag1;
	char padding;
} pgtha;

/*
PGTHB                 DSECT ,        O U T P U T - - - - - - - - - -
PGTHBID               DS    CL4      "gthb"
PGTHBCONTINUE         DS    0CL14    NEXT VALUE FOR PGTHACONTINUE
PGTHBPID              DS    F        PROCESS ID
PGTHBTHID             DS    CL8      THREAD ID
PGTHBACCESSPID        DS    FL1      CURRENT/FIRST/NEXT
PGTHBACCESSTHID       DS    FL1      CURRENT/FIRST/NEXT/LAST
                      DS    FL2
PGTHBLENUSED          DS    F        LENGTH OF OUTPUT BUFFER USED
PGTHBLIMITC           DS    CL1      N, A
PGTHBOFFC             DS    FL3      OFFSET OF PROCESS AREA
PGTHBLIMITD           DS    CL1      N, A, X
PGTHBOFFD             DS    FL3      OFFSET OF CONTTY  AREA
PGTHBLIMITE           DS    CL1      N, A, X
PGTHBOFFE             DS    FL3      OFFSET OF PATH    AREA
PGTHBLIMITF           DS    CL1      N, A, X
PGTHBOFFF             DS    FL3      OFFSET OF COMMAND AREA
PGTHBLIMITG           DS    CL1      N, A, X
PGTHBOFFG             DS    FL3      OFFSET OF FILE DATA AREA
PGTHBLIMITJ           DS    CL1      N, A, V, X
PGTHBOFFJ             DS    FL3      OFFSET OF THREAD AREA
PGTHB#LEN             EQU   *-PGTHB
*/
typedef struct pgthb {
	/* "gthb" eyecatcher. */
	char id[4];
	/* Start of PGTHBCONTINUE. */
	int pid;
	char thid[8];
	char accesspid;
	char accessthid;
	/* End of PGTHBCONTINUE. */
	char padding[2];
	int lenused;
	char limitc;
	char offc[3];
	char limitd;
	char offd[3];
	char limite;
	char offe[3];
	char limitf;
	char offf[3];
	char limitg;
	char offg[3];
	char limitj;
	char offj[3];
} pgthb;

typedef struct pgthc {
	/* "gthc" eyecatcher. */
	char id[4];
	char flag1;
	char flag2;
	char flag3;
	char padding;
	int pid;
	int ppid;
	int pgpid;
	int sid;
	int fgpid;
	int euid;
	int ruid;
	int suid;
	int egid;
	int rgid;
	int sgid;
	int tsize;
	int syscallcount;
	int usertime;
	int systime;
	int starttime;
	short cntoe;
	short cntptcreated;
	short cntthreads;
	short asid;
	char jobname[8];
	char loginname[8];
	union {
		int value;
		struct {
			char value[3];
			char demonination;
		} tuple;
	} memlimit;
	union {
		int value;
		struct {
			char value[3];
			char demonination;
		} tuple;
	} memusage;
} pgthc;

typedef struct pgthj {
	/* "gthj" eyecatcher. */
	char id[4];
	char limitj;
	char offj[3];
	char limitk;
	char offk[3];
	char thid[8];
	char syscall[4];
	struct tcb *tcb;
	int ttime;
	int wtime;
	int padding;
	/* if status2==D. */
	short semnum;
	/* if status2==D. */
	short semval;
	int latchwaitpid;
	char penmask[8];
	char loginname[8];
	 /* Last 5 syscalls. */
	char prevsc[5][4];
	/* Start of PGTHJSTATUSCHARS group. */
	char status1;
	char status2;
	char status3;
	char status4;
	char status5;
	/* End of PGTHJSTATUSCHARS group. */
} pgthj;

typedef struct ProcessData {
	struct pgthb pgthb;
	struct pgthj pgthj;
	char padding[256];
} ProcessData;

#pragma pack(pop)

#endif /* !defined(OMRGETTHENT_H_) */
