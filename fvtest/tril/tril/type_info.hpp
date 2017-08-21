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

#ifndef TYPE_INFO_HPP
#define TYPE_INFO_HPP

#include "il/DataTypes.hpp"

#include <string>
#include <stdexcept>

namespace Tril {

/**
 * @brief Class for dealing with data types in Tril
 */
struct TypeInfo {
    /**
     * @brief Gets the TR::DataTypes value from the data type's name
     * @param name is the name of the data type as a string
     * @return the TR::DataTypes value corresponding to the specified name
     */
    static TR::DataTypes getTRDataTypes(const std::string& name) {
        if (name == "Int8") return TR::Int8;
        else if (name == "Int16") return TR::Int16;
        else if (name == "Int32") return TR::Int32;
        else if (name == "Int64") return TR::Int64;
        else if (name == "Address") return TR::Address;
        else if (name == "Float") return TR::Float;
        else if (name == "Double") return TR::Double;
        else if (name == "NoType") return TR::NoType;
        else {
            throw std::runtime_error{std::string{"Unknown type name: "}.append(name)};
        }
    }
};

} // namespace Tril

#endif // TYPE_INFO_HPP
