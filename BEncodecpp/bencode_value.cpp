#include <iostream>
#include "value.h"
#include "writer.h"
#include <utility>
#include <stdexcept>
#include <cstring>
#include <cassert>

#include <cstddef>		// size_t

#define BENCODE_ASSERT_UNREACHABLE assert(false)
#define BENCODE_ASSERT(condition) assert(condition);	// @todo <= change this into an exception throw
#define BENCODE_ASSERT_MESSAGE(condition, message) if (!( condition )) throw std::runtime_error( message );

namespace Bencode {

	const Value Value::null;
	const Int Value::minInt = Int(~(UInt(-1) / 2));
	const Int Value::maxInt = Int(UInt(-1) / 2);
	const UInt Value::maxUInt = UInt(-1);

	ValueAllocator::~ValueAllocator()
	{
	}

	class DefaultValueAllocator : public ValueAllocator
	{
	public:
		virtual ~DefaultValueAllocator()
		{
		}

		virtual char* makeMemberName(const char* memberName)
		{
			return duplicateStringValue(memberName);
		}

		virtual void releaseMemberName(char* memberName)
		{
			releaseStringValue(memberName);
		}

		virtual char* duplicateStringValue(const char* value,
			unsigned int length = unknown)
		{
			//@todo invesgate this old optimization
			//if ( !value  ||  value[0] == 0 )
			//   return 0;

			if (length == unknown)
				length = (unsigned int)strlen(value);
			char* newString = static_cast<char*>(malloc(length + 1));
			memcpy(newString, value, length);
			newString[length] = 0;
			return newString;
		}

		virtual void releaseStringValue(char* value)
		{
			if (value)
				free(value);
		}
	};

	static ValueAllocator*& valueAllocator()
	{
		static DefaultValueAllocator defaultAllocator;
		static ValueAllocator* valueAllocator = &defaultAllocator;
		return valueAllocator;
	}

	static struct DummyValueAllocatorInitializer {
		DummyValueAllocatorInitializer()
		{
			valueAllocator();      // ensure valueAllocator() statics are initialized before main().
		}
	} dummyValueAllocatorInitializer;

#include "bencode_valueiterator.inl"

	Value::CZString::CZString(int index)
		: cstr_(0)
		, index_(index)
	{
	}
	Value::CZString::CZString(const char* cstr, DuplicationPolicy allocate)
		: cstr_(allocate == duplicate ? valueAllocator()->makeMemberName(cstr)
			: cstr)
		, index_(allocate)
	{
	}
	Value::CZString::CZString(const CZString& other)
		: cstr_(other.index_ != noDuplication && other.cstr_ != 0
			? valueAllocator()->makeMemberName(other.cstr_)
			: other.cstr_)
		, index_(other.cstr_ ? (other.index_ == noDuplication ? noDuplication : duplicate)
			: other.index_)
	{
	}
	Value::CZString::~CZString()
	{
		if (cstr_ && index_ == duplicate)
			valueAllocator()->releaseMemberName(const_cast<char*>(cstr_));
	}
	Value::CZString& Value::CZString::operator=(const CZString& other)
	{
		// TODO: 在此处插入 return 语句
		CZString temp(other);
		swap(temp);
		return *this;
	}
	bool Value::CZString::operator<(const CZString& other) const
	{
		if (cstr_)
			return strcmp(cstr_, other.cstr_) < 0;
		return index_ < other.index_;
	}
	bool Value::CZString::operator==(const CZString& other) const
	{
		if (cstr_)
			return strcmp(cstr_, other.cstr_) == 0;
		return index_ == other.index_;
	}
	int Value::CZString::index() const
	{
		return index_;
	}
	const char* Value::CZString::c_str() const
	{
		return cstr_;
	}
	bool Value::CZString::isStaticString() const
	{
		return index_ == noDuplication;
	}
	void Value::CZString::swap(CZString& other)
	{
		std::swap(cstr_, other.cstr_);
		std::swap(index_, other.index_);
	}

