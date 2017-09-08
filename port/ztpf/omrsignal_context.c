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
 * \file
 * \ingroup Port
 * \brief Saves and returns information about signals
 */

#define ZTPF_NEEDS_GETCONTEXT

#include <errno.h>
#include <string.h>
#include <sys/ucontext.h>
#include "sys/sigcontext.h"
#include <tpf/c_proc.h>
#include "omrport.h"
#include "omrsignal_context.h"

/**
 * The number of registers in the hardware implementation.
 */
#define NUM_REGS 16

/**
 * The number of elements in our PIC-to-si_signo,si_code table
 */
#define PIC_TABLE_MAX  60 

/*
 *	What follows is a set of signal number & signal code values to which a s390
 *	PIC should translate. For multiply-assignable high-level PICs, leave the 
 *	si_code field zero; we can try to clear it up after PIC translation. This 
 *	table was pretty well matched up with how CPSE treats PICs, apart from 
 *	having to deal with PER and/or transactional considerations.
 *
 *	In terms of virtual memory management, we work in a primary-space mode. 
 *	Exceptions associated with the all the others: secondary-space mode,
 *	access-register mode, and home mode. Virtual address mapping is the same
 *	for all CPUs (I-streams) except for page 0 (addresses 0-0400 hex).
 */
static const sigvpair_t	pic2pair[] = {	IMPOSSIBLE, /*	0: Not assigned						*/
	{ SIGILL,	ILL_ILLOPC },	/* PIC 1, Operation exc */
	{ SIGILL,	ILL_PRVOPC },	/* PIC 2, Priv Oper'n	*/
	{ SIGILL,	ILL_ILLADR },	/* PIC 3, Execute exc.	*/
	{ SIGSEGV,	SEGV_ACCERR },	/* PIC 4: Protection exc*/
	{ SIGSEGV,	SEGV_ACCERR },	/* PIC 5: Addressing	*/
	{ SIGILL,	ILL_ILLOPN },	/* PIC 6, Spec exc		*/
	{ SIGFPE,	0 },			/* PIC 7: Data. All math*/
	{ SIGFPE,	FPE_INTOVF },	/* PIC 8: Int overflow	*/
	{ SIGFPE,	FPE_INTDIV },	/* PIC 9: Int divide	*/
	{ SIGFPE,	FPE_INTOVF },	/* PIC A: Dec overflow	*/
	{ SIGFPE,	FPE_INTDIV },	/* PIC B: Dec divide	*/
	IMPOSSIBLE, /*	c: HFP Exponent O'flow. Impossible	*/
	IMPOSSIBLE, /*	d: HFP Exponent U'flow. Impossible	*/
	IMPOSSIBLE, /*	e: HFP Significance. Impossible.	*/
	IMPOSSIBLE, /*	f: HFP Divide Exception. Impossible */
	{ SIGSEGV,	SEGV_MAPERR },	/* PIC 10: PST exc(seg) */
	{ SIGSEGV,	SEGV_MAPERR },	/* PIC 11: PST exc(tbl) */
	{ SIGSEGV,	SEGV_MAPERR },	/* PIC 12: Translation	*/
	{ SIGILL,	ILL_PRVOPC },	/* PIC 13: Spcl Oper	*/
	IMPOSSIBLE, /* 14:									*/
	{ SIGSEGV,	SEGV_ACCERR },	/* PIC 15: Operand excp */
	IMPOSSIBLE, /* 16: Trace table. Impossible.			*/
	IMPOSSIBLE, /* 17:									*/
	IMPOSSIBLE, /* 18:									*/
	IMPOSSIBLE, /* 19:									*/
	IMPOSSIBLE, /* 1a: Block-Volatility. Impossible.	*/
	IMPOSSIBLE, /* 1b:									*/ 
	IMPOSSIBLE, /* 1c: Space switch. Impossible.		*/
	IMPOSSIBLE, /* 1d: HFP Square Root. Impossible.		*/
	IMPOSSIBLE, /* 1e:									*/
	IMPOSSIBLE, /* 1f: PC Translation-Spec. Impossible. */
	IMPOSSIBLE, /* 20: AFX-Translation. Impossible		*/
	IMPOSSIBLE, /* 21: ASX Translation. Impossible		*/
	IMPOSSIBLE, /* 22: LX Translation. Impossible.		*/
	IMPOSSIBLE, /* 23: EX-Translation. Impossible.		*/
	IMPOSSIBLE, /* 24: Primary Authority. Impossible.	*/
	IMPOSSIBLE, /* 25: Secondary Authority. Impossible. */
	IMPOSSIBLE, /* 26: LFX Translation. Impossible.		*/
	IMPOSSIBLE, /* 27: LSX Translation. Impossible.		*/
	IMPOSSIBLE, /* 28: ALET Specification. Impossible.	*/
	IMPOSSIBLE, /* 29: ALEN Xlation. Impossible.		*/
	IMPOSSIBLE, /* 2a: ALET Sequence. Impossible.		*/
	IMPOSSIBLE, /* 2b: ASTE Validity. Impossible.		*/
	IMPOSSIBLE, /* 2c: ASTE Sequence. Impossible.		*/
	IMPOSSIBLE, /* 2d: Extended Authority. Impossible.	*/
	IMPOSSIBLE, /* 2e: LSTE Sequence. Impossible.		*/
	IMPOSSIBLE, /* 2f: ASTE Instance. Impossible.		*/
	IMPOSSIBLE, /* 30: Stack full. Impossible.			*/
	IMPOSSIBLE, /* 31:									*/
	IMPOSSIBLE, /* 32: Stack specification. Impossible. */
	IMPOSSIBLE, /* 33: Stack type. Impossible.			*/
	IMPOSSIBLE, /* 34: Stack operation. Impossible.		*/
	IMPOSSIBLE, /* 35:									*/
	IMPOSSIBLE, /* 36:									*/
	IMPOSSIBLE, /* 37:									*/
	{ SIGSEGV,	SEGV_MAPERR },	/* PIC 38: ASCE Excep.	*/
	{ SIGSEGV,	SEGV_MAPERR },	/* PIC 39: Reg 1st Xlt	*/
	{ SIGSEGV,	SEGV_MAPERR },	/* PIC 3a: Reg 2nd Xlt	*/
	{ SIGSEGV,	SEGV_MAPERR }	/* PIC 3b: Reg 3rd Xlt	*/
}; 
typedef	struct iproc IPROC;

