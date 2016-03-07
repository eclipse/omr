/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

#if !defined(SPINLIMITER_HPP_)
#define SPINLIMITER_HPP_

#include "omrcfg.h"
#include "EnvironmentBase.hpp"

/*
 * Use Watchdog model:
 * - counter is decrementing
 * - from MAXIMUM_LOOPS_THRESHOLD to FREE_LOOPS_THRESHOLD spin without extra delay
 * - from FREE_LOOPS_THRESHOLD to 0 spin with dealy
 * - stop spinning if counter reach 0
 */

/* Maximum number of sequential spins before reading clock */
#define FREE_LOOPS_THRESHOLD	10000

/* Maximum number of sequential spins between reading clock */
#define FREE_LOOPS_BETWEEN_THRESHOLD	10000

/* Maximum allowed spin time in milliseconds */
#define MAX_SPIN_TIME_MILLIS	100

class MM_SpinLimiter
{
public:

protected:

private:

MM_EnvironmentBase* _env; /**< thread environment */
U_64  _startTime; /**< spinning start time (except time of first pre-spin) */
UDATA _counter; /**< number of sequential loops */

public:

	/**
	 * Allow run to continue until spinning time exceeds time threshold
	 *
	 * @return true if spinning can be continued
	 */
	MMINLINE bool
	spin()
	{
		bool result = true;
		if (_counter > 0) {
			_counter -= 1;
		} else {
			OMRPORT_ACCESS_FROM_OMRPORT(_env->getPortLibrary());
			U_64 time = omrtime_hires_clock();
			if (0 == _startTime) {
				_startTime = time;
				_counter = FREE_LOOPS_BETWEEN_THRESHOLD;
			} else {
				if (MAX_SPIN_TIME_MILLIS <= (UDATA)omrtime_hires_delta(_startTime, time, OMRPORT_TIME_DELTA_IN_MILLISECONDS)) {
					result = false;
				} else {
					_counter = FREE_LOOPS_BETWEEN_THRESHOLD;
				}
			}
		}
		return result;
	}

	MMINLINE void
	reset()
	{
		_counter = FREE_LOOPS_THRESHOLD;
		_startTime = 0;
	}

	MMINLINE MM_SpinLimiter(MM_EnvironmentBase* env) :
		_env(env)
		, _startTime(0)
		, _counter(FREE_LOOPS_THRESHOLD)
	{

	}

protected:

private:

};
#endif /* SPINLIMITER_HPP_ */
