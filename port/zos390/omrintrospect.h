/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/**
 * @file
 * @ingroup Port
 * @brief process introspection support
 */
#ifndef J9INTROSPECT_INCLUDE
#define J9INTROSPECT_INCLUDE

#include <leawi.h>
#include "edcwccwi.h"
#import "omrgetthent.h"
#include "omrintrospect_common.h"
#include "omrutil.h"

#if defined(OMR_ENV_DATA64)
#pragma linkage(getthent, OS)
#pragma map(getthent, "BPX4GTH")
#else /* defined(OMR_ENV_DATA64) */
#pragma linkage(getthent, OS_UPSTACK)
#pragma map(getthent, "BPX1GTH")
#endif /* defined(OMR_ENV_DATA64) */

#if defined(OMR_ENV_DATA64)
#pragma map(pthread_quiesce, "BPX4PTQ")
#else /* defined(OMR_ENV_DATA64) */
#pragma linkage(pthread_quiesce, OS_UPSTACK)
#pragma map(pthread_quiesce, "BPX1PTQ")
#endif /* defined(OMR_ENV_DATA64) */

#if !defined(OMR_ENV_DATA64)
#pragma linkage(pthread_quiesce_and_get_np_X, OS_UPSTACK)
#pragma map(pthread_quiesce_and_get_np_X, "BPX1PQG")
#endif /* !defined(OMR_ENV_DATA64) */

#ifdef MAX_NAME
#undef MAX_NAME
#endif
#define MAX_NAME 257

#define clock_gettime(x, y) -1

typedef __mcontext_t_ thread_context;

#pragma pack(1)

/* Program routine entry area (XPLINK) */
typedef struct XPLINK_Routine_entry {
	uint8_t  eyecatcher[7];
	uint8_t  mark_type;
	uint32_t ppa1_offset;
	uint32_t dsa_size_and_entry_flags;
} XPLINK_Routine_entry;

/* Program Prologue area version 3 (XPLINK) */
typedef struct PPA1_Version3 {
	uint8_t  version;
	uint8_t  le_signature;
	uint16_t gpr_mask;
	int32_t ppa2_offset;
	uint8_t  flags1;
	uint8_t  flags2;
	uint8_t  flags3;
	uint8_t  flags4;
	uint16_t length_parms;
	uint8_t  length_prolog;
	uint8_t  alloca_reg_and_sp_offset;
	uint32_t length_code;
} PPA1_Version3;

/*   flags3
	 '0.......'B State Variable locator field is not in optional area.
	 '1.......'B State Variable locator field is in the optional area.
	 '.0......'B Argument Area Length is not in the optional area.
	 '.1......'B Argument Area Length is in the optional area.
	 '..0.....'B FP Register Mask is not in the optional area.
	 '..1.....'B FP Register Mask is in the optional area.
	 '...0....'B No ARs are saved. AR mask not in optional area.
	 '...1....'B ARs are saved. AR mask in optional area.
	 '....0...'B Member PPA1 word is not present in optional area.
	 '....1...'B Member PPA1 word is present in the optional area.
	 '.....0..'B Offset to BDI is not present in optional area.
	 '.....1..'B Offset to BDI is present in the optional area.
	 '......0.'B Interface mapping flags not in the optional area.
	 '......1.'B Interface mapping flags in the optional area.
	 '.......0'B Java Method Locator Table not in the optional area.
	 '.......1'B Java Method Locator Table in the optional area.
*/
#define STATE_VARIABLE_LOCATOR	0x80
#define ARGUMENT_AREA_LENGTH	0x40
#define FP_REGISTER				0x20
#define AR_SAVED				0x10
#define PPA1_WORD_FLAG			0x08
#define BDI_OFFSET				0x04
#define INTERFACE_MAPPING		0x02
#define JAVA_METHOD_LOCATOR		0x01

/*  flags4
	'0000000.'B Reserved for future optional fields (must all be zero).
	'.......0'B Name length and name are not in the optional area.
	'.......1'B Name length and name in the optional area.
*/

#define NAME_LENGTH_AND_NAME   0x01

