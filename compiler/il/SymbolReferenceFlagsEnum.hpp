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

ENUM_V(Unresolved                        , 0x00000001),
ENUM_V(CanGCandReturn                    , 0x00000002),
ENUM_V(CanGCandExcept                    , 0x00000004),
ENUM_V(ReallySharesSymbol                , 0x00000008),
ENUM_V(StackAllocatedArrayAccess         , 0x00000010),
ENUM_V(SideEffectInfo                    , 0x00000020),
ENUM_V(LiteralPoolAddress                , 0x00000040),
ENUM_V(FromLiteralPool                   , 0x00000080),
ENUM_V(OverriddenBitAddress              , 0x00000100),
ENUM_V(InitMethod                        , 0x00000200), ///< J9
ENUM_V(TempVariableSizeSymRef            , 0x00000400),
ENUM_V(Adjunct                           , 0x00000800), ///< auto symbol represents the adjunct part of the dual symbol
ENUM_V(Dual                              , 0x00001000), ///< auto symbol represents a dual symbol consisting of two parts
ENUM_V(TemporaryNegativeOffset           , 0x00002000),
ENUM_V(HoldsMonitoredObjectForSyncMethod , 0x00004000),
ENUM_V(AccessedAtRuntimeBase             , 0x10000000),
ENUM_V(AccessedAtRuntimeMask             , 0x30000000)
