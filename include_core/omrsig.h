/*******************************************************************************
 * Copyright IBM Corp. and others 2015
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

#include <signal.h>
#if defined(OMR_OS_WINDOWS)
/* windows.h defined UDATA.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#endif /* defined(OMR_OS_WINDOWS) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(OSX)
#define __THROW
#endif /* defined(OSX) */

#if defined(LINUXPPC)
typedef __sighandler_t sighandler_t;
#elif defined(LINUX) || defined(OSX)
typedef void (*sighandler_t)(int sig);
#elif defined(J9ZOS390) || defined(AIXPPC)
typedef void (*sighandler_t)(int sig);
#if !defined(__GNUC__)
#define __THROW
#endif
#elif defined(OMR_OS_WINDOWS)
typedef void (__cdecl *sighandler_t)(int sig);
#define __THROW
#endif /* defined(OMR_OS_WINDOWS) */

#define OMRSIG_RC_ERROR -1
#define OMRSIG_RC_SIGNAL_HANDLED 0
#define OMRSIG_RC_DEFAULT_ACTION_REQUIRED 1

/**
 * Calls the registered secondary signal handler, only if both a primary and secondary are
 * registered, as if it were invoked directly by the OS in response to a caught signal.
 * Secondary handlers support only the SA_RESETHAND, SA_SIGINFO, and SA_NODEFER flags.
 * Secondary handlers will inherit the masked signals of the current primary handler.
 *
 * @param[in] sig The signal number
 * @param[in] siginfo siginfo_t* passed into the primary handler by the system.
 * @param[in] uc ucontext_t* passed into the primary handler by the system.
 * @return an int error code
 *		SIGNAL_HANDLED - If caller should not continue to do any more signal handling.
 *		DEFAULT_ACTION_REQUIRED - If caller needs to call default action (SIG_DFL) for this signal.
 */
int omrsig_handler(int sig, void *siginfo, void *uc);

/**
 * Register a primary signal handler function, emulating the behavior, parameters, and return of signal().
 *
 * @param[in] signum Signal number
 * @param[in] handler Signal hanlder function to register
 * @return
 *		Previous value of the signal handler if successful
 *		SIG_ERR with errno set to EINVAL if signum is invalid.
 */
sighandler_t omrsig_primary_signal(int signum, sighandler_t handler);

#if defined(OMR_OS_WINDOWS)

_CRTIMP void (__cdecl * __cdecl signal(_In_ int _SigNum, _In_opt_ void (__cdecl * _Func)(int)))(int);

#else /* defined(OMR_OS_WINDOWS) */

/**
 * Register a primary signal handler function, emulating the behavior, parameters, and return of sigaction().
 * Primary handlers only support the SA_SIGINFO and SA_NODEFER flags.
 *
 * @param[in] signum Signal number
 * @param[in] act Structure containing flags, mask, and signal handler function
 * @param[out] oldact Previously registered signal handler struct
 * @return an int error code
 *		0 on success
 *		-1 on failure, with errno set to EINVAL if the sig argument is not a valid signal number or an attempt is made to catch a signal that cannot be caught or ignore a signal that cannot be ignored.
 */
int omrsig_primary_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) __THROW;
sighandler_t signal(int signum, sighandler_t handler) __THROW;

sighandler_t sigset(int sig, sighandler_t disp) __THROW;
int sigignore(int sig) __THROW;
sighandler_t bsd_signal(int signum, sighandler_t handler) __THROW;
#if !defined(J9ZOS390)
sighandler_t sysv_signal(int signum, sighandler_t handler) __THROW;
#endif /* !defined(J9ZOS390) */
#if defined(LINUX)
__sighandler_t __sysv_signal(int sig, __sighandler_t handler) __THROW;
sighandler_t ssignal(int sig, sighandler_t handler) __THROW;
#endif /* defined(LINUX) */


#endif /* !defined(OMR_OS_WINDOWS) */

#if defined(J9ZOS390)
int __sigactionset(size_t newct, const __sigactionset_t newsets[], size_t *oldct, __sigactionset_t oldsets[], int options);
#endif /* defined(J9ZOS390) */

#ifdef __cplusplus
} /* extern "C" { */
#endif  /* __cplusplus */
