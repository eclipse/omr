/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2010, 2016
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

#if !defined(FREQUENTOBJECTSSTATS_HPP_)
#define FREQUENTOBJECTSSTATS_HPP_

#include "objectdescription.h"
#include "spacesaving.h"

#include "Base.hpp"

class MM_EnvironmentBase;

#define TOPK_FREQUENT_DEFAULT 10
#define K_TO_SIZE_RATIO 8

/*
 * Keeps track of the most frequent class instantiations
 */

class MM_FrequentObjectsStats : public MM_Base
{
private:

	/*
	 * Estimates the space necessary to report the top k elements accurately 90% of the time.
	 * Derived from a sample run of Eclipse, which showed that the size necessary to have accurately report the top k
	 * elements was approximately linear.
	 */
	uint32_t
	getSizeForTopKFrequent(uint32_t topKFrequent)
	{
		/* DO NOTHING */
		return 0;
	}

/* Function Members */
public:
	static MM_FrequentObjectsStats *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	/* reset the stats*/
	void clear()
	{
		/* DO NOTHING */
		return;
	}

	/*
	 * Update stats with another class
	 * @param clazz another clazz to record
	 */
	void
	update(MM_EnvironmentBase *env, omrobjectptr_t object)
	{
		/* DO NOTHING */
		return;
	}

	/* Creates a data structure which keeps track of the k most frequent class allocations (estimated probability of 90% of
	 * reporting this accurately (and in the correct order).  The larger k is, the more memory is required
	 * @param portLibrary the port library
	 * @param k the number of frequent objects we'd like to accurately report
	 */
	MM_FrequentObjectsStats(OMRPortLibrary *portLibrary, uint32_t k=TOPK_FREQUENT_DEFAULT)
	{}


	/* Merges a FrequentObjectStats structures together together with this one*/
	void merge(MM_FrequentObjectsStats* frequentObjectsStats);

	void traceStats(MM_EnvironmentBase *env);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
};

#endif /* !FREQUENTOBJECTSSTATS_HPP_ */