/*
PGTHA                 DSECT ,        I N P U T - - - - - - - - - - -
PGTHACONTINUE         DS    0CL14
PGTHAPID              DS    F        PROCESS ID (IGNORED IF FIRST)
PGTHATHID             DS    CL8      THREAD ID (IGNORED IF FIRST/LAST)
PGTHAACCESSPID        DS    FL1      FIRST, CURRENT, NEXT
PGTH#NEXT             EQU   2        NEXT AFTER SPECIFIED
PGTH#CURRENT          EQU   1        AS SPECIFIED
PGTH#FIRST            EQU   0        FIRST (EQUIV NEXT WITH PID=0)
PGTH#LAST             EQU   3        only with PGTHIACCESSTHID
*
PGTHAACCESSTHID       DS    FL1      FIRST, CURRENT, NEXT, LAST
* ONLY FLAG1 BITS THREAD AND PTAG WILL BE CONSIDERED WHEN
* ACCESSPID=CURRENT AND ACCESSTHID=NEXT
*
* ASID AND LOGINNAME FILTERS APPLY ONLY WHEN ACCESSPID = FIRST, NEXT
PGTHAASID             DS    FL2      FILTER - ASID
* LOGINNAME COMPARISON WILL LOOK FOR UNIX ALIAS. IF PGHTALOGINNAME
* IS NOT AN ALIAS, IT WILL BE SHIFTED TO UPPER CASE AND CHECKED
* AGAINST MVS ID.
PGTHALOGINNAME        DS    CL8      FILTER - USERID ALIAS OR MVS
*
PGTHAFLAG1            DS    FL1      WHAT OUTPUT AREAS TO INCLUDE
PGTHAPROCESS          EQU   X'80'      PGTHC, PROCESS DATA
PGTHACONTTY           EQU   X'40'      PGTHD, CONTTY
PGTHAPATH             EQU   X'20'      PGTHE, PATH
PGTHACOMMAND          EQU   X'10'      PGTHF, CMD & ARGS
PGTHAFILEDATA         EQU   X'08'      PGTHG, FILE DATA
PGTHATHREAD           EQU   X'04'      PGTHJ, THREAD DATA
PGTHAPTAG             EQU   X'02'      PGTHK, PTAG (NEEDS PGTHJ)
                      DS    FL1
PGTHA#LEN             EQU   *-PGTHA
*/
#define PGTHA_ACCESS_FIRST			0
#define PGTHA_ACCESS_NEXT			2
#define PGTHA_ACCESS_LAST			3

#define PGTHA_FLAG_CONTTY			0x40
#define PGTHA_FLAG_PATH				0x20
#define PGTHA_FLAG_CMD_AND_ARGS		0x10
#define PGTHA_FLAG_FILE_DATA		0x8
#define PGTHA_FLAG_THREAD_DATA		0x4
#define PGTHA_FLAG_PTAG				0x2 /* needs thread data */

/*
* VALUES FOR PGTH.LIMIT. FIELDS
PGTH#NOTREQUESTED     EQU   C'N'     Associated PghtA.. bit off
PGTH#OK               EQU   C'A'     All data included
PGTH#STORAGE          EQU   C'S'     output buffer exhausted
* EXHAUSTED STORAGE < 1ST PGTHJ RESULTS IN -1 EINVAL JRBUFFTOOSMALL
PGTH#VAGUE            EQU   C'V'     Changed out from under us
PGTH#NOTCONNECTED     EQU   C'X'     Need data not connected
*/
#define PGTH_NOTREQUESTED	0xD5
#define PGTH_OK				0xC1
#define PGTH_STORAGE		0xE2
#define PGTH_VAGUE			0xE5
#define PGTH_NOTCONNECTED	0xE7

