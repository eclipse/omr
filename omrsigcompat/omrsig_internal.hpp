/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
 
#include <signal.h>
#if (HOST_OS == OMR_WINDOWS)
/* windows.h defined UDATA.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#else /* (HOST_OS == OMR_WINDOWS) */
#include <pthread.h>
#endif /* (HOST_OS == OMR_WINDOWS) */

#include "AtomicSupport.hpp"

#if (HOST_OS == OMR_WINDOWS)
#include "omrsig.h"

struct sigaction {
	sighandler_t sa_handler;
};

#else /* (HOST_OS == OMR_WINDOWS) */

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
#elif (OMRZTPF) /* defined(J9ZOS390) */
#define SECONDARY_FLAGS_WHITELIST (SA_NOCLDSTOP)
#else /* defined(J9ZOS390) */
#define SECONDARY_FLAGS_WHITELIST (SA_ONSTACK | SA_NOCLDSTOP | SA_NOCLDWAIT)
#endif /* defined(J9ZOS390) */

#endif /* (HOST_OS == OMR_WINDOWS) */

struct OMR_SigData {
	struct sigaction primaryAction;
	struct sigaction secondaryAction;
};

#if defined(J9ZOS390)
#define NSIG 65
#elif defined(OMRZTPF)
#ifdef NSIG
#undef NSIG
#endif /* ifdef NSIG */
#define NSIG MNSIG
#endif /* defined(J9ZOS390) */


#if (HOST_OS == OMR_WINDOWS)

#define LockMask
#define SIGLOCK(sigMutex) \
	sigMutex.lock();
#define SIGUNLOCK(sigMutex) \
	sigMutex.unlock();

#if !defined(MSVC_RUNTIME_DLL)
#if (_MSC_VER == 1200) /* Visual Studio (VS) 6 */
#define MSVC_RUNTIME_DLL "MSVCR60.dll"
#elif (_MSC_VER == 1300) /* VS 7 */
#define MSVC_RUNTIME_DLL "MSVCR70.dll"
#elif (_MSC_VER == 1310) /* VS 7.1 */
#define MSVC_RUNTIME_DLL "MSVCR71.dll"
#elif (_MSC_VER == 1400) /* VS 8 */
#define MSVC_RUNTIME_DLL "MSVCR80.dll"
#elif (_MSC_VER == 1500) /* VS 9 */
#define MSVC_RUNTIME_DLL "MSVCR90.dll"
#elif (_MSC_VER == 1600) /* VS 10 */
#define MSVC_RUNTIME_DLL "MSVCR100.dll"
#elif (_MSC_VER == 1700) /* VS 11 */
#define MSVC_RUNTIME_DLL "MSVCR110.dll"
#elif (_MSC_VER == 1800) /* VS 12 */
#define MSVC_RUNTIME_DLL "MSVCR120.dll"
#elif (_MSC_VER > 1800) /* VS 14+ */
#define MSVC_RUNTIME_DLL "UCRTBASE.dll"
#else /* VS unknown */
/* This will assure that a user will update MSVC_RUNTIME_DLL
 * if it is undefined.
 */
#error "Unrecognized MSVC_RUNTIME_DLL."
#endif /* (_MSC_VER >= 1200) */
#endif /* !defined(MSVC_RUINTIME_DLL) */
#else /* (HOST_OS == OMR_WINDOWS) */

#define LockMask sigset_t *previousMask
#define SIGLOCK(sigMutex) \
	sigset_t previousMask; \
	sigMutex.lock(&previousMask);
#define SIGUNLOCK(sigMutex) \
	sigMutex.unlock(&previousMask);

#endif /* (HOST_OS == OMR_WINDOWS) */

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
#if !(HOST_OS == OMR_WINDOWS)
		/* Receiving a signal while a thread is holding a lock would cause deadlock. */
		sigset_t mask;
		sigfillset(&mask);
		pthread_sigmask(SIG_BLOCK, &mask, previousMask);
#endif /* !(HOST_OS == OMR_WINDOWS) */
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

#if !(HOST_OS == OMR_WINDOWS)
		pthread_sigmask(SIG_SETMASK, previousMask, NULL);
#endif /* !(HOST_OS == OMR_WINDOWS) */
	}
};
