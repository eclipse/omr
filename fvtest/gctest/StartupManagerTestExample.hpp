/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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


#if !defined(MM_STARTUPMANAGERTESTEXAMPLE_HPP_)
#define MM_STARTUPMANAGERTESTEXAMPLE_HPP_

#include "StartupManagerImpl.hpp"

class MM_StartupManagerTestExample : public MM_StartupManagerImpl
{
	/*
	 * Data members
	 */
private:
	const char *_configFile;
protected:

public:

	/*
	 * Function members
	 */
private:
protected:
	/**
	 * parse gc options in test configuration file
	 * @param extensions GCExtensions
	 * @return true if parsing completed, false otherwise
	 */
	virtual bool parseLanguageOptions(MM_GCExtensionsBase *extensions);

public:
	MM_StartupManagerTestExample(OMR_VM *omrVM, const char *configFile)
		: MM_StartupManagerImpl(omrVM)
		, _configFile(configFile)
	{
	}
};

#endif /* MM_STARTUPMANAGERTESTEXAMPLE_HPP_ */