/*
* USING PGTHC,Rx where Rx = ADDRESS of PGTHB + PGTHBOFFC
PGTHC                 DSECT ,        P R O C E S S - - - - - - - - -
PGTHCID               DS    CL4      "gthc"
PGTHCFLAG1            DS    FL1
PGTHCMULPROCESS       EQU   X'80'    MULTIPLE PROCESSES
PGTHCSWAP             EQU   X'40'    TCBOUT
PGTHCTRACE            EQU   X'20'    THREAD IS BEING TRACED
PGTHCSTOPPED          EQU   X'10'    STOPPED
PGTHCINCOMPLETE       EQU   X'08'    NOT ALL BLOCKS PRESENT
PGTHCZOMBIE           EQU   X'04'    PROCESS IS A ZOMBIE
PGTHCBLOCKING         EQU   X'02'    Shutdown blocking
PGTHCPERM             EQU   X'01'    SHutdown permanent
PGTHCFLAG2            DS    FL1
PGTHCMEMLTYPE         EQU   X'80'    ON - MemLimit is a BinMult
PGTHCRESPAWN          EQU   X'40'    respawnable process
PGTHCFLAG3            DS    FL1
PGTHCMEMUTYPE         EQU   X'80'    ON - MemUsage is a BinMult
                      DS    1FL1
PGTHCPID              DS    F        PROCESS ID
PGTHCPPID             DS    F        PARENT ID
PGTHCPGPID            DS    F        PROCESS GROUP
PGTHCSID              DS    F        SESSION ID
PGTHCFGPID            DS    F        FOREGROUND PROCESS GROUP
PGTHCEUID             DS    F        EFFECTIVE USER ID
PGTHCRUID             DS    F        REAL USER ID
PGTHCSUID             DS    F        SAVED SET USER ID
PGTHCEGID             DS    F        EFFECTIVE GROUP ID
PGTHCRGID             DS    F        REAL GROUP ID
PGTHCSGID             DS    F        SAVED SET GROUP ID
PGTHCTSIZE            DS    F        TOTAL SIZE
PGTHCSYSCALLCOUNT     DS    F        COUNT OF SLOW-PATH SYSCALLS
PGTHCUSERTIME         DS    F        TIME SPENT IN USER CODE
PGTHCSYSTIME          DS    F        TIME SPENT IN SYSTEM CODE
PGTHCSTARTTIME        DS    F        TIME PROCESS WAS DUBBED
PGTHCCNTOE            DS    FL2      NO. OE THREADS
PGTHCCNTPTCREATED     DS    FL2      NO. PTHREAD CREATED THREADS
PGTHCCNTTHREADS       DS    FL2      COUNT OF ALL THREADS
PGTHCASID             DS    FL2      ADDRESS SPACE ID
PGTHCJOBNAME          DS    CL8      MVS JOB NAME
PGTHCLOGINNAME        DS    CL8      LOGIN NAME - ALIAS OR MVS
PGTHCMEMLIMIT         DS    1FL4     maximum Memlimit in bytes
        ORG    PGTHCMEMLIMIT
PGTHCMEMLIMITVAL      DS    1FL3     Hex value
PGTHCMEMLMULT         DS    1CL1     multiplier when PGTHCMEMLTYPE
PGTHCMEMUSAGE         DS    1FL4     bytes in use
        ORG    PGTHCMEMUSAGE
PGTHCMEMUSAGEVAL      DS    1FL3     Hex value
PGTHCMEMUMULT         DS    1CL1     multiplier when PGTHCMEMUTYPE
PGTHCX   DS    0C

*      **************************************************************
*      *
*      * PGTHCMEMLIMIT constants are used by PGTHCMEMLMULT and
*      * PGTHCMEMUMULT when the TYPE is a binmult.
*      *
*      * When PGTHCMEMLTYPE is on PGTHCMEMLIMIT consists or a
*      * 24bit binary value in the first three bytes followed by
*      * and ebcdic contanst that indicates the demonination.
*      *
*      * When PGTHCMEMLTYPE is off PGTHCMEMLIMIT consists or a
*      * 32bit binary value.
*      *
*      **************************************************************
*
*
PGTH#KILO EQU  C'K'      Kilobytes
PGTH#MEGA EQU  C'M'      Megabytes
PGTH#GIGA EQU  C'G'      Gigabytes
PGTH#TERA EQU  C'T'      Terabytes
PGTH#PETA EQU  C'P'      Petabytes
PGTHC#LEN             EQU   *-PGTHC
*
*/

