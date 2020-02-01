#include "writer.h"
#include <utility>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#if _MSC_VER >= 1400 // VC++ 8.0
#pragma warning( disable : 4996 )   // disable warning about strdup being deprecated.
#endif

namespace Bencode {
	Writer::Writer()
	{
		document_.clear();
	}
	UInt Writer::write(const Value& root)
	{
		document_.clear();
		writeValue(root);
		return document_.size();
	}

	char* Writer::getCString()
	{
		return &document_[0];
	}

	void Writer::writeValue(const Value& value)
	{
		switch (value.type())
		{
		case intValue:
			valueToString(value.asInt());
			break;
		case stringValue:
			valueToString(value.asCString(), value.getStringLength());
			break;
		case listValue:
		{
			document_.push_back('l');
			for (auto v : value) {
				writeValue(v);
			}
			document_.push_back('e');
		}
			break;
		case dictValue:
		{
			Value::Members members(value.getMemberNames());
			document_.push_back('d');
			for (auto member:members)
			{
				valueToString(member);
				writeValue(value[member]);
			}
			document_.push_back('e');
		}
			break;
		}
	}

	void Writer::valueToString(Int value)
	{
		int n = 0;
		int temp = value;
		bool isNegative = value < 0;
		while (temp != 0)
		{
			temp /= 10;
			++n;
		}
		if (isNegative)
		{
			++n;
		}
		UInt current = document_.size();
		document_.resize(current + n + 2);
		document_[current++] = 'i';
		itoa(value, &document_[current], 10);
		document_[current + n] = 'e';
	}

	void Writer::valueToString(std::string str)
	{
		valueToString(str.c_str(), str.length());
	}

	void Writer::valueToString(const char* value, UInt length)
	{
		int n = 0;
		int temp = length;
		while (temp != 0)
		{
			temp /= 10;
			++n;
		}
		UInt current = document_.size();
		document_.resize(current + n + 1 + length);
		itoa(length, &document_[current], 10);
		current += n;
		document_[current++] = ':';
		memcpy(&document_[current], value, length);
	}
} // namespace Bencode
