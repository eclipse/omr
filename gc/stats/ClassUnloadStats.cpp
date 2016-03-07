/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include "ClassUnloadStats.hpp"

void
MM_ClassUnloadStats::clear()
{
	_classLoaderUnloadedCount = 0;
	_classLoaderCandidates = 0;
	_classesUnloadedCount = 0;
	_anonymousClassesUnloadedCount = 0;
	_startTime = 0;
	_endTime = 0;
	_startSetupTime = 0;
	_endSetupTime = 0;
	_startScanTime = 0;
	_endScanTime = 0;
	_startPostTime = 0;
	_endPostTime = 0;
	_classUnloadMutexQuiesceTime = 0;
};