#define PGTHC_FLAG1_MULTIPLE_PROCESSES		0x80
#define PGTHC_FLAG1_TCBOUT					0x40
#define PGTHC_FLAG1_THREAD_BEING_TRACED		0x20
#define PGTHC_FLAG1_STOPPED					0x10
#define PGTHC_FLAG1_NOT_ALL_BLOCKS_PRESENT	0x08
#define PGTHC_FLAG1_PROCESS_IS_ZOMBIE		0x04
#define PGTHC_FLAG1_SHUTDOWN_BLOCKING		0x02
#define PGTHC_FLAG1_SHUTDOWN_PERMENANT		0x01

#define PGTHC_FLAG2_MEM_LIMIT_TYPE			0x80 /* On - MemLimit is a BinMult */
#define PGTHC_FLAG2_RESPAWNABLE_PROCESS		0x40

#define PGTHC_FLAG3_MEM_USAGE_TYPE			0x80 /* On - MemUsage is a BinMult */

#define PGTHC_MEM_DENOMINATION_KILOBYTE 0xD2
#define PGTHC_MEM_DENOMINATION_MEGABYTE 0xD4
#define PGTHC_MEM_DENOMINATION_GIGABYTE 0xC7
#define PGTHC_MEM_DENOMINATION_TERABYTE 0xE3
#define PGTHC_MEM_DENOMINATION_PETABYTE 0xD7

/*
* USING PGTHD,Rx where Rx = ADDRESS of PGTHB + PGTHBOFFD
PGTHD                 DSECT ,        C O N T T Y - - - - - - - - - -
PGTHDID               DS    CL4      "gthd"
PGTHDLEN              DS    FL2      Length of ConTty
PGTHDCONTTY           DS    CL1024   1024 = max ConTty
*/
struct pgthd {
	char id[4]; /* "gthd" eyecatcher */
	short len;
	char contty[1024];
};

/*
* USING PGTHE,Rx where Rx = ADDRESS of PGTHB + PGTHBOFFE
PGTHE                 DSECT ,        P A T H - - - - - - - - - - - -
PGTHEID               DS    CL4      "gthe"
PGTHELEN              DS    FL2      Length of Path
PGTHEPATH             DS    CL1024   1024 = max path
*/
struct pgthe {
	char id[4]; /* "gthe" eyecatcher */
	short len;
	char path[1024];
};

/*
* USING PGTHF,Rx where Rx = ADDRESS of PGTHB + PGTHBOFFF
PGTHF                 DSECT ,        C O M M A N D - - - - - - - - -
PGTHFID               DS    CL4      "gthf"
PGTHFLEN              DS    FL2      Length of command and arguments
PGTHFCOMMAND          DS    CL1024   1024 = max command
*/
struct pgthf {
	char id[4]; /* "gthf" eyecatcher */
	short len;
	char command[1024];
};

/*
* USING PGTHG,Rx where Rx = ADDRESS of PGTHB + PGTHBOFFG
PGTHG                 DSECT ,        F I L E   H E A D E R - - - - -
PGTHGID               DS    CL4      "gthg"
PGTHGLIMITH           DS    CL1      N, A, S, X
PGTHGOFFH             DS    FL3      Offset of PgthH
PGTHGCOUNT            DS    F        Count of PgthH elements
PGTHGMAXVNODETOKENS   DS    F        MAX NUMBER VNODE TOKENS
PGTHGVNODETOKENCOUNT  DS    F        CURRENT NUMBER VNODE TOKENS
PGTHGSERVERFLAGS      DS    F        SABFLAGS
PGTHGSERVERNAME       DS    CL32     SABSERVERNAME SERVER=
PGTHGACTIVEFILES      DS    F        SABVDECOUNT   AF=
PGTHGMAXFILES         DS    F        SABMAXVDES    MF=
PGTHGSERVERTYPE       DS    F        SABSERVERTYPE TYPE=
PGTHG#LEN             EQU   *-PGTHG
*
PGTHGARRAY            DS    0C       first PGTHH
*/
struct pgthg {
	char id[4]; /* "gthg" eyecatcher */
	char limith;
	char offh[3];
	int count;
	int maxvnodetokens;
	int vnodetokencount;
	int serverflags;
	char servername[32];
	int activefiles;
	int servertype;
};