#include "safe_storage.h"

static void	ztpfDeriveSigcontext( struct sigcontext *sigcPtr, ucontext_t *uctPtr );
static void	ztpfDeriveUcontext( ucontext_t *uscPtr );
safeStorage *allocateDurableStorage( void );

#define MAX_CSTG_BLOCKS	16
#define _RGN_SIZE (sizeof(safeStorage))
/*
 * TOD_16_SECONDS is really 16.777216 seconds, but who's splitting hairs?
 * See Principles Of Operation, page 4-61 ... we're masking out up to bit 28.
 * If we cycle through all these blocks in 16 wall-clock seconds, allocate
 * twice as many and try again. Likewise if we find the 16-second cycle time
 * too short.
 */
#define TOD_16_SECONDS (UINT_MAX & 0x0000000fffffffff)

typedef struct _unixcontext_block {
	uint64_t tod;
	safeStorage region;
} ustgndx;

/*
 * These two short lines define a whole slew of space.
 */
static ustgndx regions[MAX_CSTG_BLOCKS];
static uint32_t	regions_used;

/**
 * \function allocateDurableStorage()
 *
 * Prior to some testing, we discovered that allocating UNIX-formatted signal-related
 * context blocks on the stack wasn't such a wise idea once the original faulting thread
 * had gone back to the thread pool and its stack got tromped all over. This fact was
 * completely unknown until late in development.
 *
 * The solution was to grab some storage that wouldn't be subject to such problems. The
 * memory into which the .bss segment is assigned is a (set of) master page(s) of hex 
 * zeros, and once written upon, is copied-on-write and become the property of the 
 * PROCESS (don't want to raise any confusion here) which did the writing. Therefore,
 * this storage appears zero to each new process which calls this module. NO SMP-
 * RELATED CONCURRENCY CONCERNS EXIST; one of the attractive features which forced
 * us into this storage class. The biggest users, by far, of this space are threads
 * which have called the system dump agent, whether they've taken an actual fault or
 * not. We labor under the presumption that the lifetime of these context blocks is
 * well under 16 seconds, and TOD-stamp test the time of each block's allocation 
 * before reusing it.
 *
 * This E/P must be exported! File module.xml is therefore modified conditionally to 
 * export this label, and it will appear in j9prt.exp when it is created for z/TPF, 
 * when the buildtools are built. z/TPF is the only known system which needs this 
 * kind of allocation. It is used exclusively for the translation of z/TPF-ish data 
 * into UNIX-ish context & signal blocks.
 *
 * 
 * \returns		A uint8_t pointer sufficiently large to house all three required UNIX-
 *				style context & signal information blocks. IT IS UP TO THE CALLER
 *				TO PARTITION THIS BLOCK.
 */

