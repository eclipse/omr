/******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "infra/BitVector.hpp"
#include <string>
#include "gtest/gtest.h"

namespace {

class SingleBitContainerTest : public :: testing :: Test {

	protected:
		TR_SingleBitContainer container;
		TR_SingleBitContainer other;


	virtual void SetUp() {
		container.resetAll(1);
		other.resetAll(1);
	}


};


//****** restAll(int64_t m, int64_t) ******//

TEST_F(SingleBitContainerTest, resetAllTest1) {

	//Reseting container to false
	container.set();
	container.resetAll(0, 1);
	ASSERT_EQ(container.get(), false) << "resetAll - bit not reset";
}

TEST_F(SingleBitContainerTest, resetAllTest2) {
	
	//Reseting with m = 1 and n = 1, container should not be reset
	container.set();
	container.resetAll(1,1);
	ASSERT_EQ(container.get(), true);
}

TEST_F(SingleBitContainerTest, resetAllTest3) {
	
	//Reseting with m = 0, n = 0, container should not be reset
	container.set();
	container.resetAll(0,0);
	ASSERT_EQ(container.get(), true);
}

//****** Test resetAll(int64_t n) *******//
	
TEST_F(SingleBitContainerTest, resetAllTest4) {
	
	//Reseting with n = 0, container should not be reset
	container.set();
	container.resetAll(0);
	ASSERT_EQ(container.get(), true) << "resetAll - invalid bit number, should not reset bit";

}

TEST_F(SingleBitContainerTest, resetAllTest5) {
	
	//Reset the first bit
	container.set();
	container.resetAll(1);
	ASSERT_EQ(container.get(), false) << "resetAll - bit not reset to false";
}


//****** Test setAll(int64_t m, int64_t n) ******//

TEST_F(SingleBitContainerTest, setAllTest1) {
	
	//Set first bit to true
	container.setAll(0, 1);
	ASSERT_EQ(container.get(), true) << "setAll - bit not set";
}

TEST_F(SingleBitContainerTest, setAllTest2) {

	//m = 1, n = 1, container should not be set	
	container.setAll(1,1);
	ASSERT_EQ(container.get(), false);
}

TEST_F(SingleBitContainerTest, setAllTest3) {

	//m = 0, n = 0, container should not be set
	container.setAll(0,0);
	ASSERT_EQ(container.get(), false);
}


//******* Test setAll(int64_t n) *******//

TEST_F(SingleBitContainerTest, setAllTest4) {
	
	//setting bit to false
	container.setAll(0);
	ASSERT_EQ(container.get(), false);
}

TEST_F(SingleBitContainerTest, setAllTest5) {
	
	//setting bit to true
	container.setAll(1);
	ASSERT_EQ(container.get(), true);
}


//******* Test intersection of two container ******//

TEST_F(SingleBitContainerTest, intersects) {

	//Intersection of two false containers
	container.resetAll(1);
	other.resetAll(1);
	ASSERT_EQ(container.intersects(other), false);

	//Intersection of two true bit containers
	container.setAll(1);
	other.setAll(1);
	ASSERT_EQ(container.intersects(other), true);

	//Intersection of one false one true containers
	container.resetAll(1);
	other.setAll(1);
	ASSERT_EQ(container.intersects(other), false);
}

//****** Test operatorEqual ******//

TEST_F(SingleBitContainerTest, operatorEqual){
	
	//Both container set to false
	container.resetAll(1);
	other.resetAll(1);
	ASSERT_EQ(container == other, true);

	//Both container set to true
	container.setAll(1);
	other.setAll(1);
	ASSERT_EQ(container == other, true);

	//Both container set differently
	container.setAll(1);
	other.resetAll(1);
	ASSERT_EQ(container == other, false);

}

//****** Test operatorNotEqual ******//

TEST_F(SingleBitContainerTest, operatorNotEqualTest1) {
	
	//Both container set to false
	container.resetAll(1);
	other.resetAll(1);
	ASSERT_EQ(container != other, false);

	//Containers set differently
	container.setAll(1);
	other.resetAll(1);
	ASSERT_EQ(container != other, true);
}

//****** Test for |= operator ******//

TEST_F(SingleBitContainerTest, bitOperator) {
	
	//Both container set to false based on default behaviour
	container |= other;
	ASSERT_EQ(container.get(), false);

	//Both containers set differently
	container.setAll(1);
	other.resetAll(1);
	container |= other;
	ASSERT_EQ(container.get(), true);

	//other container set to true
	container.resetAll(1);
	other.setAll(1);
	container |= other;
	ASSERT_EQ(container.get(), true);

}

//****** Test for &= operator ******//

TEST_F(SingleBitContainerTest, operatorAndEqualTest) {
	
	//Both container set to false be default
	container &= other;
	ASSERT_EQ(container.get(), false);

	//Both container set to true
	container.setAll(1);
	other.setAll(1);
	container &= other;
	ASSERT_EQ(container.get(), true);

	//other container set to false
	container.setAll(1);
	other.resetAll(1);
	container &= other;
	ASSERT_EQ(container.get(), false);

}

//****** Test for SubEqual operator ******//

TEST_F(SingleBitContainerTest, operatorSubEqualTest){
	
	//Both container set to false by default
	container -= other;
	ASSERT_EQ(container.get(), false);

	//other container is set to true
	container.resetAll(1);
	other.setAll(1);
	container -= other;
	ASSERT_EQ(container.get(), false);

}

//****** Test for assignment Operator *******//

TEST_F(SingleBitContainerTest, assignmentTest) {
	
	//other is set to true
	other.setAll(1);
	container = other;
	ASSERT_EQ(container.get(), true);

	//assigning other to container
	container.setAll(1);
	other.resetAll(1);
	container = other;
	ASSERT_EQ(container.get(), false);



}

//****** Test for empty checking ******//

TEST_F(SingleBitContainerTest, emptyTest){

	//Testing emptiness for a filled container
	container.set();
	ASSERT_EQ(container.get(), true);

	//Testing emptiness for an empty container
	container.empty();
	ASSERT_EQ(container.get(), false);

}

//****** Test for element count ******//

TEST_F(SingleBitContainerTest, hasMoreThanOneElementTest){

	//SingleBitContainer should always have one element
	ASSERT_EQ(container.hasMoreThanOneElement(), false);
}

//******* Test for element count ******//

TEST_F(SingleBitContainerTest, elementCount){

	//SingleBitContainer should always have one element
	ASSERT_EQ(container.elementCount(), 1);
}

//****** Test for number of non-zero chunks  ******//

TEST_F(SingleBitContainerTest, numUsedChunks1){

	//should always have only one element in use
	container.set();
	ASSERT_EQ(container.numUsedChunks(), 1);
}

//****** Test for number of non-zero chunks  ******//

TEST_F(SingleBitContainerTest, numNonZeroChunks) {

	//testing an empty container
	container.empty();
	ASSERT_EQ(container.numNonZeroChunks(), 0);

	//testing a filled container
	container.set();
	ASSERT_EQ(container.numNonZeroChunks(), 1);
}

//****** Test emptiness of container ******//

TEST_F(SingleBitContainerTest, isEmpty) {

	//empty the container
	container.empty();
	ASSERT_EQ(container.isEmpty(), true);

	//testing emptiness of filled container
	container.set();
	ASSERT_EQ(container.isEmpty(), false);
}


};


