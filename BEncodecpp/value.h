#ifndef BENCODE_VALUE_H_INCLUDE
#define BENCODE_VALUE_H_INCLUDE

#include "forwards.h"
#include <string>
#include <vector>

#include <map>

namespace Bencode {
	
	/** \brief Type of the value held by a Value object.
	 */
	enum ValueType
	{
		nullValue = 0,
		intValue,		// integer value
		stringValue,	// UTF-8 string value
		listValue,		// list value
		dictValue		// dict value
	};

	class StaticString 
	{
	public:
		explicit StaticString(const char* czstring) :
			str_(czstring)
		{

		}

		operator const char* () const
		{
			return str_;
		}

		const char* c_str() const
		{
			return str_;
		}

	private:
		const char* str_;
	};

	class Value
	{
		friend class ValueIteratorBase;
	public:
		typedef std::vector<std::string> Members;
		typedef ValueIterator iterator;
		typedef ValueConstIterator const_iterator;
		typedef Bencode::Int Int;
		typedef Bencode::UInt UInt;
		typedef UInt ArrayIndex;

		static const Value null;
		static const Int minInt;
		static const Int maxInt;
		static const UInt maxUInt;


	private:
		class CZString
		{
		public:
			enum DuplicationPolicy
			{
				noDuplication = 0,
				duplicate,
				duplicateOnCopy
			};
			CZString(int index);
			CZString(const char* cstr, DuplicationPolicy allocate);
			CZString(const CZString& other);
			~CZString();
			CZString& operator =(const CZString& other);
			bool operator<(const CZString& other) const;
			bool operator==(const CZString& other) const;
			int index() const;
			const char* c_str() const;
			bool isStaticString() const;
		private:
			void swap(CZString& other);
			const char* cstr_;
			int index_;
		};

	public:
		typedef std::map<CZString, Value> ObjectValues;

		Value(ValueType type = nullValue);
		Value(Int value);
		Value(const char* value, UInt length);
		Value(const char* beginValue, const char* endValue);


		Value(const StaticString& value);
		Value(const std::string& value);

		Value(const Value& other);
		~Value();

		Value& operator=(const Value& other);

		void swap(Value& other);

		ValueType type() const;

		bool operator <(const Value& other) const;
		bool operator <=(const Value& other) const;
		bool operator >=(const Value& other) const;
		bool operator >(const Value& other) const;

		bool operator ==(const Value& other) const;
		bool operator !=(const Value& other) const;


		const char* asCString() const;
		std::string asString() const;

		Int asInt() const;

		bool isInt() const;
		bool isString() const;
		bool isList() const;
		bool isDict() const;

		bool isConvertibleTo(ValueType other) const;

		/// Number of values in list or dict
		UInt size() const;

		/// \brief Return true if empty array, empty object, or null;
		/// otherwise, false.
		bool empty() const;

		/// Remove all object members and list elements.
		void clear();

		/// Access an array element (zero based index ).
		/// If the array contains less than index element, then null value are inserted
		/// in the array so that its size is index+1.
		/// (You may need to say 'value[0u]' to get your compiler to distinguish
		///  this from the operator[] which takes a string.)
		Value& operator[](UInt index);
		/// Access an array element (zero based index )
		/// (You may need to say 'value[0u]' to get your compiler to distinguish
		///  this from the operator[] which takes a string.)
		const Value& operator[](UInt index) const;
		/// If the array contains at least index+1 elements, returns the element value, 
		/// otherwise returns defaultValue.
		Value get(UInt index,
			const Value& defaultValue) const;
		/// Return true if index < size().
		bool isValidIndex(UInt index) const;
		/// \brief Append value to array at the end.
		///
		/// Equivalent to jsonvalue[jsonvalue.size()] = value;
		Value& append(const Value& value);

		/// Access an object value by name, create a null member if it does not exist.
		Value& operator[](const char* key);
		/// Access an object value by name, returns null if there is no member with that name.
		const Value& operator[](const char* key) const;
		/// Access an object value by name, create a null member if it does not exist.
		Value& operator[](const std::string& key);
		/// Access an object value by name, returns null if there is no member with that name.
		const Value& operator[](const std::string& key) const;

		Value& operator[](const StaticString& key);

		/// Return the member named key if it exist, defaultValue otherwise.
		Value get(const char* key,
			const Value& defaultValue) const;
		/// Return the member named key if it exist, defaultValue otherwise.
		Value get(const std::string& key,
			const Value& defaultValue) const;

		/// \brief Remove and return the named member.  
		///
		/// Do nothing if it did not exist.
		/// \return the removed Value, or null.
		/// \pre type() is objectValue or nullValue
		/// \post type() is unchanged
		Value removeMember(const char* key);
		/// Same as removeMember(const char*)
		Value removeMember(const std::string& key);

		/// Return true if the object has a member named key.
		bool isMember(const char* key) const;
		/// Return true if the object has a member named key.
		bool isMember(const std::string& key) const;

		/// \brief Return a list of the member names.
		///
		/// If null, return an empty list.
		/// \pre type() is objectValue or nullValue
		/// \post if type() was nullValue, it remains nullValue
		Members getMemberNames() const;

		/// \brief Return the length of string.
		///
		/// If is not a string type, return 0.
		UInt getStringLength() const;

		//std::string toStyledString() const;

		const_iterator begin() const;
		const_iterator end() const;

		iterator begin();
		iterator end();

	private:
		Value& resolveReference(const char* key,
			bool isStatic);

	private:
		union ValueHolder
		{
			Int int_;
			char* string_;
			ObjectValues* map_;
		} value_;
		UInt stringlength_;
		ValueType type_ : 8;
		int allocated_ : 1;
	};

