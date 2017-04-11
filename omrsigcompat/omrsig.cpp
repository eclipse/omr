/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#if defined(J9ZOS390)
#define _OPEN_THREADS 2
#define _UNIX03_SOURCE
#endif /* defined(J9ZOS390) */

#if defined(WIN32)
/* windows.h defined UDATA.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#else /* defined(WIN32) */
#include <dlfcn.h>
#endif /* defined(WIN32) */
#include <errno.h>
#if !defined(WIN32)
#include <pthread.h>
#endif /* !defined(WIN32) */
#include <string.h>

#include "omrsig.h"
#include "omrsig_internal.hpp"

extern "C" {

static OMR_SigData sigData[NSIG];
static SigMutex sigMutex;

static sighandler_t omrsig_signal_internal(int signum, sighandler_t handler);
static int omrsig_sigaction_internal(int signum, const struct sigaction *act, struct sigaction *oldact, bool primary);
static bool validSignalNum(int signum, bool nullAction);
static bool handlerIsFunction(const struct sigaction *act);

#if defined(WIN32)

typedef void (__cdecl * (__cdecl *SIGNAL)(_In_ int _SigNum, _In_opt_ void (__cdecl * _Func)(int)))(int);
static SIGNAL signalOS = NULL;

#elif defined(J9ZOS390)

#pragma map (sigactionOS, "\174\174SIGACT")
extern int sigactionOS(int, const struct sigaction*, struct sigaction*);
static bool checkMaskEquality(int signum, const sigset_t *mask, const sigset_t *otherMask);

#elif defined(POSIX_SIGNAL)

typedef int (*SIGACTION)(int signum, const struct sigaction *act, struct sigaction *oldact);
static SIGACTION sigactionOS = NULL;

#else /* defined(POSIX_SIGNAL) */

typedef sighandler_t (*SIGNAL)(int signum, sighandler_t handler);
static SIGNAL signalOS = NULL;

#endif /* !defined(POSIX_SIGNAL) */

static bool
handlerIsFunction(const struct sigaction *act)
{
	bool functionHandler = (SIG_DFL != act->sa_handler) && (SIG_IGN != act->sa_handler) && (NULL != act->sa_handler);
#if defined(POSIX_SIGNAL)
	return (((sigaction_t)SIG_DFL != act->sa_sigaction)
		&& ((sigaction_t)SIG_IGN != act->sa_sigaction)
		&& (SA_SIGINFO & act->sa_flags))
		|| functionHandler;
#else /* defined(POSIX_SIGNAL) */
	return functionHandler;
#endif /* defined(POSIX_SIGNAL) */

}

static bool
validSignalNum(int signum, bool nullAction)
{
#if defined(WIN32)
	return (SIGABRT == signum) || (SIGFPE == signum) || (SIGILL == signum) || (SIGINT == signum)
		|| (SIGSEGV == signum) || (SIGTERM == signum);
#elif defined(J9ZOS390)
	return (signum >= 0) && (signum < NSIG) && (((signum != SIGKILL) && (signum != SIGSTOP)
		&& (signum != SIGTHCONT) && (signum != SIGTHSTOP)) || nullAction);
#else /* defined(J9ZOS390) */
	return (signum >= 0) && (signum < NSIG) && (((signum != SIGKILL) && (signum != SIGSTOP)) || nullAction);
#endif /* defined(J9ZOS390) */
}

/**
 * Manually process the flags for the registered secondary handler to call it as if it had been evoked by the OS
 * in response to a signal, without re-raising it.
 */
int
omrsig_handler(int sig, void *siginfo, void *uc)
{
	int rc = OMRSIG_RC_ERROR;
	if (validSignalNum(sig, false)) {
		/* Only mask signals and lock while getting the handler slot. */
		SIGLOCK(sigMutex);
		OMR_SigData handlerSlot = sigData[sig];
		SIGUNLOCK(sigMutex);

		if (handlerIsFunction(&handlerSlot.secondaryAction)) {
#if defined(POSIX_SIGNAL)
#if defined(OSX)
			sigset_t oldMask = {0};
#else /* defined(OSX) */
			sigset_t oldMask = {{0}};
#endif /* defined(OSX) */
			sigset_t usedMask = handlerSlot.secondaryAction.sa_mask;
			sigaddset(&usedMask, sig);
			int ec = pthread_sigmask(SIG_BLOCK, &usedMask, &oldMask);

			/* SA_NODEFER - If set, sig will not be added to the process' signal mask on entry to the signal handler
			 * unless it is included in sa_mask. Otherwise, sig will always be added to the process' signal mask on
			 * entry to the signal handler.
			 */
			if ((0 == ec) && ((handlerSlot.secondaryAction.sa_flags & SA_NODEFER)
#if (defined(AIXPPC) || defined(J9ZOS390))
				/* Only AIX and zos respects that SA_RESETHAND behaves like SA_NODEFER by POSIX spec. */
				|| (handlerSlot.secondaryAction.sa_flags & SA_RESETHAND)
#endif /* (defined(AIXPPC) || defined(J9ZOS390)) */
				)) {
#if defined(OSX)
				sigset_t mask = {0};
#else /* defined(OSX) */
				sigset_t mask = {{0}};
#endif /* defined(OSX) */
				sigemptyset(&mask);
				sigaddset(&mask, sig);
				ec = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
			}


			/* SA_RESETHAND - If set, the disposition of the signal will be reset to SIG_DFL and the SA_SIGINFO flag
			 * will be cleared on entry to the signal handler. Does not work for SIGILL or SIGTRAP, silently. Also
			 * behaves like SA_NODEFER according to POSIX standard, but AIX is the supported OS found to do this.
			 */
			if (0 == ec) {
				sighandler_t handler = SIG_DFL;
				sigaction_t action = (sigaction_t)SIG_DFL;
				int flags = handlerSlot.secondaryAction.sa_flags;
				if (flags & SA_SIGINFO) {
					action = handlerSlot.secondaryAction.sa_sigaction;
				} else {
					handler = handlerSlot.secondaryAction.sa_handler;
				}
				if (handlerSlot.secondaryAction.sa_flags & SA_RESETHAND) {
					handlerSlot.secondaryAction.sa_flags &= ~SA_SIGINFO;
					handlerSlot.secondaryAction.sa_handler = SIG_DFL;
					omrsig_sigaction_internal(sig, &handlerSlot.secondaryAction, NULL, false);
				}

				/* SA_SIGINFO - The signal handler takes three arguments, not one. In this case, sa_sigaction should be
				 * used instead of sa_handler.
				 * NOTE FOR NOW: null, DFL, and IGN check the sa_handler when registering it, not here
				 */
				if (flags & SA_SIGINFO) {
					action(sig, (siginfo_t *)siginfo, uc);
				} else {
					handler(sig);
				}
				ec = pthread_sigmask(SIG_SETMASK, &oldMask, NULL);
			}
			if (0 == ec) {
				rc = OMRSIG_RC_SIGNAL_HANDLED;
			}
#else /* defined(POSIX_SIGNAL) */
			handlerSlot.secondaryAction.sa_handler(sig);
			rc = OMRSIG_RC_SIGNAL_HANDLED;
#endif /* defined(POSIX_SIGNAL) */
		} else if (SIG_DFL == handlerSlot.secondaryAction.sa_handler){
			rc = OMRSIG_RC_DEFAULT_ACTION_REQUIRED;
		}
	}
	return rc;
}

sighandler_t
omrsig_primary_signal(int signum, sighandler_t handler)
{
	struct sigaction act = {{0}};
	struct sigaction oldact = {{0}};
	act.sa_handler = handler;
#if defined(POSIX_SIGNAL)
	/* Add necessary flags to emulate signal() behavior using sigaction(). */
	act.sa_flags = SA_NODEFER | SA_RESTART;
	sigemptyset(&act.sa_mask);
#endif /* defined(POSIX_SIGNAL) */
	sighandler_t ret = SIG_DFL;
	if (0 == omrsig_sigaction_internal(signum, &act, &oldact, true)) {
		ret = oldact.sa_handler;
	} else {
		ret = SIG_ERR;
	}
	return ret;
}

#if defined(POSIX_SIGNAL)
int
omrsig_primary_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	return omrsig_sigaction_internal(signum, act, oldact, true);
}
#endif /* defined(POSIX_SIGNAL) */