safeStorage *
allocateDurableStorage( void )
{ 
	uint8_t	i = 0;
	safeStorage *rtnv;
	uint64_t todNow;
	tpf_TOD_type tod;
	uint64_t elapsed;

reenter:
	tpf_STCK( &tod );
	todNow = tod.stck_hex;
	if( regions_used ) {
		for( i = 0; i < MAX_CSTG_BLOCKS; i++ ) {
			elapsed = todNow - regions[i].tod;
			if (elapsed > TOD_16_SECONDS ) {
				break;
			}
		}
		if( i == MAX_CSTG_BLOCKS ) {
			usleep(50000);
			goto reenter;
		}
	}
	/*
	 * We've made our selection. Reserve it and clear it out.
	 */
	regions[i].tod = todNow;
	regions_used += 1;
	rtnv = &(regions[i].region);
	memset( rtnv, 0, sizeof(safeStorage) );
	rtnv->pPROC = iproc_process;
	rtnv->argv.sii = &(rtnv->siginfo);
	rtnv->argv.uct = &(rtnv->ucontext);
	rtnv->argv.sct = &(rtnv->sigcontext);
	rtnv->argv.wkspcSize = PATH_MAX;
	rtnv->argv.wkSpace = &(rtnv->workbuffer[0]);
	rtnv->argv.OSFilename = &(rtnv->filename[0]);
	return rtnv;
}

