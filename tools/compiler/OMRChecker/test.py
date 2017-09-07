#! /usr/bin/python

###############################################################################
# Copyright (c) 2016, 2016 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code is also Distributed under one or more Secondary Licenses,
# as those terms are defined by the Eclipse Public License, v. 2.0: GNU
# General Public License, version 2 with the GNU Classpath Exception [1]
# and GNU General Public License, version 2 with the OpenJDK Assembly
# Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# Contributors:
#   Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

import sys
import os
import argparse
import testing.gen_data as gen_data
import testing.tool as tool
import testing.tooltester as tooltester

# argument handling - general flags
arg_parser = argparse.ArgumentParser(description='Test the OMR extensible class checker.')
arg_parser.add_argument('--checker', dest='CHECKER', type=str, default=os.path.join(os.getcwd(), 'OMRChecker.so'))
tooltester.addTestArgs(arg_parser)


class OMRChecker(tool.Tool):
   '''A wrapper providing an interface for interacting with OMRChecker.'''

   def __init__(self, checker):
      base = ['clang++', '-fsyntax-only', '-Xclang', '-load', '-Xclang', checker, '-Xclang', '-add-plugin', '-Xclang', 'omr-checker'] 
      super(OMRChecker, self).__init__(lambda args: base + args)


class CheckerTestCase(tooltester.TestCase):
   def __init__(self, inputFilePath, checker):
      super(CheckerTestCase, self).__init__('[' + inputFilePath  + ']', checker)
      self.inputFilePath = inputFilePath
   
   def invokeChecker(self, filePath):
      return self.invokeTool([filePath])


class TestReturnZero(CheckerTestCase):
   def __init__(self, inputFilePath, checker=None):
      super(TestReturnZero, self).__init__(inputFilePath, checker)

   def run(self):
      self.invokeChecker(self.inputFilePath)
      self.assertOutput(lambda output: 0 == output.returncode, 'return code was not zero.')


class TestReturnNotZero(CheckerTestCase):
   def __init__(self, inputFilePath, checker=None):
      super(TestReturnNotZero, self).__init__(inputFilePath, checker)

   def run(self):
      self.invokeChecker(self.inputFilePath)
      self.assertOutput(lambda output: 0 != output.returncode, 'return code was zero.')


if __name__ == '__main__':
   args = arg_parser.parse_args(sys.argv[1:])
   args = vars(args)
   
   if not os.path.exists(args['CHECKER']):
      print("Could not find: " + args['CHECKER'])
      exit(1)

   checker = OMRChecker(os.path.abspath(args['CHECKER']))

   goodFiles = gen_data.genFileList('testing/input/good')
   badFiles = gen_data.genFileList('testing/input/bad')
   tests = [TestReturnZero(inputFilePath) for inputFilePath in goodFiles] + [TestReturnNotZero(inputFilePath) for inputFilePath in badFiles]

   testSuite = tooltester.TestSuite(checker, tests)
   runner = tooltester.SuiteRunner(testSuite, args['MCOMMAND'], args['MRETURN'], args['MSTDERR'], args['MSTDOUT'])
   runner.runTests()

   runner.printSummary()
   if runner.testsFailed != 0:
      exit(1)