#if defined(WIN32)
#pragma warning(push)
#pragma warning(disable : 4273)
void (__cdecl * __cdecl
signal(_In_ int signum, _In_opt_ void (__cdecl * handler)(int)))(int)
#else /* defined(WIN32) */
sighandler_t
signal(int signum, sighandler_t handler) __THROW
#endif /* defined(WIN32) */
{
	return omrsig_signal_internal(signum, handler);
}
#if defined(WIN32)
#pragma warning(pop)
#endif /* defined(WIN32) */

static sighandler_t
omrsig_signal_internal(int signum, sighandler_t handler)
{
	struct sigaction act = {{0}};
	struct sigaction oldact = {{0}};
	oldact.sa_handler = SIG_DFL;
	act.sa_handler = handler;
#if defined(POSIX_SIGNAL)
	act.sa_flags = SA_NODEFER | SA_RESETHAND;
	sigemptyset(&act.sa_mask);
#endif /* defined(POSIX_SIGNAL) */

	sighandler_t ret = SIG_DFL;
#if defined(WIN32)
	if (SIG_GET == handler) {
		if (0 == omrsig_sigaction_internal(signum, NULL, &oldact, false)) {
			ret = oldact.sa_handler;
		} else {
			ret = SIG_ERR;
		}
	} else {
#endif /* defined(WIN32) */
		if (0 == omrsig_sigaction_internal(signum, &act, &oldact, false)) {
			ret = oldact.sa_handler;
		} else {
			ret = SIG_ERR;
		}
#if defined(WIN32)
	}
#endif /* defined(WIN32) */
	return ret;
}