void
ztpfDeriveSiginfo( siginfo_t *build ) {
   uint16_t	pic;
   uint16_t	ilc;
   uint8_t dxc;
   sigvpair_t *translated = NULL;
   void *lowcore = NULL;
   DIB *pDib = ecbp2()->ce2dib;
   DBFHDR *pDbf = (DBFHDR *)(pDib->dibjdb);
   DBFITEM *pDbi;
   struct cderi *pEri = NULL;
   struct iproc *procPtr = iproc_process;

   /*
	*	 If we have a java dump buffer (ijavdbf) which belongs to this
	*	 thread AND that buffer is not locked, then use it. We prefer
	*	 the low core image in the java dump buffer to the copy in the
	*	 DIB; but use the DIB's copy if there's no buffer.
	*/
   if( pDbf && pDbf->ijavcnt ) {	/* We have the full Error Recording Info, use it.	*/
	  pDbi = (DBFITEM *)pDbf+1;					/* We have a bunch of void ptrs			*/
	  pEri = (struct cderi *)(pDbi->ijavsbuf);	/*	to cast through ... ugh...			*/
	  lowcore = pEri;							/* And struct cderi is incomplete ...	*/
	  lowcore += (unsigned long int)0x310;		/*	ai yi yi Lucy.						*/
   }
   else {							/* All we have is the DIB, so use it instead.		*/
	  lowcore = pDib->dilow;					/* Fixup low core pointer				*/
	  pEri = NULL;								/* And indicate there's no full ERI.	*/
   }
   pic = s390FetchPIC( lowcore );		/* Translate the PIC into si_signo & si_code	*/
   translated = (sigvpair_t *)&pic2pair[pic];
   build->si_signo = translated->si_signo;
   build->si_code = translated->si_code;
   /*
	* This could just as well have been an "if-then-else" construct, but
	* it was felt wise to leave it in case testing revealed something else
	* that requires analysis.
	*/
   switch( build->si_signo ) { 
   case SIGFPE:
	  if( 0 == build->si_code ) {		   /* If we need to interpret DXC bits,   */
		 /*
		 * Returning a good si_code value means we have to interrogate the
		 * DXC value, so go get a normalized version of it.
		 */
		 dxc = s390FetchDXC( lowcore );
		 /*
		  * "Sluice" through the acceptable values of the DXC for known si_code
		  * values. If we don't find one, call it SI_KERNEL for lack of any 
		  * better idea.
		  */
		 if( 0x09 == dxc )
			build->si_code = FPE_INTDIV;
		 else if( 0x08 == dxc )
			build->si_code = FPE_INTOVF;
		 else if( 0x40 == dxc )
			build->si_code = FPE_FLTDIV;
		 else if( 0x20 == dxc )
			build->si_code = FPE_FLTOVF;
		 else if( 0x10 == dxc )
			build->si_code = FPE_FLTUND;
		 else if( 0x08 == dxc )
			build->si_code = FPE_FLTRES;
		 else if( 0x80 == dxc )
			build->si_code = FPE_FLTINV;
		 else
			build->si_code = SI_KERNEL;    /* Unknown synchronous signal	  */
	  }
	  break;
	/*
	 * SIGSEGV signals need si_addr set to the addr that failed translation
	 * It's in the ERI, but not in the DIB. So if we have an ERI, use it.
	 */
   case SIGSEGV:
	  if( build->si_code == SEGV_MAPERR && pEri ) { 
		 uint64_t *address;
		 lowcore = pEri;			/* The failing vaddr is at ISVTRXAD */
		 address = lowcore+(unsigned long int)0x1b8;	/*	at offset 0x1B8 into the ERI*/
		 build->si_addr = (void *)*address; /* Move the failed address.		*/
	  }									/* Otherwise leave it zero.			*/
	  break;
   case SIGILL:
	  build->si_addr = (char *)pDib->dispw[1];	/*	Failing PSW $IP			*/
	  break;
   default:
	  break;
   }										
   build->si_pid = procPtr->iproc_pid;	/* All of them need PID and			 */
   build->si_uid = procPtr->iproc_uid;	/*	UID values, with TID & errno	 */
   build->si_ztpf_tid = ecbp2()->ce2thnum; /* values, too.					 */
   build->si_errno = errno;
   return;
}

/*
 * This routine completes the blank uscPtr-based memory area
 * into which the sigcontext structure is supposed to go.
 *
 * It cannot be called until the ucontext block has been filled
 * in so we have a place to conveniently ripoff information from.
 */

static void
ztpfDeriveSigcontext( struct sigcontext *sigcPtr, ucontext_t *uctPtr ) {
        memset( sigcPtr, 0, sizeof(*sigcPtr) );         /* Set it all to zeros, then fill it all in.     */

        sigcPtr->sregs->regs.psw.mask  = uctPtr->uc_mcontext.psw.mask;
        sigcPtr->sregs->regs.psw.addr  = uctPtr->uc_mcontext.psw.addr;

        memcpy(&(sigcPtr->sregs->regs.gprs), &(uctPtr->uc_mcontext.gregs),sizeof(sigcPtr->sregs->regs.gprs));
        memcpy(&(sigcPtr->sregs->regs.acrs), &(uctPtr->uc_mcontext.aregs),sizeof(sigcPtr->sregs->regs.acrs));

        memcpy(&(sigcPtr->sregs->fpregs), &(uctPtr->uc_mcontext.fpregs), sizeof(fpregset_t));

        sigcPtr->oldmask[0]       = uctPtr->uc_sigmask;
	return;
}


