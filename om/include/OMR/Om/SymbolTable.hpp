#if !defined(B9_SYMBOLS_HPP_)
#define B9_SYMBOLS_HPP_

#include <b9/objects.hpp>

#include <map>
#include <string>

namespace b9
{
class SymbolTable
{
public:
	Id lookup(std::string& string)
	{
		auto it = lookupTable_.find(string);
		if (it != lookupTable_.end()) {
			return it->second;
		} else {
			auto id = idGenerator_.newId();
			lookupTable_.insert({string, id});
			return id;
		}
	}

private:
	IdGenerator idGenerator_;
	std::map<std::string, Id> lookupTable_;
};

}  // namespace b9

#endif  // B9_SYMBOLS_HPP_