#if defined(POSIX_SIGNAL)
int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) __THROW
{
	return omrsig_sigaction_internal(signum, act, oldact, false);
}
#endif /* defined(POSIX_SIGNAL) */

int
omrsig_signalOS_internal(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	int rc = 0;
#if defined(J9ZOS390)
	/* zos does not seem to allow dlopen with NULL and no dll name. Use pragma map to SIGACT instead.*/
	rc = sigactionOS(signum, act, oldact);
#elif defined(POSIX_SIGNAL)
	/* Find the system implementation of sigaction on first call. */
	if (NULL == sigactionOS) {
		sigactionOS = (SIGACTION)dlsym(RTLD_NEXT, "sigaction");
		if (NULL == sigactionOS) {
			char *lib = NULL;
#if defined(AIXPPC)
#if defined(OMR_ENV_DATA64)
			lib = "/usr/lib/libc.a(shr_64.o)";
#else /* defined(OMR_ENV_DATA64) */
			lib = "/usr/lib/libc.a(shr.o)";
#endif /* defined(OMR_ENV_DATA64) */
			void *handle = dlopen(lib, RTLD_LAZY | RTLD_MEMBER);
#else /* defined(AIXPPC) */
			void *handle = dlopen(lib, RTLD_LAZY);
#endif /* defined(AIXPPC) */
			sigactionOS = (SIGACTION)dlsym(handle, "sigaction");
		}
	}
	if (NULL == sigactionOS) {
		rc = -1;
	} else {
		rc = sigactionOS(signum, act, oldact);
	}
#else /* defined(POSIX_SIGNAL) */
	if (NULL == signalOS) {
		signalOS = (SIGNAL)GetProcAddress(GetModuleHandle(TEXT(MSVC_RUNTIME_DLL)), "signal");
	}
	if (NULL == signalOS) {
		rc = -1;
	} else {
		sighandler_t old = signalOS(signum, act->sa_handler);
		if (SIG_ERR == old) {
			rc = -1;
		} else if (NULL != oldact) {
			oldact->sa_handler = old;
		}
	}
#endif /* defined(POSIX_SIGNAL) */
	return rc;
}