static void
ztpfDeriveUcontext( ucontext_t *uscPtr ) { 
   DIB * pDib = ecbp2()->ce2dib;
   DBFHDR * pJdb = (DBFHDR *)(pDib->dibjdb);
   DBFITEM * pDbi = NULL;
   void * lowcore = NULL;

   memset( uscPtr, 0, sizeof(*uscPtr) );	/* Zeroize everything		*/
   if( pJdb ) {					 /* If JDB contents are for this thread */
	  if( pJdb->ijavcnt ) {		 /*  and the JDB is unlocked			*/
		 pDbi = (DBFITEM *)(pJdb+1);		 /* Point at the JDB base	*/
		 lowcore = (pDbi->ijavsbuf);		/*  and its low core copy. */
	  }
	  else {					 /* Otherwise use what's in the DIB		*/
		 lowcore = pDib->dilow;  /* Use the low core pointer from DIB	*/
	  }
   }
	/*
     *	 Fill in the uc_mcontext, uc_flags and uc_sigmask fields. 
	 *	 Leave uc_link alone, it has no meaning in or for z/TPF.
     */
   uscPtr->uc_mcontext.psw.mask = pDib->dispw[0];  /* PSW lo half */
   uscPtr->uc_mcontext.psw.addr = pDib->dispw[1];  /* PSW hi half */
   memcpy( uscPtr->uc_mcontext.gregs,
		  pDib->direg, sizeof(pDib->direg) );		/*GPRs*/
	memcpy( uscPtr->uc_mcontext.fpregs.fprs, 
		  pDib->dfreg, sizeof(pDib->dfreg) );		/*FPRs*/
	uscPtr->uc_flags = ecbp2()->ce2thnum;			/* Put the TID in ucontext->uc_flags  */
	sigprocmask( SIG_SETMASK, NULL, &(uscPtr->uc_sigmask) ); /* Our mask is the old mask  */
	return;
}


/**
 * \brief Translate z/TPF-ish data into UNIX-ish context data
 *
 * This routine sets up all three failure context records needed
 * by UNIX-ish 3-argument signal handlers: the sigcontext, the
 * ucontext_t, and siginfo_t structures, one at a time.
 *
 * \param[in][out] si	Address of a blank siginfo_t buffer.
 * \param[in][out] uc	Address of a blank ucontext_t buffer.
 * \param[in][out] sc	Address of a blank struct sigcontext buffer.
 *
 * \returns void		All blank buffer regions filled in.
 */
void
translateInterruptContexts( args *argv ) { 

   ztpfDeriveSiginfo( dumpSiginfo(argv) );
   ztpfDeriveUcontext( dumpUcontext(argv) );
   ztpfDeriveSigcontext( dumpSigcontext(argv), dumpUcontext(argv) );
   return;
}


/**
 * \brief Store signal information in the J9UnixSignalInfo struct.
 *
 * This routine is <em>supposed</em> to store the context information provided
 * to it by its caller in a J9UnixSignalInfo structure. We have no idea where 
 * that struct is stored, the caller must provide it.
 *
 * \param[in]	portLibrary		Pointer to the OMRPortLibrary in use
 * \param[in]	contextInfo		Pointer to context information
 * \param[out]	j9info			Pointer to a J9UnixSignalInfo structure
 *
 * \returns		Not a thing.
 */
void
fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct J9UnixSignalInfo *j9Info)
{
	j9Info->platformSignalInfo.context = (ucontext_t *)contextInfo;		/* module info is filled on demand */
}


/**
 * \brief Return specific signal information to the caller.
 *
 * This routine stores the name and value attributes of a given signal 
 * described in a J9UnixSignalInfo structure whose address is passed 
 * by the caller. The <TT>index</TT> parameter is the artificial J9
 * signal value (or 'class type'?) associated with the kind of signal.
 *
 * \param[in]	portLibrary		The OMRPortLibrary in use
 * \param[in]	info			A pointer to the J9UnixSignalInfo from which the information
 *								is taken
 * \param[in]	index			The J9 signal value assigned to the 'signal type'
 * \param[out]	name			The name of the signal type class, double pointer to type char
 * \param[out]	value			The real (raw) signal number
 *
 * \returns		Unsigned 32-bit integer describing what the \ref value return is, an address, or
 *				an address. Also can return an error indicator.
 */