	Value::Value(ValueType type)
		: type_(type)
		, stringlength_(0)
		, allocated_(0)
	{
		switch (type)
		{
		case nullValue:
			break;
		case intValue:
			value_.int_ = 0;
			break;
		case stringValue:
			value_.string_ = 0;
			break;
		case listValue:
		case dictValue:
			value_.map_ = new ObjectValues();
			break;
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
	}
	Value::Value(Int value)
		: type_(intValue)
	{
		value_.int_ = value;
	}
	Value::Value(const char* value, UInt length)
		: type_(stringValue)
		, allocated_(true)
	{
		value_.string_ = valueAllocator()->duplicateStringValue(value, length);
		stringlength_ = length;
	}
	Value::Value(const char* beginValue, const char* endValue)
		: type_(stringValue)
		, allocated_(true)
	{
		value_.string_ = valueAllocator()->duplicateStringValue(beginValue,
			UInt(endValue - beginValue));
		stringlength_ = endValue - beginValue;
	}
	Value::Value(const StaticString& value)
		: type_(stringValue)
		, allocated_(false)
	{
		value_.string_ = const_cast<char*>(value.c_str());
	}
	Value::Value(const std::string& value)
		: type_(stringValue)
		, allocated_(true)
	{
		value_.string_ = valueAllocator()->duplicateStringValue(value.c_str(),
			(unsigned int)value.length());
		stringlength_ = (unsigned int)value.length();
	}
	Value::Value(const Value& other)
		: type_(other.type_)
	{
		switch (type_)
		{
		case nullValue:
		case intValue:
			value_ = other.value_;
			break;
		case stringValue:
			if (other.value_.string_)
			{
				value_.string_ = valueAllocator()->duplicateStringValue(other.value_.string_, other.stringlength_);
				stringlength_ = other.stringlength_;
				allocated_ = true;
			}
			else {
				value_.string_ = 0;
				stringlength_ = 0;
			}
			break;
		case listValue:
		case dictValue:
			value_.map_ = new ObjectValues(*other.value_.map_);
			break;
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
	}
	Value::~Value()
	{
		switch (type_)
		{
		case nullValue:
		case intValue:
			break;
		case stringValue:
			if (allocated_)
				valueAllocator()->releaseStringValue(value_.string_);
			break;
		case listValue:
		case dictValue:
			delete value_.map_;
			break;
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
	}
	Value& Value::operator=(const Value& other)
	{
		// TODO: 在此处插入 return 语句
		Value temp(other);
		swap(temp);
		return *this;
	}
	void Value::swap(Value& other)
	{
		ValueType temp = type_;
		type_ = other.type_;
		UInt templength = stringlength_;
		stringlength_ = other.stringlength_;
		other.stringlength_ = templength;
		other.type_ = temp;
		std::swap(value_, other.value_);
		int temp2 = allocated_;
		allocated_ = other.allocated_;
		other.allocated_ = temp2;
	}
	ValueType Value::type() const
	{
		return type_;
	}
	bool Value::operator<(const Value& other) const
	{
		int typeDelta = type_ - other.type_;
		if (typeDelta)
			return typeDelta < 0 ? true : false;
		switch (type_)
		{
		case nullValue:
			return false;
		case intValue:
			return value_.int_ < other.value_.int_;
		case stringValue:
			return (value_.string_ == 0 && other.value_.string_)
				|| (other.value_.string_
					&& value_.string_
					&& strcmp(value_.string_, other.value_.string_) < 0);
		case listValue:
		case dictValue:
		{
			int delta = int(value_.map_->size() - other.value_.map_->size());
			if (delta)
				return delta < 0;
			return (*value_.map_) < (*other.value_.map_);
		}

		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
		return 0;  // unreachable
	}
	bool Value::operator<=(const Value& other) const
	{
		return !(other > * this);
	}
	bool Value::operator>=(const Value& other) const
	{
		return !(*this < other);
	}
	bool Value::operator>(const Value& other) const
	{
		return other < *this;
	}
	bool Value::operator==(const Value& other) const
	{
		int temp = other.type_;
		if (type_ != temp)
			return false;
		switch (type_)
		{
		case nullValue:
			return true;
		case intValue:
			return value_.int_ == other.value_.int_;
		case stringValue:
			return (value_.string_ == other.value_.string_)
				|| (other.value_.string_
					&& value_.string_
					&& strcmp(value_.string_, other.value_.string_) == 0);
		case listValue:
		case dictValue:
			return value_.map_->size() == other.value_.map_->size()
				&& (*value_.map_) == (*other.value_.map_);
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
		return 0;  // unreachable
	}
	bool Value::operator!=(const Value& other) const
	{
		return !(*this == other);
	}
	const char* Value::asCString() const
	{
		BENCODE_ASSERT(type_ == stringValue);
		return value_.string_;
	}
	std::string Value::asString() const
	{
		switch (type_)
		{
		case nullValue:
			return "";
		case stringValue:
			return value_.string_ ? value_.string_ : "";
		case listValue:
		case dictValue:
			BENCODE_ASSERT_MESSAGE(false, "Type is not convertible to string");
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
		return ""; // unreachable
	}
	Value::Int Value::asInt() const
	{
		switch (type_)
		{
		case nullValue:
			return 0;
		case intValue:
			return value_.int_;
		case stringValue:
		case listValue:
		case dictValue:
			BENCODE_ASSERT_MESSAGE(false, "Type is not convertible to int");
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
		return 0; // unreachable;
	}
	bool Value::isInt() const
	{
		return type_ == intValue;
	}
	bool Value::isString() const
	{
		return type_ == stringValue;
	}
	bool Value::isList() const
	{
		return type_ == nullValue || type_ == listValue;
	}
	bool Value::isDict() const
	{
		return type_ == nullValue || type_ == dictValue;
	}
	bool Value::isConvertibleTo(ValueType other) const
	{
		switch (type_)
		{
		case nullValue:
			return true;
		case intValue:
			return (other == nullValue && value_.int_ == 0)
				|| other == intValue
				|| other == stringValue;
		case stringValue:
			return other == stringValue
				|| (other == nullValue && (!value_.string_ || value_.string_[0] == 0));
		case listValue:
			return other == listValue
				|| (other == nullValue && value_.map_->size() == 0);
		case dictValue:
			return other == dictValue
				|| (other == nullValue && value_.map_->size() == 0);
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
		return false; // unreachable;
	}
	Value::UInt Value::size() const
	{
		switch (type_)
		{
		case nullValue:
		case intValue:
		case stringValue:
			return 0;
		case listValue:  // size of the array is highest index + 1
			if (!value_.map_->empty())
			{
				ObjectValues::const_iterator itLast = value_.map_->end();
				--itLast;
				return (*itLast).first.index() + 1;
			}
			return 0;
		case dictValue:
			return Int(value_.map_->size());
		default:
			BENCODE_ASSERT_UNREACHABLE;
		}
		return 0; // unreachable;
	}

	bool Value::empty() const
	{
		if (isList() || isDict())
			return size() == 0u;
		else
			return false;
	}

	void Value::clear()
	{
		BENCODE_ASSERT(type_ == nullValue || type_ == listValue || type_ == dictValue);

		switch (type_)
		{
		case listValue:
		case dictValue:
			value_.map_->clear();
			break;
		default:
			break;
		}
	}

	Value& Value::operator[](UInt index)
	{
		// TODO: 在此处插入 return 语句
		BENCODE_ASSERT(type_ == nullValue || type_ == listValue);
		if (type_ == nullValue)
			*this = Value(listValue);
		CZString key(index);
		ObjectValues::iterator it = value_.map_->lower_bound(key);
		if (it != value_.map_->end() && (*it).first == key)
			return (*it).second;

		ObjectValues::value_type defaultValue(key, null);
		it = value_.map_->insert(it, defaultValue);
		return (*it).second;
	}

	const Value& Value::operator[](UInt index) const
	{
		// TODO: 在此处插入 return 语句
		BENCODE_ASSERT(type_ == nullValue || type_ == listValue);
		if (type_ == nullValue)
			return null;
		CZString key(index);
		ObjectValues::const_iterator it = value_.map_->find(key);
		if (it == value_.map_->end())
			return null;
		return (*it).second;
	}

	Value Value::get(UInt index, const Value& defaultValue) const
	{
		const Value* value = &((*this)[index]);
		return value == &null ? defaultValue : *value;
	}

	bool Value::isValidIndex(UInt index) const
	{
		return index < size();
	}

	Value& Value::append(const Value& value)
	{
		// TODO: 在此处插入 return 语句
		return (*this)[size()] = value;
	}

	Value& Value::operator[](const char* key)
	{
		// TODO: 在此处插入 return 语句
		return resolveReference(key, false);
	}

	const Value& Value::operator[](const char* key) const
	{
		// TODO: 在此处插入 return 语句
		BENCODE_ASSERT(type_ == nullValue || type_ == dictValue);
		if (type_ == nullValue)
			return null;
		CZString actualKey(key, CZString::noDuplication);
		ObjectValues::const_iterator it = value_.map_->find(actualKey);
		if (it == value_.map_->end())
			return null;
		return (*it).second;
	}

	Value& Value::operator[](const std::string& key)
	{
		// TODO: 在此处插入 return 语句
		return (*this)[key.c_str()];
	}

	const Value& Value::operator[](const std::string& key) const
	{
		// TODO: 在此处插入 return 语句
		return (*this)[key.c_str()];
	}

	Value& Value::operator[](const StaticString& key)
	{
		// TODO: 在此处插入 return 语句
		return resolveReference(key, true);
	}

	Value Value::get(const char* key, const Value& defaultValue) const
	{
		const Value* value = &((*this)[key]);
		return value == &null ? defaultValue : *value;
	}

	Value Value::get(const std::string& key, const Value& defaultValue) const
	{
		return get(key.c_str(), defaultValue);
	}

	Value Value::removeMember(const char* key)
	{
		BENCODE_ASSERT(type_ == nullValue || type_ == dictValue);
		if (type_ == nullValue)
			return null;
		CZString actualKey(key, CZString::noDuplication);
		ObjectValues::iterator it = value_.map_->find(actualKey);
		if (it == value_.map_->end())
			return null;
		Value old(it->second);
		value_.map_->erase(it);
		return old;
	}

	Value Value::removeMember(const std::string& key)
	{
		return removeMember(key.c_str());
	}

	bool Value::isMember(const char* key) const
	{
		const Value* value = &((*this)[key]);
		return value != &null;
	}

	bool Value::isMember(const std::string& key) const
	{
		return isMember(key.c_str());
	}

	Value::Members Value::getMemberNames() const
	{
		BENCODE_ASSERT(type_ == nullValue || type_ == dictValue);
		if (type_ == nullValue)
			return Value::Members();
		Members members;
		members.reserve(value_.map_->size());
		ObjectValues::const_iterator it = value_.map_->begin();
		ObjectValues::const_iterator itEnd = value_.map_->end();
		for (; it != itEnd; ++it)
			members.push_back(std::string((*it).first.c_str()));
		return members;
	}

	//std::string Value::toStyledString() const
	//{
	//	StyledWriter writer;
	//	return writer.write(*this);
	//}

	UInt Value::getStringLength() const
	{
		if (isString())
		{
			return stringlength_;
		}
		return 0;
	}

	Value::const_iterator Value::begin() const
	{
		switch (type_)
		{
		case listValue:
		case dictValue:
			if (value_.map_)
				return const_iterator(value_.map_->begin());
			break;
		default:
			break;
		}
		return const_iterator();
	}

	Value::const_iterator Value::end() const
	{
		switch (type_)
		{
		case listValue:
		case dictValue:
			if (value_.map_)
				return const_iterator(value_.map_->end());
			break;
		default:
			break;
		}
		return const_iterator();
	}

	Value::iterator Value::begin()
	{
		switch (type_)
		{
		case listValue:
		case dictValue:
			if (value_.map_)
				return iterator(value_.map_->begin());
			break;
		default:
			break;
		}
		return iterator();
	}

	Value::iterator Value::end()
	{
		switch (type_)
		{
		case listValue:
		case dictValue:
			if (value_.map_)
				return iterator(value_.map_->end());
			break;
		default:
			break;
		}
		return iterator();
	}

	Value& Value::resolveReference(const char* key, bool isStatic)
	{
		// TODO: 在此处插入 return 语句
		BENCODE_ASSERT(type_ == nullValue || type_ == dictValue);
		if (type_ == nullValue)
			*this = Value(dictValue);
		CZString actualKey(key, isStatic ? CZString::noDuplication
			: CZString::duplicateOnCopy);
		ObjectValues::iterator it = value_.map_->lower_bound(actualKey);
		if (it != value_.map_->end() && (*it).first == actualKey)
			return (*it).second;

		ObjectValues::value_type defaultValue(actualKey, null);
		it = value_.map_->insert(it, defaultValue);
		Value& value = (*it).second;
		return value;
	}


	// class PathArgument
	// //////////////////////////////////////////////////////////////////

	PathArgument::PathArgument()
		: kind_(kindNone)
	{
	}


	PathArgument::PathArgument(Value::UInt index)
		: index_(index)
		, kind_(kindIndex)
	{
	}


	PathArgument::PathArgument(const char* key)
		: key_(key)
		, kind_(kindKey)
	{
	}


	PathArgument::PathArgument(const std::string& key)
		: key_(key.c_str())
		, kind_(kindKey)
	{
	}

	// class Path
// //////////////////////////////////////////////////////////////////

	Path::Path(const std::string& path,
		const PathArgument& a1,
		const PathArgument& a2,
		const PathArgument& a3,
		const PathArgument& a4,
		const PathArgument& a5)
	{
		InArgs in;
		in.push_back(&a1);
		in.push_back(&a2);
		in.push_back(&a3);
		in.push_back(&a4);
		in.push_back(&a5);
		makePath(path, in);
	}


	void
		Path::makePath(const std::string& path,
			const InArgs& in)
	{
		const char* current = path.c_str();
		const char* end = current + path.length();
		InArgs::const_iterator itInArg = in.begin();
		while (current != end)
		{
			if (*current == '[')
			{
				++current;
				if (*current == '%')
					addPathInArg(path, in, itInArg, PathArgument::kindIndex);
				else
				{
					Value::UInt index = 0;
					for (; current != end && *current >= '0' && *current <= '9'; ++current)
						index = index * 10 + Value::UInt(*current - '0');
					args_.push_back(index);
				}
				if (current == end || *current++ != ']')
					invalidPath(path, int(current - path.c_str()));
			}
			else if (*current == '%')
			{
				addPathInArg(path, in, itInArg, PathArgument::kindKey);
				++current;
			}
			else if (*current == '.')
			{
				++current;
			}
			else
			{
				const char* beginName = current;
				while (current != end && !strchr("[.", *current))
					++current;
				args_.push_back(std::string(beginName, current));
			}
		}
	}


	void
		Path::addPathInArg(const std::string& path,
			const InArgs& in,
			InArgs::const_iterator& itInArg,
			PathArgument::Kind kind)
	{
		if (itInArg == in.end())
		{
			// Error: missing argument %d
		}
		else if ((*itInArg)->kind_ != kind)
		{
			// Error: bad argument type
		}
		else
		{
			args_.push_back(**itInArg);
		}
	}


	void
		Path::invalidPath(const std::string& path,
			int location)
	{
		// Error: invalid path.
	}


	const Value&
		Path::resolve(const Value& root) const
	{
		const Value* node = &root;
		for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it)
		{
			const PathArgument& arg = *it;
			if (arg.kind_ == PathArgument::kindIndex)
			{
				if (!node->isList() || node->isValidIndex(arg.index_))
				{
					// Error: unable to resolve path (array value expected at position...
				}
				node = &((*node)[arg.index_]);
			}
			else if (arg.kind_ == PathArgument::kindKey)
			{
				if (!node->isDict())
				{
					// Error: unable to resolve path (object value expected at position...)
				}
				node = &((*node)[arg.key_]);
				if (node == &Value::null)
				{
					// Error: unable to resolve path (object has no member named '' at position...)
				}
			}
		}
		return *node;
	}


	Value
		Path::resolve(const Value& root,
			const Value& defaultValue) const
	{
		const Value* node = &root;
		for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it)
		{
			const PathArgument& arg = *it;
			if (arg.kind_ == PathArgument::kindIndex)
			{
				if (!node->isList() || node->isValidIndex(arg.index_))
					return defaultValue;
				node = &((*node)[arg.index_]);
			}
			else if (arg.kind_ == PathArgument::kindKey)
			{
				if (!node->isDict())
					return defaultValue;
				node = &((*node)[arg.key_]);
				if (node == &Value::null)
					return defaultValue;
			}
		}
		return *node;
	}


	Value&
		Path::make(Value& root) const
	{
		Value* node = &root;
		for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it)
		{
			const PathArgument& arg = *it;
			if (arg.kind_ == PathArgument::kindIndex)
			{
				if (!node->isList())
				{
					// Error: node is not an array at position ...
				}
				node = &((*node)[arg.index_]);
			}
			else if (arg.kind_ == PathArgument::kindKey)
			{
				if (!node->isDict())
				{
					// Error: node is not an object at position...
				}
				node = &((*node)[arg.key_]);
			}
		}
		return *node;
	}

} // namespace Bencode
