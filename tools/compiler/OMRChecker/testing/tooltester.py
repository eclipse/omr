###############################################################################
# Copyright (c) 2016, 2016 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

import testing.tool


class AssertFailure(Exception):
   '''Class representing an assertion failure in a test case.'''
   def __init__(self, msg=None):
      self.msg = msg


class TestCase(object):
   '''Base class for a test case.'''

   def __init__(self, name, tool):
      self.name = name
      self.tool = tool
      self.log = []

   def invokeTool(self, args):
      logEntry = self.tool.call(args)
      self.log += [logEntry]
      return logEntry.output

   def assertTrue(self, condition, msg=None):
      if not condition:
         raise AssertFailure(msg)
         
   def assertFalse(self, condition, msg=None):
      if condition:
         raise AssertFailure(msg)

   def assertEqual(self, expected, actual, msg=None):
      self.assertTrue(expected == actual, (msg if msg is not None else "" ) + " [ " + str(expected) + " != " + str(actual) + " ]")

   def assertNotEqual(self, expected, actual, msg=None):
      self.assertTrue(expected != actual, (msg if msg is not None else "" ) + " [ " + str(expected) + " == " + str(actual) + " ]")

   def assertOutput(self, predicate, msg=None):
      self.assertTrue(predicate(self.log[-1].output) , msg)

   def run(self):
      pass


class TestRunner(object):
   '''Class for running a test case.'''

   def __init__(self, test, mcommand=1, mreturn=1, mstderr=1, mstdout=1):
      self.test = test
      self.mcommand = mcommand
      self.mreturn = mreturn
      self.mstderr = mstderr
      self.mstdout = mstdout
      self.testPassed = True

   def runTest(self):
      try:
         self.test.run()
         print("Test " + self.test.name + " PASSED.")
      except AssertFailure as e:
         self.testPassed = False
         self.printFailure(e.msg)

      self.printLog()
      return self.testPassed

   def printFailure(self, msg):
      if msg is not None:
         print("Test " + self.test.name + " FAILED: " + msg)
      else:
         print("Test " + self.test.name + " FAILED.")

   def printLog(self):
      outputLevel = 2  if self.testPassed else 1
      for logEntry in self.test.log:
         if self.mcommand >= outputLevel:
            print('>Command: ' + logEntry.cmdstr)
         if self.mreturn >= outputLevel:
            print('>Return Code: ' + str(logEntry.output.returncode))
         if self.mstderr >= outputLevel:
            print('>Standard Error:\n' + logEntry.output.stderr.decode("utf-8"))
         if self.mstdout >= outputLevel:
            print('>StandardOutput:\n' + logEntry.output.stdout.decode("utf-8"))


class TestSuite(object):
   '''Base class for a suite of test cases. All tests cases must use the same tool.'''

   def __init__(self, tool, tests=[]):
      self.tool = tool
      self.tests = tests
      for test in tests:
         test.tool = self.tool

   def addTest(self, test):
      test.tool = self.tool
      self.tests += test

   def getTests(self):
      for test in self.tests:
         yield test


class SuiteRunner(object):
   '''Class for running all tests in a test suite.'''

   def __init__(self, testSuite, mcommand=1, mreturn=1, mstderr=1, mstdout=1):
      self.testSuite = testSuite
      self.mcommand = mcommand
      self.mreturn = mreturn
      self.mstderr = mstderr
      self.mstdout = mstdout
      self.testsRun = 0
      self.testsFailed = 0

   def runTests(self):
      for test in self.testSuite.getTests():
         runner = TestRunner(test, self.mcommand, self.mreturn, self.mstderr, self.mstdout)
         passed = runner.runTest()
         self.testsRun += 1
         if not passed:
            self.testsFailed += 1

   def printSummary(self):
      print("Total tests: " + str(len(self.testSuite.tests)))
      print("Total run: " + str(self.testsRun))
      print("Tests passed: " + str(self.testsRun -  self.testsFailed))
      print("Tests failed: " + str(self.testsFailed))


def addTestArgs(argParser):
   '''Adds command line arguments specific for this test framework to an argument parser.'''

   argParser.add_argument('--mcommand', dest='MCOMMAND', type=int, default=1)
   argParser.add_argument('--mreturn', dest='MRETURN', type=int, default=1)
   argParser.add_argument('--mstderr', dest='MSTDERR', type=int, default=1)
   argParser.add_argument('--mstdout', dest='MSTDOUT', type=int, default=1)
