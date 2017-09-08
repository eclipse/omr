/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief z/TPF-specific Dump formatting declarations
 */

#ifndef _ZTPF_J9OSDUMP_HELPERS_H_INCLUDED
#define _ZTPF_J9OSDUMP_HELPERS_H_INCLUDED

#include <tpf/i_jdib.h>
#include <tpf/i_jdbf.h>
#include <tpf/c_deri.h>
#include <elf.h>
#include <errno.h>
#include <tpf/tpfapi.h>

#ifndef OSDUMP_SYMBOLS_ONLY
#include "omrport.h"
#include "omrportpriv.h"
#endif

/*
 * Get process lock
 */
#define PROC_LOCK(proc_lock_ptr, old_key)       \
    do{                                         \
        cinfc(CINFC_WRITE, CINFC_CMMFS1);       \
        old_key = ecbp2()->ce2okey;             \
        if (is_threaded_ecb()) {                \
            lockc(proc_lock_ptr,LOCK_O_SPIN); } \
    } while(0)
/*
 * Release process lock
 */
#define PROC_UNLOCK(proc_lock_ptr, old_key)     \
    do{                                         \
        if (is_threaded_ecb()) {                \
            unlkc(proc_lock_ptr); }             \
        ecbp2()->ce2okey = old_key;             \
        keyrc_okey();                           \
    } while(0)

/* This data structure is the argument block whose address we need to pass to
 * ztpfBuildCoreFile() as its one and only parameter. Function
 * translateInterruptContexts() also uses it since it requires so much of the
 * same data.
 *
 * There are ten (10) data members in this block, it is 0x50 bytes long.
 */
typedef struct {
   siginfo_t *sii; /* argv[0] = ptr to siginfo_t */
   ucontext_t *uct;	/* argv[1] = ptr to ucontext_t */
   struct sigcontext *sct; /* argv[2] = ptr to struct sigcontext */
   uint8_t *OSFilename; /* argv[4] = ptr to dump OS filename */
   int32_t rc; /* argv[3] = return code area */
   uint32_t flags; /* argv[5] = 32 bits of output flags	*/
   uint32_t	wkspcSize; /* argv[6] = How big is argv[7]?	*/
   uint8_t *wkSpace; /* argv[7] = ptr to workspace */
   struct OMRPortLibrary	*portLibrary;   /* argv[8] = ptr to OMRPortLibrary */
   DIB *dibPtr;		  /* argv[9] = ptr to Dump Interchange Blk.	*/
} args;

/* Argument pointer Accessors */
#define dumpSiginfo(p) (p->sii)
#define dumpUcontext(p) (p->uct)
#define dumpSigcontext(p) (p->sct)
#define returnCode(p) (p->rc)
#define dumpFilename(p) (p->OSFilename)
#define dumpFlags(p) (p->flags)
#define workSpcSize(p) (p->wkspcSize)
#define workSpace(p) (p->wkSpace)
#define portLib(p) (p->portLibrary)
#define dibAddr(p) (p->dibPtr)
/* Values for args.flags follow  */
#define  J9TPF_NO_PORT_LIBRARY		0x01	 /* No port library available		*/
#define  J9TPF_SLAVE_OUTSTANDING	0x02	 /* The pthread spawned to			*/
											 /* ztpfBuildCoreFile() was launched*/
											 /* and we don't know if it's yet	*/
											 /* completed.						*/
#define  J9TPF_FILE_SYSTEM_ERROR	0x04	 /* File could not be written: media*/
#define  J9TPF_NO_JAVA_DUMPBUFFER	0x08	 /* There was a DIB but no JDB		*/
											 /*  anchored to it.				*/
