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

import os
import sys
import json

def genFileList(dataDir, pred=None):
   if pred is None:
      return [os.path.join(dirpath, fileName) for dirpath, dirs, files in os.walk(dataDir) for fileName in files]
   else:
      return [os.path.join(dirpath, fileName) for dirpath, dirs, files in os.walk(dataDir) for fileName in files if pred(fileName)]

if __name__ == "__main__":

   fileList = genFileList(os.path.join(os.getcwd(), sys.argv[1]))

   f = open(sys.argv[1].replace('/','_') + ".json", "w")
   f.write(json.dumps(fileList, indent=3, separators=(',', ': ')))
   f.close()
