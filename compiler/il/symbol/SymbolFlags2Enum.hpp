/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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
 ******************************************************************************/

/**************************************************************
 * NOTE: THIS FILE INTENTIONALLY DOES NOT HAVE INCLUDE GUARDS *
 **************************************************************/

ENUM_V(RelativeLabel             , 0x00000001), // Label Symbols only *+N
ENUM_V(ConstMethodType           , 0x00000002), // JSR292
ENUM_V(ConstMethodHandle         , 0x00000004), // JSR292
ENUM_V(CallSiteTableEntry        , 0x00000008), // JSR292
ENUM_V(HasAddrTaken              , 0x00000010), // used to denote that we have a loadaddr of this symbol
ENUM_V(MethodTypeTableEntry      , 0x00000020), // JSR292
ENUM_V(NotDataAddress            , 0x00000040), // isStatic only: AOT
ENUM_V(RealRegister              , 0x00000080), // RegisterSymbol is machine real register
ENUM_V(UnsafeShadow              , 0x00000100),
ENUM_V(NamedShadow               , 0x00000200),
ENUM_V(ImmutableField            , 0x00000400),
ENUM_V(PendingPush               , 0x00000800),
