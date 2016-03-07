***********************************************************************
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
*    Multiple authors (IBM Corp.) - initial API and implementation
*    and/or initial documentation
***********************************************************************

         TITLE 'j9csrsi_wrp.s'

* Assembler glue routine to call CSRSI (an AMODE31 service) from an
* AMODE64/31 C program.
* CSRSI is a system information service to request information about
* the machine, LPAR, and VM.  The returned information is mapped by
* DSECTs in CSRSIIDF macro.  SIV1V2V3 DSECT maps the data returned
* when MACHINE, LPAR and VM data are requested.
* See MVS Data Areas Volume 1 (ABEP-DDRCOM).
* Only one parameter is expected on input - address of a structure
* containing packed arguments to CSRSI for SIV1V2V3 DSECT.
*
* Following are the various general registers that would be used
* explicitly for passing parameters, holding entry addresses of
* service routines, and finally as base registers for establishing
* addressability inside control section.
*
R0       EQU   0      Entry address of CSRSI load module (LOAD macro)
R1       EQU   1      Input/Output - address of struct csrsi_v2v3
R2       EQU   2      Base register (USING instruction)
R3       EQU   3      Base register (USING instruction)
R6       EQU   6      Base register (USING instruction)
R8       EQU   8      Base register (XPLINK assembler prolog)
R9       EQU   9      Base register (USING instruction)
R10      EQU   10     Base register (USING instruction)
R15      EQU   15     Entry address of CSRSI service (CALL macro)
*
* SYSPARM should be set either in makefile or on command-line to
* switch between 64-bit and 31-bit XPLINK prolog or epilog.
* Branch based on setting of the 64-bit mode.
*
         AIF   ('&SYSPARM' EQ 'BIT64').JMP1
*
* Generate 31-bit XPLINK assembler prolog code with _J9CSRSI as
* the entry point:
* Number of 4-byte input parameters is 1.
* R8 is the base register for establishing addressability.
* Export this function when link edited into a DLL.
*
_J9CSRSI EDCXPRLG PARMWRDS=1,BASEREG=8,EXPORT=YES
*
* Generate code for z/Architecture level
*
         SYSSTATE ARCHLVL=2
*
* Unconditional branching after 31-bit prolog code.
*
         AGO   .JMP2
*
* Does no operation but allows branching to 64-bit XPLINK
* assembler prolog code.
*
.JMP1    ANOP
*
* Generate assembler prolog for AMODE 64 routine with _J9CSRSI as
* the 64-bit entry marker:
* Number of 8-byte input parameters is 1.
* R8 is the base register for establishing addressability.
* Mark this entry point as an exported DLL function.
*
_J9CSRSI CELQPRLG PARMWRDS=1,BASEREG=8,EXPORT=YES
*
* Generate code for z/Architecture level
*
         SYSSTATE ARCHLVL=2
*
* Set addressing mode to 31-bit.
*
         SAM31
*
* Does no operation but allows code continuation after 31-bit
* XPLINK assembler prolog code.
*
.JMP2    ANOP
_J9CSRSI ALIAS C'j9csrsi_wrp'
*
* Set the value of base registers 
*
         LR    R2,R1
         LA    R3,4095(,R1)
         LA    R9,4095(,R3)
         LA    R6,4095(,R9)
         LA    R10,4095(,R6)
*
* Establish addressability with IOARGS as the base address.
*
         USING IOARGS,R2
         USING IOARGS+4095,R3
         USING IOARGS+8190,R9
         USING IOARGS+12285,R6
         USING IOARGS+16380,R10
*
* Bring CSRSI entry into virtual storage.
* After the LOAD call, R0 contains entry point address
* of the requested load module.
* No explicit error handling routine and instead rely on
* system issuing ABEND call in the event of failure.
*
         LOAD  EP=CSRSI
*
* CALL macro expects R15 contains the entry address of
* the called program.
*
         LR    R15,R0
*
* Initiate executable form of the CALL macro.
* Pass set of addresses to CALL macro as per CSRSI
* requirements:
* (input) request code,
* (input) info area length,
* (input/output) info area, and
* (output) return code.
* Build a 4-byte entry parameter list.
* Construct a remote problem program parameter list
* using the address specified in the MF parameter.
*
         CALL  (15),(REQ,INFOAL,INFOAREA,RETC),PLIST4=YES,MF=(E,CPLIST)
*
* Relinquish control of loaded CSRSI module.
*
         DELETE EP=CSRSI
*
         DROP  R2,R3,R9,R6,R10
*
* Branch based on setting of the 64-bit mode.
*
         AIF   ('&SYSPARM' EQ 'BIT64').JMP3
*
* Generate 31-bit assembler epilog code.
*
         EDCXEPLG
*
* Unconditional branching after 31-bit epilog code.
*
         AGO   .JMP4
*
* Does no operation but allows branching to address
* mode reset instruction in 64-bit mode.
*
.JMP3    ANOP
*
* Switch address mode back to 64-bit.
*
         SAM64
*
* Terminate AMODE 64 routine by generating appropriate
* structures.
*
         CELQEPLG
*
* Does no operation but allows branching to program end.
*
.JMP4    ANOP
*
* Dummy control section for input/output arguments.
*
IOARGS   DSECT
*
* Reserve storage for CSRSI input/output arguments as
* well as space for constructing CALL parameter list.
*
REQ      DS    A                     request id
INFOAL   DS    A                     infoarea length
RETC     DS    A                     return code
INFOAREA DS    CL(X'4040')           infoarea buffer
CPLIST   CALL  ,(,,,),MF=L           4-argument parameter list
*
* End the program assembly.
*
         END