#define  J9TPF_JDUMPBUFFER_LOCKED	0x10	 /* JDB in use or no JDB exists		*/
#define  J9TPF_OUT_OF_BUFFERSPACE	0x20	 /* Insufficient memory for I/O buf */
#define	 J9TPF_ERRMSG_IN_WKSPC		0x40	 /* Useable error msg in workSpace(p)	*/
#define  J9TPF_FLAGS_ERR_MASK \
(J9TPF_OUT_OF_BUFFERSPACE|J9TPF_JDUMPBUFFER_LOCKED|J9TPF_NO_JAVA_DUMPBUFFER|J9TPF_FILE_SYSTEM_ERROR)


		/*
		 *    Exportable entry point declarations. This must be referenced in a 
		 *    'version script' with which this module is linked. Conditionally,
		 *    when the build target is z/TPF, include it in "module.xml" so it
		 *    will come out in a file called "j9prt.exp".
		 */
void *			ztpfBuildCoreFile( void *argv ); /* This decl is set up to take a
					void pointer as its argument because of the constraints of
					pthread_create(), and we really mean to use a thread to 
					execute this function. It will cast the pointer back into 
					an (args *) because of this ugliness.						*/

		/*
		 *    External entry point declarations WRT this T.U.
		 */
extern void		ztpfDeriveSiginfo( siginfo_t *build ) __attribute__((nonnull));
extern void		translateInterruptContexts( args *argv ) __attribute__((nonnull));
extern uint8_t *allocateContextStorage( void );
extern void     ztpf_preemptible_helper(void);
extern void     ztpf_nonpreemptible_helper(void);

			/*********************************************************/
			/*	  Programming convenience definitions/declarations	 */
			/*********************************************************/
#ifndef  PAGE_SIZE
#define  PAGE_SIZE	 4096			   /* The size of one page (for vmm purposes)	*/
#undef	 HOLD_PAGE_SIZE
#else 
#define  HOLD_PAGE_SIZE PAGE_SIZE
#undef	 PAGE_SIZE
#define  PAGE_SIZE	 4096
#endif

#ifndef  TRUE
#define  TRUE  1
#endif
#ifndef	 FALSE
#define  FALSE 0
#endif

/**
 * \internal This macro pushes an address up to the nearest higher
 *	  paragraph (0x10) boundary.
 *
 * \param[in]  a	 Any address/pointer value
 *
 * \returns			 The address of the next higher paragraph.
 */
#define NEXT_PARA(a) (((((UDATA)(a))+15) >> 4) << 4 )

/**
 * \internal DBFINULL is a typecast-qualified equivalent of NULL
 */
#define DBFINULL	((DBFITEM *)0)
#define DBTIVADDR	(curr->ijavsad)			/** The starting EVA of the data region.			*/
#define DBTIOFFS	(curr->ijavsbuf)		/** The starting address in our local copy buffer.	*/
#define DBTISIZ		(curr->ijavsiz)			/** The size of the memory region					*/
#define DBTIGAP		(curr->ijavgap)			/** The gap field									*/
/*
 * DBITYPE is one byte of bitfields; if you write one zero there, you'll
 * have turned all its flags off. These bit indicators are symbolized 
 * for two pointer combinations; the DBITYPE bit addressed by the 
 * <tt>curr</tt> pointer and that addressed by the <tt>pred</tt> pointer. 
 * Values starting with 'C' are addressed off the <tt>curr</tt> pointer,
 * and values starting with 'P' are addressed off the <tt>pred</tt> pointer.
 *	nSETSTRT is the starting item for a contiguous set
 *	nSETMIDL is in a set, not the starter, not the ender
 *	nSETLAST is the last (ender) of a contiguous set
 *	nSETNONE means this item can be ignored
 */
#define DBTITYPE	(curr->ijavdesc+35)	/** A byte of bitfields, as follows:	*/
#define CSETSTRT	(curr->ijavsset)		/** Starts a set	*/
#define CSETMIDL	(curr->ijavmset)		/** Middle of a set	*/
#define CSETLAST	(curr->ijavlset)		/** End of a set	*/
#define CSETSOLO	(curr->ijavnone)		/** A set of one	*/
#define PSETSTRT	(pred->ijavsset)		/** Starts a set	*/ 
#define PSETMIDL	(pred->ijavmset)		/** Middle of a set	*/
#define PSETLAST	(pred->ijavlset)		/** End of a set	*/
#define PSETSOLO	(pred->ijavnone)		/** A set of one	*/

