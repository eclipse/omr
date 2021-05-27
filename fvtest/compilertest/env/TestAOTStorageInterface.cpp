/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include "env/AOTStorageInterface.hpp"
#include "infra/Assert.hpp"

namespace TestCompiler
{
TR::AOTStorageInterface *
TestCompiler::AOTStorageInterface::self()
   {
   return static_cast<TR::AOTStorageInterface *>(this);
   }

uint8_t* TestCompiler::AOTStorageInterface::loadEntry(const char* key)
    {
    uint8_t r = 0;
    uint8_t* ret = &r;
    std::ifstream indata("SharedFile");
    if(!indata) 
        {
        perror("Error opening file.");
        return ret;
        }
    for(std::string str; getline(indata, str);) 
        {
        int n = str.length(); 
        char arr[n + 1],arr_key[n + 1]; 
        int position[2];
        int dataLen = 0, keyLen = 0, start = 0, end = 0, j = 0, k = 0;
        strcpy(arr, str.c_str());
        strcpy(arr_key, str.c_str());
        const char *token = std::strtok(arr_key, "|");
        std::string keys = key;
        if(&token[0] == keys)
            {
            for(int i = 0; i<n; i++)
                {
                if(arr[i] == '|')
                    {
                    position[j] = i;
                    j++;    
                    }
                }
            dataLen = position[1] - position[0] - 1;
            uint8_t* arrayC = new uint8_t[dataLen];
            start = position[0] + 1;
            end = position[1] - 1;
            for(int i = start; i <= end; i++)
                {
                arrayC[k] = arr[i]; 
                k++; 
                }
            return arrayC;
            }
        }
    return ret;
   }

void TestCompiler::AOTStorageInterface::storeEntry(const char* key, void* data, uint8_t size)
    {
    FILE *appFile;
    std::string key_str = key;
    std::string data_str =  std::to_string(*(uint8_t *)data);
    std::string merged_str = key_str + "|" + data_str + "|" + std::to_string(size) + "\n";
    appFile = fopen("SharedFile", "a");
    if(appFile == NULL) 
        {
        perror("Error opening file.");
        }
    else
        {
        fprintf(appFile, "%s", merged_str.c_str());
        }
    fclose(appFile);
   }
}
   