/*
* USING PGTHH,Rx where Rx = ADDRESS of PGTHB + PGHTGOFFH
* Increment   Rx by PGTHH#LEN until PGTHGCOUNT exhausted
PGTHH                 DSECT ,        F I L E   D A T A - - - - - - -
PGTHHID               DS    CL2
PGTHH#IDR             EQU   C'rd'    root directory    (first)
PGTHH#IDC             EQU   C'cd'    current directory (second)
PGTHH#IDF             EQU   C'fd'    file directory
PGTHH#IDV             EQU   C'vd'    vnode directory
PGTHHTYPE             DS    BL1      Mapped in BPXYFTYP see FT_DIR +
PGTHHOPEN             DS    BL1      Mapped in BPXYOPNF see O_FLAGS4
PGTHHINODE            DS    F        I-NODE        see stat()
PGTHHDEVNO            DS    F        DEVICE NUMBER see stat()
PGTHH#LEN             EQU   *-PGTHH
*/
struct pgthh {
	char id[2]; /* rd = root directory (fist), cd = current directory (second), fd = file directory, vd = vnode directory */
	char type;
	char open;
	int inode;
	int devno;
};

/*
* USING PGTHJ,Rx where Rx = ADDRESS of PGTHB + PGTHBOFFJ
* Reset Rx to be PGTHB + PGTHJOFFJ for the next thread
PGTHJ                 DSECT ,        T H R E A D - - - - - - - - - -
PGTHJID               DS    CL4      "gthj"
PGTHJLIMITJ           DS    CL1      A, S, X
PGTHJOFFJ             DS    FL3      Offset of next PgthJ
PGTHJLIMITK           DS    CL1      N, A, S, X
PGTHJOFFK             DS    FL3      Offset of PgthK, this thread
PGTHJTHID             DS    CL8      THREAD ID
PGTHJSYSCALL          DS    CL4      SYSCALL (eg. "1FRK" for fork)
PGTHJTCB              DS    A        TCB ADDRESS
PGTHJTTIME            DS    F        TIME RUNNING .001 SECS
PGTHJWTIME            DS    F        OE WAITING TIME .001 SECS
                      DS    F         space
PGTHJSEMNUM           DS    H        SEMAPHORE NUMBER IF STATUS2=D
PGTHJSEMVAL           DS    H        SEMAPHORE VALUE  IF STATUS2=D
PGTHJLATCHWAITPID     DS    F        LATCH PROCESS ID WAITED FOR
PGTHJPENMASK          DS    XL8      SIGNAL PENDING MASK
PGTHJLOGINNAME        DS    CL8      LOGIN NAME - ALIAS or MVS
PGTHJPREVSC           DS    5CL4     LAST FIVE SYSCALLS
PGTHJSTATUSCHARS      DS    0CL5     STATUS
*
PGTHJSTATUS1          DS    CL1      STATUS 1
PGTHJ#PTHDCREATED     EQU   C'J'       pthread created
*
PGTHJSTATUS2          DS    CL1      STATUS 2
PGTHJ#MSGRCV          EQU   C'A'       msgrcv wait
PGTHJ#MSGSND          EQU   C'B'       msgsnd wait
PGTHJ#WAITC           EQU   C'C'       communication wait
PGTHJ#SEMOP           EQU   C'D'       see PgthJSemVal/SemNum
PGTHJ#WAITF           EQU   C'F'       file system wait
PGTHJ#MVSPAUSE        EQU   C'G'       MVS in pause
PGTHJ#WAITO           EQU   C'K'       other kernel wait
PGTHJ#WAITP           EQU   C'P'       PTwaiting
PGTHJ#RUN             EQU   C'R'       running / non-kernel wait
PGTHJ#SLEEP           EQU   C'S'       sleep
PGTHJ#CHILD           EQU   C'W'       waiting for child
PGTHJ#FORK            EQU   C'X'       fork new process
PGTHJ#MVSWAIT         EQU   C'Y'       MVS wait
*
PGTHJSTATUS3          DS    CL1      STATUS 3
PGTHJ#MEDIUMWGHT      EQU   C'N'       medium weight thread
PGTHJ#ASYNC           EQU   C'O'       asynchronous thread
PGTHJ#IPT             EQU   C'U'       Initial process thread
PGTHJ#ZOMBIE          EQU   C'Z'       Process terminated and parent
*                                           has not completed wait
*
PGTHJSTATUS4          DS    CL1      STATUS 4
PGTHJ#DETACHED        EQU   C'V'       thread is detached
*
PGTHJSTATUS5          DS    CL1      STATUS 5
PGTHJ#FREEZE          EQU   C'E'       quiesce freeze
                      DS    CL3
*
PGTHJ#LEN             EQU   *-PGTHJ
*/