	/** \brief Experimental and untested: represents an element of the "path" to access a node.
	 */
	class PathArgument
	{
	public:
		friend class Path;

		PathArgument();
		PathArgument(UInt index);
		PathArgument(const char* key);
		PathArgument(const std::string& key);

	private:
		enum Kind
		{
			kindNone = 0,
			kindIndex,
			kindKey
		};
		std::string key_;
		UInt index_;
		Kind kind_;
	};

	/** \brief Experimental and untested: represents a "path" to access a node.
	 *
	 * Syntax:
	 * - "." => root node
	 * - ".[n]" => elements at index 'n' of root node (an array value)
	 * - ".name" => member named 'name' of root node (an object value)
	 * - ".name1.name2.name3"
	 * - ".[0][1][2].name1[3]"
	 * - ".%" => member name is provided as parameter
	 * - ".[%]" => index is provied as parameter
	 */
	class Path
	{
	public:
		Path(const std::string& path,
			const PathArgument& a1 = PathArgument(),
			const PathArgument& a2 = PathArgument(),
			const PathArgument& a3 = PathArgument(),
			const PathArgument& a4 = PathArgument(),
			const PathArgument& a5 = PathArgument());

		const Value& resolve(const Value& root) const;
		Value resolve(const Value& root,
			const Value& defaultValue) const;
		/// Creates the "path" to access the specified node and returns a reference on the node.
		Value& make(Value& root) const;

	private:
		typedef std::vector<const PathArgument*> InArgs;
		typedef std::vector<PathArgument> Args;

		void makePath(const std::string& path,
			const InArgs& in);
		void addPathInArg(const std::string& path,
			const InArgs& in,
			InArgs::const_iterator& itInArg,
			PathArgument::Kind kind);
		void invalidPath(const std::string& path,
			int location);

		Args args_;
	};

	/** \brief Experimental do not use: Allocator to customize member name and string value memory management done by Value.
	 *
	 * - makeMemberName() and releaseMemberName() are called to respectively duplicate and
	 *   free an Json::objectValue member name.
	 * - duplicateStringValue() and releaseStringValue() are called similarly to
	 *   duplicate and free a Json::stringValue value.
	 */
	class ValueAllocator
	{
	public:
		enum { unknown = (unsigned)-1 };

		virtual ~ValueAllocator();

		virtual char* makeMemberName(const char* memberName) = 0;
		virtual void releaseMemberName(char* memberName) = 0;
		virtual char* duplicateStringValue(const char* value,
			unsigned int length = unknown) = 0;
		virtual void releaseStringValue(char* value) = 0;
	};

	/** \brief base class for Value iterators.
	 *
	 */
	class ValueIteratorBase
	{
	public:
		typedef unsigned int size_t;
		typedef int difference_type;
		typedef ValueIteratorBase SelfType;

		ValueIteratorBase();
		explicit ValueIteratorBase(const Value::ObjectValues::iterator& current);

		bool operator ==(const SelfType& other) const
		{
			return isEqual(other);
		}

		bool operator !=(const SelfType& other) const
		{
			return !isEqual(other);
		}

		difference_type operator -(const SelfType& other) const
		{
			return computeDistance(other);
		}

		/// Return either the index or the member name of the referenced value as a Value.
		Value key() const;

		/// Return the index of the referenced Value. -1 if it is not an arrayValue.
		UInt index() const;

		/// Return the member name of the referenced Value. "" if it is not an objectValue.
		const char* memberName() const;

	protected:
		Value& deref() const;

		void increment();

		void decrement();

		difference_type computeDistance(const SelfType& other) const;

		bool isEqual(const SelfType& other) const;

		void copy(const SelfType& other);

	private:
		Value::ObjectValues::iterator current_;
		// Indicates that iterator is for a null value.
		bool isNull_;
	};

	/** \brief const iterator for dict and list value.
	 *
	 */
	class ValueConstIterator :public ValueIteratorBase
	{
		friend class Value;
	public:
		typedef unsigned int size_t;
		typedef int difference_type;
		typedef const Value& reference;
		typedef const Value* pointer;
		typedef ValueConstIterator SelfType;

		ValueConstIterator();
	private:
		explicit ValueConstIterator(const Value::ObjectValues::iterator& current);
	
	public:
		SelfType& operator =(const ValueIteratorBase& other);

		SelfType operator++(int)
		{
			SelfType temp(*this);
			++* this;
			return temp;
		}

		SelfType operator--(int)
		{
			SelfType temp(*this);
			--* this;
			return temp;
		}

		SelfType& operator--()
		{
			decrement();
			return *this;
		}

		SelfType& operator++()
		{
			increment();
			return *this;
		}

		reference operator *() const
		{
			return deref();
		}
	};

	/** \brief Iterator for dict and list value.
	 */
	class ValueIterator :public ValueIteratorBase
	{
		friend class Value;
	public:
		typedef unsigned int size_t;
		typedef int difference_type;
		typedef Value& reference;
		typedef Value* pointer;
		typedef ValueIterator SelfType;

		ValueIterator();
		ValueIterator(const ValueConstIterator& other);
		ValueIterator(const ValueIterator& other);

	private:
		explicit ValueIterator(const Value::ObjectValues::iterator& current);

	public:
		SelfType& operator =(const SelfType& other);

		SelfType operator++(int)
		{
			SelfType temp(*this);
			++* this;
			return temp;
		}

		SelfType operator--(int)
		{
			SelfType temp(*this);
			--* this;
			return temp;
		}

		SelfType& operator--()
		{
			decrement();
			return *this;
		}

		SelfType& operator++()
		{
			increment();
			return *this;
		}

		reference operator *() const
		{
			return deref();
		}
	};

} // namespace Bencode

#endif // !BENCODE_VALUE_H_INCLUDE
