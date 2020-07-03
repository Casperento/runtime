#include "arraydata.h"
#include "value.h"
#include "virtualmachine.h"
#include "type.h"
#include <sstream>
#include <algorithm>

namespace err = logmessage::runtime;

sqf::arraydata sqf::arraydata::deep_copy() const
{
	std::vector<value> arr_copy;
	for (auto&& val : mvalue)
	{
		if (val.dtype() == type::ARRAY)
		{
			arr_copy.emplace_back(sqf::arraydata{ val.as_vector() }.deep_copy().innervector());
		}
		else
		{
			arr_copy.emplace_back(val);
		}
	}
	return arr_copy;
}

std::string sqf::arraydata::tosqf() const
{
	std::stringstream sstream;
	sstream << '[';
	bool first = true;
	for (auto& it : mvalue)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			sstream << ',';
		}
		sstream << it.tosqf();
	}
	sstream << ']';
	return sstream.str();
}

bool sqf::arraydata::equals(std::shared_ptr<data> d) const
{
	auto data = std::dynamic_pointer_cast<arraydata>(d);
	if (!data) return false;
	if (mvalue.size() != data->size())
	{
		return false;
	}

	return std::equal(mvalue.begin(),mvalue.end(),data->begin(),data->end(), [](value::cref left, value::cref right) {
		if (left.dtype() == type::STRING && left.dtype() == right.dtype())
			return left.as_string() == right.as_string();
		return left.equals(right);
	});
}

void sqf::arraydata::resize(size_t newsize)
{
	auto cursize = mvalue.size();
	mvalue.resize(newsize);
	if (newsize > cursize)
	{
		for (; cursize < newsize; cursize++)
		{
			mvalue[cursize] = value();
		}
	}
}

void sqf::arraydata::reverse()
{
	std::reverse(mvalue.begin(), mvalue.end());
}

void sqf::arraydata::extend(std::vector<value> other)
{
	mvalue.reserve(mvalue.size() + std::distance(other.begin(), other.end()));
	mvalue.insert(mvalue.end(), other.begin(), other.end());
}

void sqf::arraydata::delete_at(int position)
{
	mvalue.erase(mvalue.begin() + position);
}

std::array<double, 3> sqf::arraydata::as_vec3() const
{
	switch (size())
	{
	case 0:
		return std::array<double, 3> { 0, 0, 0 };
	case 1:
		return std::array<double, 3> { at(0).as_double(), 0, 0 };
	case 2:
		return std::array<double, 3> { at(0).as_double(), at(1).as_double(), 0 };
	default:
		return std::array<double, 3> { at(0).as_double(), at(1).as_double(), at(2).as_double() };
	}
}

std::array<double, 2> sqf::arraydata::as_vec2() const
{
	switch (size())
	{
	case 0:
		return std::array<double, 2> { 0, 0 };
	case 1:
		return std::array<double, 2> { at(0).as_double(), 0 };
	default:
		return std::array<double, 2> { at(0).as_double(), at(1).as_double() };
	}
}

bool sqf::arraydata::check_type(virtualmachine * vm, sqf::type t, size_t min, size_t max) const
{
	bool errflag = true;
	if (size() < min || size() > max)
	{
		if (min == max)
		{
			vm->logmsg(err::ExpectedArraySizeMissmatch(*vm->current_instruction(), min, size()));
		}
		else
		{
			vm->logmsg(err::ExpectedArraySizeMissmatch(*vm->current_instruction(), min, max, size()));
		}
		return false;
	}
	for (size_t i = 0; i < size(); i++)
	{
		if (at(i).dtype() != t)
		{
			vm->logmsg(err::ExpectedArrayTypeMissmatch(*vm->current_instruction(), i, t, at(i).dtype()));
			errflag = false;
		}
	}
	return errflag;
}
bool sqf::arraydata::check_type(virtualmachine * vm, const sqf::type * arr, size_t min, size_t max) const
{
	bool errflag = true;
	if (size() < min || size() > max)
	{
		if (min == max)
		{
			vm->logmsg(err::ExpectedArraySizeMissmatch(*vm->current_instruction(), min, size()));
		}
		else
		{
			vm->logmsg(err::ExpectedArraySizeMissmatch(*vm->current_instruction(), min, max, size()));
		}
		return false;
	}
	for (size_t i = 0; i < size(); i++)
	{
		if (at(i).dtype() != arr[i])
		{
			vm->logmsg(err::ExpectedArrayTypeMissmatch(*vm->current_instruction(), i, arr[i], at(i).dtype()));
			errflag = false;
		}
	}
	return errflag;
}

bool sqf::arraydata::recursion_test_helper(std::vector<std::shared_ptr<arraydata>>& tree)
{
	for (auto& it : this->mvalue)
	{
		if (it.dtype() == type::ARRAY)
		{
			// Get child
			auto arr = it.data<arraydata>();

			// Check if child was visited already
			if (std::find(tree.begin(), tree.end(), arr) != tree.end())
			{
				// Child already was visited, recursion test failed.
				return false;
			}

			// Add child to visited list
			tree.push_back(arr);

			// Check child recursion
			if (!arr->recursion_test_helper(tree))
			{
				return false;
			}

			// Remove child from visited list
			tree.pop_back();
		}
	}
	return true;
}


bool sqf::arraydata::get(size_t index, bool defval)
{
	if (size() <= index || at(index).dtype() != BOOL)
		return defval;
	return at(index).as_bool();
}
float sqf::arraydata::get(size_t index, float defval)
{
	if (size() <= index || at(index).dtype() != SCALAR)
		return defval;
	return at(index).as_float();
}
std::string sqf::arraydata::get(size_t index, std::string defval)
{
	if (size() <= index || at(index).dtype() != STRING)
		return defval;
	return at(index).as_string();
}