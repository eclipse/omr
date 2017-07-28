###############################################################################
#
# (c) Copyright IBM Corp. 2017
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

add_library(omrglue
	ArrayletObjectModel.hpp
	CollectorLanguageInterfaceImpl.cpp
	CollectorLanguageInterfaceImpl.hpp
	CompactSchemeFixupObject.cpp
	CompactSchemeFixupObject.hpp
	ConfigurationLanguageInterfaceImpl.cpp
	ConfigurationLanguageInterfaceImpl.hpp
	ContractslotScanner.hpp
	EnvironmentLanguageInterfaceImpl.cpp
	EnvironmentLanguageInterfaceImpl.hpp
	FrequentObjectsStats.cpp
	FrequentObjectsStats.hpp
	HeapRegionDescriptorTypes.hpp
	LanguageSegregatedAllocationCache.hpp
	LanguageThreadLocalHeap.hpp
	MixedObjectModel.hpp
	ObjectIterator.cpp
	ObjectIterator.hpp
	ObjectModel.cpp
	ObjectModel.hpp
	ObjectScannerState.hpp
	StartupManagerImpl.cpp
	StartupManagerImpl.hpp
	VerboseManagerImpl.cpp
	VerboseManagerImpl.hpp
)
omr_set_target_flags(omrglue) 

target_include_directories(omrglue PUBLIC .)
target_link_libraries(omrglue PUBLIC omrgc)