static int
omrsig_sigaction_internal(int signum, const struct sigaction *act, struct sigaction *oldact, bool primary)
{
	int rc = 0;
	/* Check for valid signum. */
	if (!validSignalNum(signum, (NULL == act) || (!handlerIsFunction(act)))) {
		rc = -1;
		errno = EINVAL;
	} else {
		/* Get signal slot. */
		struct sigaction *savedAction = NULL;
		if (primary) {
			savedAction = &sigData[signum].primaryAction;
		} else {
			savedAction = &sigData[signum].secondaryAction;
		}

		SIGLOCK(sigMutex);
		/* Get previous handler from slot. */
		if (NULL != oldact) {
			*oldact = *savedAction;
		}

		if (NULL != act) {
			/* Copy handler to be installed into slot. */
			*savedAction = *act;

			/* Primary handler cannot support the SA_RESETHAND flag as it is not possible
			 * to reinstall the secondary handler before entering the primary handler function.
			 */
#if defined(POSIX_SIGNAL)
			if (primary) {
				savedAction->sa_flags &= ~SA_RESETHAND;
			}
#endif /* defined(POSIX_SIGNAL) */

			/* Register primary with OS if it exists, otherwise register the secondary. */
			struct sigaction newRegisteringHandler;
			if (handlerIsFunction(&sigData[signum].primaryAction)) {
				/* If secondary has SA_ONSTACK/NOCLDSTOP/NOCLDWAIT flag and primary does not, add those flags. */
				newRegisteringHandler = sigData[signum].primaryAction;
#if defined(POSIX_SIGNAL)
				newRegisteringHandler.sa_flags |= sigData[signum].secondaryAction.sa_flags & SECONDARY_FLAGS_WHITELIST;
#endif /* defined(POSIX_SIGNAL) */
			} else {
				newRegisteringHandler = sigData[signum].secondaryAction;
			}
#if defined(J9ZOS390)
			/* On zos, when an alt signal stack is set, the second call to pthread_sigmask() or
			 * sigprocmask() within a signal handler will cause the program to sig kill itself.
			 */
			newRegisteringHandler.sa_flags &= ~SA_ONSTACK;
#endif /* defined(J9ZOS390) */
			rc = omrsig_signalOS_internal(signum, &newRegisteringHandler, NULL);
		}
		SIGUNLOCK(sigMutex);
	}
	return rc;
}

#if defined(LINUX)

__sighandler_t
__sysv_signal(int sig, __sighandler_t handler) __THROW
{
	return omrsig_signal_internal(sig, handler);
}

sighandler_t
ssignal(int sig, sighandler_t handler) __THROW
{
	return omrsig_signal_internal(sig, handler);
}

#endif /* defined(LINUX) */

#if defined(J9ZOS390)

static bool
checkMaskEquality(int signum, const sigset_t *mask, const sigset_t *otherMask)
{
	for (unsigned int i = 1; i < NSIG; i += 1) {
		if (sigismember(mask, signum) != sigismember(otherMask, signum)) {
			return false;
		}
	}
	return true;
}

int __sigactionset(size_t newct, const __sigactionset_t newsets[], size_t *oldct, __sigactionset_t oldsets[], int options)
{
	if ((newct > 64) && (NULL != newsets)) {
		errno = EINVAL;
		goto failed;
	} else {
		/* First, check for attempts to set an invalid signal. If error, change no handlers. */
		if (!(__SSET_IGINVALID & options)) {
			for (unsigned int i = 0; i < newct; i += 1) {
				for (unsigned int j = 0; j < NSIG; j += 1) {
					struct sigaction act = {{0}};
					act.sa_handler = newsets[i].__sa_handler;
					act.sa_sigaction = newsets[i].__sa_sigaction;
					act.sa_flags = newsets[i].__sa_flags;
					if (1 == sigismember(&newsets[i].__sa_signals, j)
						&& !validSignalNum(j, !handlerIsFunction(&act))) {
						errno = EINVAL;
						goto failed;
					}
				}
			}
		}

		/* Get the old handlers before changes are made. */
		SIGLOCK(sigMutex);
		if (NULL != oldsets) {
			unsigned int oldctFreeIndex = 0;
			for (unsigned int i = 0; i < NSIG; i += 1) {
				if (validSignalNum(i, false)) {
					unsigned int oldctIndex = 0;
					while(((sigData[i].secondaryAction.sa_handler != oldsets[oldctIndex].__sa_handler)
						|| !checkMaskEquality(i, &sigData[i].secondaryAction.sa_mask, &oldsets[oldctIndex].__sa_mask)
						|| (sigData[i].secondaryAction.sa_flags != oldsets[oldctIndex].__sa_flags))
						&& (oldctIndex < oldctFreeIndex)) {
						oldctIndex += 1;
					}
					if (oldctIndex < *oldct) {
						if (oldctIndex == oldctFreeIndex) {
							oldctFreeIndex += 1;
							oldsets[oldctIndex].__sa_handler = sigData[i].secondaryAction.sa_handler;
							oldsets[oldctIndex].__sa_mask = sigData[i].secondaryAction.sa_mask;
							oldsets[oldctIndex].__sa_flags = sigData[i].secondaryAction.sa_flags;
							oldsets[oldctIndex].__sa_sigaction = sigData[i].secondaryAction.sa_sigaction;
						}
						sigaddset(&oldsets[oldctIndex].__sa_signals, i);
					} else {
						errno = ENOMEM;
						SIGUNLOCK(sigMutex);
						goto failed;
					}
				}
			}
			*oldct = oldctFreeIndex;
		}
		SIGUNLOCK(sigMutex);

		/* Set new handlers. */
		for (unsigned int i = 0; i < newct; i += 1) {
			for (unsigned int j = 0; j < NSIG; j += 1) {
				if (1 == sigismember(&newsets[i].__sa_signals, j)) {
					struct sigaction act = {{0}};
					act.sa_handler = newsets[i].__sa_handler;
					act.sa_mask = newsets[i].__sa_mask;
					act.sa_flags = newsets[i].__sa_flags;
					act.sa_sigaction = newsets[i].__sa_sigaction;
					if ((0 != omrsig_sigaction_internal(j, &act, NULL, false)) && !(__SSET_IGINVALID & options)) {
						errno = EINVAL;
						goto failed;
					}
				}
			}
		}
	}
	return 0;

failed:
	return -1;
}

