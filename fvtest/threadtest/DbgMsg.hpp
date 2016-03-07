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


#ifndef DBGMSG_HPP_INCLUDED
#define DBGMSG_HPP_INCLUDED

namespace DbgMsg
{
void print(const char *pMsg, ...);
void println(const char *pMsg, ...);
void printRaw(const char *pMsg, ...);
void changeIndent(int delta);

void setVerboseLevel(int level);
void verbosePrint(const char *pMsg, ...);
};

#endif /* DBGMSG_HPP_INCLUDED */