#define PGTHJ_STATUS1_THDCREATED	0xD1

#define PGTHJ_STATUS2_MSGRCV		0xC1
#define PGTHJ_STATUS2_MSGSND		0xC2
#define PGTHJ_STATUS2_WAITC			0xC3
#define PGTHJ_STATUS2_SEMOP			0xC4
#define PGTHJ_STATUS2_WAITF			0xC6
#define PGTHJ_STATUS2_MVSPAUSE		0xC7
#define PGTHJ_STATUS2_WAITO			0xD2
#define PGTHJ_STATUS2_WAITP			0xD7
#define PGTHJ_STATUS2_RUN			0xD9
#define PGTHJ_STATUS2_SLEEP			0xE2
#define PGTHJ_STATUS2_CHILD			0xE6
#define PGTHJ_STATUS2_FORK			0xE7
#define PGTHJ_STATUS2_MVSWAIT		0xE8

#define PGTHJ_STATUS3_MEDIUMWGHT	0xD5
#define PGTHJ_STATUS3_ASYNC			0xD6
#define PGTHJ_STATUS3_IPT			0xE4
#define PGTHJ_STATUS3_ZOMBIE		0xE9

#define PGTHJ_STATUS4_DETACHED		0xE5

#define PGTHJ_STATUS5_FREEZE		0xC5

/*
* USING PGTHH,Rx where Rx = ADDRESS of PGTHB + PGTHJOFFK
PGTHK                 DSECT ,        P T A G - - - - - - - - - - - -
PGTHKDATALEN          DS    F        LENGTH TO TRAILING NULL
PGTHKDATA             DS    CL68     SEE pthread_tag_np
PGTHK#LEN             EQU   *-PGTHH
*/
struct pgthk {
	int datalen;
	char data[68];
};


#if 0
/* we're not currently using pthread_quiesce_and_get_np, but may be in the future
 * if the signals prove too dangerous, so leaving this in here.
 */
/* flags.fbytes[1] */
#define THDQALLSAFE		0x80
/* flags.fbytes[4] */
#define THDQGETSTATE	0x80

/* thread.flags.fbytes[1] */
#define THDQANOTFOUND	0x80
#define THDQAQFRZSAFE	0x40
#define THDQAOTHERLE	0x20
#define THDQANODATA		0x10
#define THDQACONDWAIT	0x8

/* thread.flags.fbytes[4] */
#define THDQAQUICKFRZ	0x80
#define THDQAREGSOK		0x40
#define THDQASKIP		0x20

#define THDQREGSPPSD	1		/* Regs from PPSD */
#define THDQREGSIRB		2		/* Regs from IRB */
#define THDQREGSUSTA	3		/* Regs from USTA */
#define THDQREGSLS		4		/* Regs from link stack */
#define THDQREGSTCB		5		/* Regs from TCB/STCB */
#define THDQREGSRB		6		/* Regs from RB/XSB */
#define THDQREGSCW		7		/* Regs for CondWait. Status returned as zeroes */
#define THDQID			'THDQ'	/* Eye catcher */
#define THDQVER			1		/* Current version of control block */
#define THDQVER01		1		/* Version 1 of control block */

