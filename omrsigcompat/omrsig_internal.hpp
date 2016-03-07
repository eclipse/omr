/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015
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
 
#include <signal.h>
#if defined(WIN32)
/* windows.h defined UDATA.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#else /* defined(WIN32) */
#include <pthread.h>
#endif /* defined(WIN32) */

#include "AtomicSupport.hpp"

#if defined(WIN32)
#include "omrsig.h"

struct sigaction {
	sighandler_t sa_handler;
};

#else /* defined(WIN32) */

/* For now, only WIN32 is known to not support POSIX signals. Non-WIN32
 * systems which do not have POSIX signals are also supported.
 */
#define POSIX_SIGNAL

typedef void (*sigaction_t)(int sig, siginfo_t *siginfo, void *uc);

#if defined(J9ZOS390)
/* On zos, when an alt signal stack is set, the second call to pthread_sigmask() or
 * sigprocmask() within a signal handler will cause the program to sig kill itself.
 */
#define SECONDARY_FLAGS_WHITELIST (SA_NOCLDSTOP | SA_NOCLDWAIT)
#else /* defined(J9ZOS390) */
#define SECONDARY_FLAGS_WHITELIST (SA_ONSTACK | SA_NOCLDSTOP | SA_NOCLDWAIT)
#endif /* defined(J9ZOS390) */

#endif /* defined(WIN32) */

struct OMR_SigData {
	struct sigaction primaryAction;
	struct sigaction secondaryAction;
};

#if defined(J9ZOS390)
#define NSIG 65
#endif /* defined(J9ZOS390) */


#if defined(WIN32)

#define LockMask
#define SIGLOCK(sigMutex) \
	sigMutex.lock();
#define SIGUNLOCK(sigMutex) \
	sigMutex.unlock();

#else /* defined(WIN32) */

#define LockMask sigset_t *previousMask
#define SIGLOCK(sigMutex) \
	sigset_t previousMask; \
	sigMutex.lock(&previousMask);
#define SIGUNLOCK(sigMutex) \
	sigMutex.unlock(&previousMask);

#endif /* defined(WIN32) */

class SigMutex
{
private:
	volatile uintptr_t locked;

public:
	SigMutex()
	{
		locked = 0;
	}

	void lock(LockMask)
	{
#if !defined(WIN32)
		/* Receiving a signal while a thread is holding a lock would cause deadlock. */
		sigset_t mask;
		sigfillset(&mask);
		pthread_sigmask(SIG_BLOCK, &mask, previousMask);
#endif /* !defined(WIN32) */
		uintptr_t oldLocked = 0;
		do {
			oldLocked = locked;
		} while (0 != VM_AtomicSupport::lockCompareExchange(&locked, oldLocked, 1));
		VM_AtomicSupport::readWriteBarrier();
	}

	void unlock(LockMask)
	{
		VM_AtomicSupport::readWriteBarrier();
		locked = 0;

#if !defined(WIN32)
		pthread_sigmask(SIG_SETMASK, previousMask, NULL);
#endif /* !defined(WIN32) */
	}
};