#ifdef	 HOLD_PAGE_SIZE
#undef	 PAGE_SIZE
#define  PAGE_SIZE	 HOLD_PAGE_SIZE
#undef	 HOLD_PAGE_SIZE
#endif

/*
 *	  Aggregate data type declarations of importance to the ELF core file.
 *	  Most of these aggregates appear somewhere in the .NOTE section.
 */
					 
/*					 
 *	The NT_AUXV segment means something to a UNIX or Linux system. It means
 *	nothing to z/TPF; we don't turn over control to the system interpreter (we
 *	don't even HAVE one) like *ix systems do. So instead, we're going to
 *	fulfill the requirement for an NT_AUXV segment with a fake one of 304 zeros.
 */
typedef struct fake_auxv {
   uint8_t fake_data[304];		/* Gotta go with 304 zeros here.			*/
} AUXV;

typedef struct { 
   uint64_t	hi_word;
   uint64_t	lo_word;
} psw_t;

typedef struct {
   long double	fpxreg[16];
} fpxregset_t;

/*
 * Here, in UNIX sequence, are the s390x general registers. We aren't honoring
 * any other hardware architecture, so this is it.
 */
typedef struct {
   psw_t		psw;				/* Program status word						*/
   uint64_t		gprs[16];			/* General purpose registers				*/
   uint32_t		acrs[16];			/* Access registers (no value in z/TPF)		*/
   uint64_t		orig_gpr2;			/* Original R2, held by interpreter.		*/
} s390_regs;

typedef struct shortsiginfo_t {		/* This is the leading struct in ELF		*/
	uint32_t		si_signo;		/*	PRSTATUS.								*/
	uint32_t		si_code;
	uint32_t		si_errno;
} ziginfo_t;

typedef struct {					/* Process status block						*/
	ziginfo_t		info;			/* Info associated with signal				*/
	int16_t			cursig;			/* Current signal							*/
	uint64_t		sigpend;		/* Set of pending signals					*/
	uint64_t		sighold;		/* Set of held signals						*/
	pid_t			pid;			/* Process id number						*/
	pid_t			ppid;			/* Parent process id number					*/
	pid_t			pgid;			/* Process group id (irrelevant in z/TPF)	*/
	pid_t			sid;			/* Session id (irrelevant in z/TPF)			*/
	struct timeval	utime;			/* User CPU time							*/
	struct timeval	stime;			/* System CPU time							*/
	struct timeval	cutime;			/* Cumulative user time						*/
	struct timeval	cstime;			/* Cumulative system time					*/
	s390_regs		reg;			/* General purpose registers				*/
	int32_t			fpvalid;		/* True if math co-processor being used.	*/
} prstatus_t;						/* Datatype for process status				*/

#define PRARGSZ	(80)				/* Number of chars for args */

typedef struct {					/* Process state information block			*/
   uint8_t		char_state;			/* numeric process state					*/
   uint8_t		sname;				/* char process state 'name'				*/
   uint8_t		zomb;				/* zombie: yes or no?						*/
   uint8_t		nice;				/* nice val									*/
   uint64_t		flag;				/* flags									*/
   uid_t		uid;				/* userid									*/
   gid_t		gid;				/* group id									*/
   pid_t		pid;				/* process id								*/
   pid_t		ppid;				/* parent process id						*/
   pid_t		pgrp;				/* process group (irrelevant in z/TPF)		*/
   pid_t		sid;				/* session id (irrelevant in z/TPF)			*/
   uint8_t		fname[16];			/* filename of executable/entry point		*/
   uint8_t		psargs[PRARGSZ];	/* initial part of arg list					*/
} prpsinfo_t;						/* Datatype for process state info			*/

#endif