struct thdq {
	char eye[4];
	short length;
	short version;
	int numents;
	union {
		int fint;
		char fbytes[4];
	} flags;
	char exitwka[16];
	char __padding[16];
	/* start of dynamic section */
	union {
		char bytes[256];
		struct {
			char thid[8];
			union {
				int fint;
				char fbytes[4];
			} flags;
			short aregssrc;
			char __padding2[2];
			void *aregsh[16];
			void *aregsl[16];
			void *downstackptr;
			void *upstackptr;
			void *pswia;
			void *caaptr;
			char __padding3[4];
			void *tcbptr;
			char exitwka[8];
			char __padding[64];
		} thread;
	} array;
};

typedef struct __thdq
 {
   char                   __eye[4];
   unsigned short int     __length;
   unsigned short int     __version;
   unsigned int           __numents;

   struct
   {
     unsigned             __allsafe    :1 ;
     unsigned                          :31;
   }                      __flags;

   char                   ___0___[16];
   char                   ___1___[16];

   struct                 __thdq_
   {
     pthread_t            __thid;

     struct
     {
       unsigned           __notfound   :1 ;
       unsigned           __frzsafe    :1 ;
       unsigned           __otherle    :1 ;
       unsigned           __nodata     :1 ;
       #if __EDC_TARGET >= __EDC_LE210
         unsigned           __condwait   :1 ;
         unsigned                        :19;
       #else
       unsigned                        :20;
       #endif /* __EDC_TARGET >= __EDC_LE210 */
       unsigned           __quickfrz   :1 ;
       unsigned           __regsok     :1 ;
       #if __EDC_TARGET >= __EDC_LE210
         unsigned           __skip       :1 ;
         unsigned                        :5 ;
       #else
       unsigned                        :6 ;
       #endif /* __EDC_TARGET >= __EDC_LE210 */
     }                    __flags;

     char                 ___1___[4];
     unsigned int         __regsh[16];
     unsigned int         __regsl[16];
     void                *__downstackptr;
     void                *__upstackptr;
     void                *__pswia;
     void                *__caaptr;
     char                 ___6___[4];
     __ptr31(void,        __tcbptr)
     char                 ___7___[8];
     char                 ___8___[64];
   }                      __array[1];
 } __thdq_t;
#endif

#ifndef J9ZOS39064
void getthent(	int *input_length,
				struct pgtha **input,
				int *output_length,
				void **output_buffer,
				int *return_value,
				int *return_code,
				int *reason_code);

void pthread_quiesce(	int *request_type,
						char **user_data,
						int *return_value,
						int *return_code,
						int *reason_code);

#endif

/* TCB structure: http://publib.boulder.ibm.com/infocenter/zos/v1r9/topic/com.ibm.zos.r9.iead500/iea2d580218.htm */
#ifndef J9ZOS39064
struct tcb {
	char _padding_fpr[32];
	/* void *tcb starts here, fpr are at a negative offset */
	char _padding_tcpptr[48];
	int32_t gpr[16];
};

#endif



/* quiesce types from http://publib.boulder.ibm.com/infocenter/zos/v1r9/topic/com.ibm.zos.r9.bpxb100/ycons.htm?resultof=%22%51%55%49%45%53%43%45%5f%46%52%45%45%5a%45%22%20 */
#define QUIESCE_TERM         1  /*  Quiesce threads type = term      @D3A */
#define QUIESCE_FORCE        2  /*  Quiesce threads type = force     @D3A */
#define QUIESCE_QUERY        3  /*  Alias of pthread_query           @P4C */
#define PTHREAD_QUERY        3  /*  Quiesce threads type = query     @P4A */
#define QUIESCE_FREEZE       4  /*  Quiesce threads type = freeze    @D6A */
#define QUIESCE_UNFREEZE     5  /*  Quiesce threads type = unfreeze  @D6A */
#define FREEZE_THIS_THREAD   6  /*  Quiesce threads type = freezeme  @D6A */
/* Skip 7 because of collision with BPXZCONS Freeze_Force */
#define FREEZE_EXIT          8  /*  Quiesce threads type = freeze_exit */
#define QUIESCE_SRB          9  /*  Quiesce threads type = SRBs      @DGA */
/* Skip 10 and 11 due to collision with BPXZCONS Freeze/Unfreeze Fast */

#pragma pack(pop)

#endif
