#ifndef BENCODE_FORWARDS_H_INCLUDE
#define BENCODE_FORWARDS_H_INCLUDE

namespace Bencode {

	// writer.h
	//class FastWriter;
	//class StyledWriter;
	class Writer;

	// reader.h
	class Reader;


	// value.h
	typedef int Int;
	typedef unsigned int UInt;
	class StaticString;
	class Path;
	class PathArgument;
	class Value;
	class ValueIteratorBase;
	class ValueIterator;
	class ValueConstIterator;

} // namespace Bencode



#endif // !BENCODE_FORWARDS_H_INCLUDE