#endif /* defined(J9ZOS390) */

#if !defined(WIN32)

sighandler_t
sigset(int sig, sighandler_t disp) __THROW
{
	sighandler_t ret = SIG_ERR;
#if defined(OSX)
	sigset_t mask = {0};
	sigset_t oldmask = {0};
#else /* defined(OSX) */
	sigset_t mask = {{0}};
	sigset_t oldmask = {{0}};
#endif /* defined(OSX) */
	struct sigaction oldact = {{0}};

	if (SIG_HOLD == disp) {
		/* If disp is SIG_HOLD, mask the signal and return previous. */
		sigemptyset(&mask);
		sigaddset(&mask, sig);
		pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
		if (0 != omrsig_sigaction_internal(sig, NULL, &oldact, false)) {
			ret = SIG_ERR;
		}
	} else {
		/* Otherwise, set handler to disp. */
		struct sigaction act = {{0}};
		act.sa_handler = disp;
		sigemptyset(&act.sa_mask);
		if (0 != omrsig_sigaction_internal(sig, &act, &oldact, false)) {
			ret = SIG_ERR;
		} else {
			sigemptyset(&mask);
			sigaddset(&mask, sig);
			if (0 != pthread_sigmask(SIG_UNBLOCK, &mask, &oldmask)) {
				ret = SIG_ERR;
			}
		}
	}

	/* If signal is blocked before the call, return SIG_HOLD. */
	if (SIG_ERR != ret) {
		if (1 == sigismember(&oldmask, sig)) {
			ret = SIG_HOLD;
		} else {
			ret = oldact.sa_handler;
		}
	}
	return ret;
}

int
sigignore(int sig) __THROW
{
	/* Unregister the secondary handler for sig if there is one. */
	SIGLOCK(sigMutex);
	sigData[sig].secondaryAction.sa_handler = SIG_IGN;
	SIGUNLOCK(sigMutex);

	return 0;
}

sighandler_t
bsd_signal(int signum, sighandler_t handler) __THROW
{
	struct sigaction act = {{0}};
	struct sigaction oldact = {{0}};
	oldact.sa_handler = SIG_DFL;
	act.sa_handler = handler;
	act.sa_flags = SA_RESTART;
	sigemptyset(&act.sa_mask);

	sighandler_t ret = SIG_DFL;
	if (0 == omrsig_sigaction_internal(signum, &act, &oldact, false)) {
		ret = oldact.sa_handler;
	} else {
		ret = SIG_ERR;
	}
	return ret;
}

#if !defined(J9ZOS390)

sighandler_t
sysv_signal(int signum, sighandler_t handler) __THROW
{
	return omrsig_signal_internal(signum, handler);
}

#endif /* !defined(J9ZOS390) */
#endif /* !defined(WIN32) */

} /* extern "C" { */