U_32
infoForSignal(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (index) {
	case OMRPORT_SIG_SIGNAL_TYPE:
	case 0:
		*name = "J9Generic_Signal_Number";
		*value = &info->portLibrarySignalType;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE:
	case 1:
		*name = "Signal_Number";
		*value = &info->sigInfo->si_signo;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_ERROR_VALUE:
	case 2:
		*name = "Error_Value";
		*value = &info->sigInfo->si_errno;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_CODE:
	case 3:
		*name = "Signal_Code";
		*value = &info->sigInfo->si_code;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_HANDLER:
	case 4:
		*name = "Handler1";
		*value = &info->handlerAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 5:
		*name = "Handler2";
		*value = &info->handlerAddress2;
		return OMRPORT_SIG_VALUE_ADDRESS;

	case OMRPORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS:
	case 6:
		/*
		 * si_code > 0 indicates that the signal was generated by the kernel
		 */
		if ( info->sigInfo->si_code > 0 ) {
			if ( ( info->sigInfo->si_signo == SIGSEGV ) ) {
				*name = "InaccessibleAddress";
				*value = &info->sigInfo->si_addr;	/* This will be wrong for z/TPF until I figure
														out where to get the failing address.		*/
				return OMRPORT_SIG_VALUE_ADDRESS;
			}
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}
	
/**
 * \brief	Return a floating point register name
 *
 * This routine is passed an index value, and returns the name of the register at that
 * index location as the <TT>name</TT>. The <TT>value</TT> parameter appears to receive
 * the register * content from context information.
 *
 * \param[in]	portLibrary		Pointer to the OMRPortLibrary in use
 * \param[in]	info			Pointer to applicable J9UnixSignalInfo structure.
 * \param[in]	index			Number of the floating point register sought.
 * \param[out]	name			Double pointer to type char, name of the register
 * \param[out]	value			Double pointer to type void, likely to be last known
 *								content of the register, so it'd be a pointer to a 
 *								pointer of type <TT>double</TT>.
 */
uint32_t
infoForFPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	const char *n_fpr[NUM_REGS] = {
									"fpr0", 
									"fpr1",  
									"fpr2",  
									"fpr3",  
									"fpr4",  
									"fpr5",  
									"fpr6",  
									"fpr7",  
									"fpr8",  
									"fpr9",  
									"fpr10",  
									"fpr11",  
									"fpr12",  
									"fpr13",  
									"fpr14",  
									"fpr15"}; 
	*name = "";
	if ( (index >=0) && (index < NUM_REGS)) {
		*name = n_fpr[index];
		*value = &(info->platformSignalInfo.context->uc_mcontext.fpregs.fprs[index]);
		return OMRPORT_SIG_VALUE_FLOAT_64;
	}
	return OMRPORT_SIG_VALUE_UNDEFINED;
}

/**
 * \brief	Return a general register name and content
 *
 * See the documentation for <TT>infoForFPR()</TT>, above; applicable to 
 * general registers instead of FPRs.
 *
 * \param[in]	portLibrary		Pointer to the OMRPortLibrary in use
 * \param[in]	info			Pointer to applicable J9UnixSignalInfo structure.
 * \param[in]	index			Number of the general register sought.
 * \param[out]	name			Double pointer to type char, name of the register
 * \param[out]	value			Double pointer to type void, last known value
 *								stored in the register.
 *
 * \note	Original z/TPF POC had this routine guarded out and returning a value of 
 *			OMRPORT_SIG_VALUE_UNDEFINED, so it appeared that the signal was not supported
 *			in z/TPF, no matter what it was. 
 * \note	This routine was unguarded as part of making the CP emulate kernel-space, UNIX-
 *			style implementations of reactions to program interrupts.
 * \note	There were guard macros here based on #ifndef J9VM_ENV_DATA64 which I've simply
 *			removed. If the guard value were true, there were 16 more definition slots for
 *			the high parts of the general registers. All gone; we don't support 32-bit 
 *			hardware (even if we do grudgingly support AMODE=31).
 */
U_32
infoForGPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	const char *n_gpr[NUM_REGS] = {
									"gpr0", 
									"gpr1",  
									"gpr2",  
									"gpr3",  
									"gpr4",  
									"gpr5",  
									"gpr6",  
									"gpr7",  
									"gpr8",  
									"gpr9",  
									"gpr10",  
									"gpr11",  
									"gpr12",  
									"gpr13",  
									"gpr14",  
									"gpr15"
	};

	*name = "";
	if ( (index >=0) && (index < NUM_REGS)) {
		*name = n_gpr[index];
		*value = &(info->platformSignalInfo.context->uc_mcontext.gregs[index]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	}
	return OMRPORT_SIG_VALUE_UNDEFINED;
}

/**
 * \fn	Return miscellaneous h/w control words from signal/dump contexts.
 *
 * \param[in]	portLibrary		Pointer to the portability library in use
 * \param[in]	info			Pointer to J9UnixSignalInfo block
 * \param[in]	index			The J9 'type class' of the signal associated with this dump
 * \param[out]	name			Dbl pointer to a buffer large enough to hold an object name
 * \param[out]	value			Dbl pointer to a buffer large enough to hold an object value
 *
 * \note	We have to trust that the pointers to <tt>name</tt> and <tt>value</tt> are properly
 *			sized and permissioned.
 *
 */
U_32
infoForControl(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	uint8_t *eip;
	mcontext_t *mcontext = (mcontext_t *)&info->platformSignalInfo.context->uc_mcontext;
	*name = "";
	
	switch (index) {
	case OMRPORT_SIG_CONTROL_PC:
	case 0:
		*name = "psw";
		*value = &(mcontext->psw.addr);
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 1:
		*name = "mask";
		*value = &(mcontext->psw.mask);
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_S390_FPC:
	case 2:
		*name = "fpc";
		*value = &(mcontext->fpregs.fpc);
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_S390_BEA:
	case 3:
		*name = "bea";
		*value = &(info->platformSignalInfo.breakingEventAddr);
		return OMRPORT_SIG_VALUE_ADDRESS;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
	/* UNREACHABLE */
}

U_32
infoForModule(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	Dl_info		*dl_info = &(info->platformSignalInfo.dl_info);
	void*		address;
	int32_t		dl_result;
	mcontext_t *mcontext = (mcontext_t *)&info->platformSignalInfo.context->uc_mcontext;
	*name = "";

	address = (void *)mcontext->psw.addr;
	dl_result = dladdr(address, dl_info);
	switch (index) {
	case OMRPORT_SIG_MODULE_NAME:
	case 0:
		*name = "Module";
		if (dl_result) {
			*value = (void *)(dl_info->dli_fname);
		return OMRPORT_SIG_VALUE_STRING;
		}
	return OMRPORT_SIG_VALUE_UNDEFINED;
	case 1:
		*name = "Module_base_address";
		if (dl_result) {
			*value = (void *)&(dl_info->dli_fbase);
		return OMRPORT_SIG_VALUE_ADDRESS;
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	case 2:
		*name = "Symbol";
		if (dl_result) {
			if (dl_info->dli_sname != NULL) {
				*value = (void *)(dl_info->dli_sname);
				return OMRPORT_SIG_VALUE_STRING;
			}
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
		case 3:
			*name = "Symbol_address";
			if (dl_result) {
				*value = (void *)&(dl_info->dli_saddr);
				return OMRPORT_SIG_VALUE_ADDRESS;
			}
			return OMRPORT_SIG_VALUE_UNDEFINED;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
	/*	UNREACHABLE  */
	return OMRPORT_SIG_VALUE_UNDEFINED;
}

