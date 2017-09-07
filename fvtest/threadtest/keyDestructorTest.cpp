/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "threadTestLib.hpp"

#include "omrTest.h"
#include "testHelper.hpp"

#if !defined(WIN32) && !defined(J9ZOS390)
#include <pthread.h>
#include <stdlib.h>
#endif /* !defined(WIN32) && !defined(J9ZOS390) */

extern ThreadTestEnvironment *omrTestEnv;

class KeyDestructorTest: public ::testing::Test {
public:
	static bool completed;
#if !defined(WIN32) && !defined(J9ZOS390)
	static pthread_key_t key;

	static void detachThread(void *p);
	static void *threadproc(void *p);
#endif /* !defined(WIN32) && !defined(J9ZOS390) */
protected:

	static void
	SetUpTestCase(void)
	{
	}
};

bool KeyDestructorTest::completed = false;

#if !defined(WIN32) && !defined(J9ZOS390)
pthread_key_t KeyDestructorTest::key;

void
KeyDestructorTest::detachThread(void *p)
{
	omrthread_t old_self = (omrthread_t)p;
	omrthread_t self = omrthread_self();

	/* omrthread_self relies on a tls key. Verify that it returns the right value. */
	if (self == old_self) {
		/* If they are equal detach the thread and mark the test as successful */
		omrthread_detach(self);
		KeyDestructorTest::completed = true;
	} else {
		omrTestEnv->log(LEVEL_ERROR, "Error old_self %p != omrthread_self() %p\n", old_self, self);
		fflush(stdout);
	}
}

void *
KeyDestructorTest::threadproc(void *p)
{
	omrthread_t self = NULL;
	intptr_t attachRC = 0;
	int rc = 0;

	attachRC = omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);
	if (0 == attachRC) {
		/* set the key value to the omrthread for this thread */
		rc = pthread_setspecific(KeyDestructorTest::key, (void *)self);
		if (rc != 0) {
			omrTestEnv->log(LEVEL_ERROR, "Error setting key to self %d\n", rc);
		}
	} else {
		omrTestEnv->log(LEVEL_ERROR, "Error attaching to omrthread library %d\n", attachRC);
	}

	/* pthread_exit to test the key destructor */
	pthread_exit(NULL);

	/* will never get here.  Just to stop compiler warning */
	return NULL;
}
#endif /* !defined(WIN32) && !defined(J9ZOS390) */

TEST_F(KeyDestructorTest, KeyDestructor)
{
#if defined(WIN32) || defined(J9ZOS390)
	completed = true;
#else
	pthread_t thread;
	int rc = 0;

	rc = pthread_key_create(&key, detachThread);
	ASSERT_EQ(0, rc);

	rc = pthread_create(&thread, NULL, threadproc, NULL);
	ASSERT_EQ(0, rc);

	pthread_join(thread, NULL);

	rc = pthread_key_delete(key);
	ASSERT_EQ(0, rc);
#endif /* !defined(WIN32) && !defined(J9ZOS390) */
	ASSERT_TRUE(completed);
}
