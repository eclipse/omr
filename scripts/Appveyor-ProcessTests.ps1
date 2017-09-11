###############################################################################
# Copyright (c) 2015, 2017 IBM Corp. and others
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

#TODO we need to add the error message (if applicable) to the output
function Process-Tests {
    process {
        $test_status = "None"
        if($_.status -eq "run") {
            if($_.failure) {
                $test_status = "Failed"
            } else {
                $test_status = "Passed"
            }
        } elseif ($_.status -eq "notrun"){
			$test_status = "Skipped"
		}
        New-Object psobject -Property @{
            'name' = $_.ParentNode.name + "." + $_.name;
            'status' = $test_status;
            'time' = [long]([float]$_.time * 1000);
			'classname' = $_.classname
        }
    }
}

# Writes test results to appveyor dashboard
function Upload-TestResults {
	process {
		Add-AppveyorTest -Name $_.name -Framework "xUnit" -FileName $_.classname -Outcome $_.status -Duration $_.time
	}
}

$testResults = Select-Xml -Path "$env:TEST_RESULT_DIR\*.xml" //testcase | Select-Object -ExpandProperty Node | Process-Tests | Upload-TestResults
