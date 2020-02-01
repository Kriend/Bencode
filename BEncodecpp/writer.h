#ifndef BENCODE_WRITER_H_INCLUDE
#define BENCODE_WRITER_H_INCLUDE

#include "value.h"
#include <vector>
#include <string>
#include <iostream>

namespace Bencode {

	class Value;

	class Writer {
	public:
		Writer();
		~Writer() {}

	public:

		UInt write(const Value& root);

		char* getCString();

	private:
		void writeValue(const Value& root);
		void valueToString(Int value);
		void valueToString(std::string str);
		void valueToString(const char* value, UInt length);

		std::vector<char> document_;
	};
	
} // namespace Bencode

#endif // !BENCODE_WRITER_H_INCLUDE
