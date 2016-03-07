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


#ifndef SANITYTESTHELPER_HPP_INCLUDED
#define SANITYTESTHELPER_HPP_INCLUDED

#include "threadTestLib.hpp"

bool SimpleSanity(void);
void SanityTestNThreads(unsigned int, unsigned int);
bool TestNThreadsLooping(unsigned int, unsigned int, unsigned int, bool);
void QuickNDirtyPerformanceTest(unsigned int);
bool TestBlockingQueue(CThread& self, const unsigned int numThreads);
bool TestWaitNotify(unsigned int);

#endif /* SANITYTESTHELPER_HPP_INCLUDED */
