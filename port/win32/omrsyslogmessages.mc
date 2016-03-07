;/*******************************************************************************
; *
; * (c) Copyright IBM Corp. 1991, 2008
; *
; *  This program and the accompanying materials are made available
; *  under the terms of the Eclipse Public License v1.0 and
; *  Apache License v2.0 which accompanies this distribution.
; *
; *      The Eclipse Public License is available at
; *      http://www.eclipse.org/legal/epl-v10.html
; *
; *      The Apache License v2.0 is available at
; *      http://www.opensource.org/licenses/apache2.0.php
; *
; * Contributors:
; *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
; *******************************************************************************/


; // This is the header section.

MessageIdTypedef=DWORD

SeverityNames=(InfoMsg=0x0:STATUS_SEVERITY_INFORMATIONAL
    WarningMsg=0x1:STATUS_SEVERITY_WARNING
    ErrorMsg=0x2:STATUS_SEVERITY_ERROR
)

LanguageNames=(Any=1:MSG00001)

; // The following are message definitions.

MessageId=0x0
Severity=InfoMsg
SymbolicName=MSG_INFO
Language=Any
%1
.
MessageId=0x1
Severity=WarningMsg
SymbolicName=MSG_WARNING
Language=Any
%1
.
MessageId=0x2
Severity=ErrorMsg
SymbolicName=MSG_ERROR
Language=Any
%1
.
