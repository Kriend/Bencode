#include "reader.h"
#include "value.h"
#include <utility>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>

#if _MSC_VER >= 1400 // VC++ 8.0
#pragma warning( disable : 4996 )   // disable warning about strdup being deprecated.
#endif

namespace Bencode {

    // Implementation of class Reader
// ////////////////////////////////


    static inline bool
        in(Reader::Char c, Reader::Char c1, Reader::Char c2, Reader::Char c3, Reader::Char c4)
    {
        return c == c1 || c == c2 || c == c3 || c == c4;
    }

    static inline bool
        in(Reader::Char c, Reader::Char c1, Reader::Char c2, Reader::Char c3, Reader::Char c4, Reader::Char c5)
    {
        return c == c1 || c == c2 || c == c3 || c == c4 || c == c5;
    }


    static bool
        containsNewLine(Reader::Location begin,
            Reader::Location end)
    {
        for (; begin < end; ++begin)
            if (*begin == '\n' || *begin == '\r')
                return true;
        return false;
    }

    static std::string codePointToUTF8(unsigned int cp)
    {
        std::string result;

        // based on description from http://en.wikipedia.org/wiki/UTF-8

        if (cp <= 0x7f)
        {
            result.resize(1);
            result[0] = static_cast<char>(cp);
        }
        else if (cp <= 0x7FF)
        {
            result.resize(2);
            result[1] = static_cast<char>(0x80 | (0x3f & cp));
            result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
        }
        else if (cp <= 0xFFFF)
        {
            result.resize(3);
            result[2] = static_cast<char>(0x80 | (0x3f & cp));
            result[1] = 0x80 | static_cast<char>((0x3f & (cp >> 6)));
            result[0] = 0xE0 | static_cast<char>((0xf & (cp >> 12)));
        }
        else if (cp <= 0x10FFFF)
        {
            result.resize(4);
            result[3] = static_cast<char>(0x80 | (0x3f & cp));
            result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
            result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
            result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
        }

        return result;
    }

    Reader::Reader()
    {

    }

    bool Reader::parse(const std::string& document, Value& root)
    {
        document_ = document;
        const char* begin = document_.c_str();
        const char* end = begin + document_.length();
        return parse(begin, end, root);
    }

    bool Reader::parse(const char* beginDoc, const char* endDoc, Value& root)
    {
        begin_ = beginDoc;
        end_ = endDoc;
        current_ = begin_;
        lastValueEnd_ = 0;
        lastValue_ = 0;
        errors_.clear();
        while (!nodes_.empty())
            nodes_.pop();
        nodes_.push(&root);

        bool successful = readValue();
        Token token;
        readToken(token);
        if (!root.isList() && !root.isDict())
        {
            // Set error location to start of doc, ideally should be first token found in doc
            token.type_ = tokenError;
            token.start_ = beginDoc;
            token.end_ = endDoc;
            addError("A valid JSON document must be either an array or an object value.",
                token);
            return false;
        }
        
        return successful;
    }

    bool Reader::parse(std::istream& is, Value& root)
    {
        std::string doc;
        std::getline(is, doc, (char)EOF);
        return parse(doc, root);
    }

    bool Reader::readValue()
    {
        Token token;
        readToken(token);
        bool successful = true;



        switch (token.type_)
        {
        case tokenDictBegin:
            successful = readDict(token);
            break;
        case tokenListBegin:
            successful = readList(token);
            break;
        case tokenNumber:
            successful = decodeNumber(token);
            break;
        case tokenString:
            successful = decodeString(token);
            break;
        default:
            return addError("Syntax error: value, dict or list expected.", token);
        }

        return successful;
    }

    bool Reader::readDict(Token& token)
    {
        Token tokenName;
        std::string name;
        currentValue() = Value(dictValue);
        while (readToken(tokenName))
        {
            if (tokenName.type_ == tokenEnd)  // empty object
                return true;
            if (tokenName.type_ != tokenString)
                break;

            name = "";
            if (!decodeString(tokenName, name))
                return recoverFromError(tokenEnd);

            Value& value = currentValue()[name];
            nodes_.push(&value);
            bool ok = readValue();
            nodes_.pop();
            if (!ok) // error already set
                return recoverFromError(tokenEnd);

        }
        return addErrorAndRecover("Missing '}' or object member name",
            tokenName,
            tokenEnd);
    }

    bool Reader::readList(Token& token)
    {
        currentValue() = Value(listValue);
        if (*current_ == 'e') // empty array
        {
            Token endArray;
            readToken(endArray);
            return true;
        }
        int index = 0;
        while (true)
        {
            Value& value = currentValue()[index++];
            nodes_.push(&value);
            bool ok = readValue();
            nodes_.pop();
            if (!ok) // error already set
                return recoverFromError(tokenEnd);

            if (*current_ == 'e') {
                ++current_;
                break;
            }
        }
        return true;
    }

    bool Reader::decodeNumber(Token& token)
    {
        Location current = token.start_;
        bool isNegative = *++current == '-';
        if (isNegative)
            ++current;
        Value::UInt value = 0;
        while (current < token.end_ - 1)
        {
            Char c = *current++;
            if (c < '0' || c > '9')
                return addError("'" + std::string(token.start_, token.end_) + "' is not a number.", token);
            value = value * 10 + Value::UInt(c - '0');
        }
        if (isNegative)
            currentValue() = -Value::Int(value);
        else if (value <= Value::UInt(Value::maxInt))
            currentValue() = Value::Int(value);
        else
            currentValue() = value;
        return true;
    }

    bool Reader::decodeString(Token& token)
    {
        std::vector<char> decoded;
        if (!decodeString(token, decoded))
            return false;

        currentValue() = { 
            &decoded[0], 
            decoded.size() 
        };

        return true;
    }

    bool Reader::decodeString(Token& token, std::vector<char>& decoded)
    {
        Location current = token.start_; // read how many chars that need to read
        int n = 0;
        do {
            int i = ctoi(*current);
            if (i == -1)return false;
            n = n * 10 + i;
        } while (*++current != ':');
        ++current;
        decoded.resize(n);
        memcpy(&decoded[0], current, n);
        return true;
    }

    bool Reader::decodeString(Token& token, std::string& decoded)
    {
        Location current = token.start_; // skip '"'
        Location end = token.end_;      // do not include '"'
        int n = 0;

        do{
            int i = ctoi(*current);
            if (i == -1)return false;
            n = n * 10 + i;
        } while (*++current != ':');
        ++current;
        decoded.reserve(n);
        while (current != end && n-- > 0)
        {
            decoded += *current++;
            /*Char c = *current++;
            if (c == '\\')
            {
                if (current == end)
                    return addError("Empty escape sequence in string", token, current);
                Char escape = *current++;
                switch (escape)
                {
                case '"': decoded += '"'; break;
                case '/': decoded += '/'; break;
                case '\\': decoded += '\\'; break;
                case 'b': decoded += '\b'; break;
                case 'f': decoded += '\f'; break;
                case 'n': decoded += '\n'; break;
                case 'r': decoded += '\r'; break;
                case 't': decoded += '\t'; break;
                case 'u':
                {
                    unsigned int unicode;
                    if (!decodeUnicodeCodePoint(token, current, end, unicode))
                        return false;
                    decoded += codePointToUTF8(unicode);
                }
                break;
                default:
                    return addError("Bad escape sequence in string", token, current);
                }
            }
            else
            {
                decoded += c;
            }*/
        }
        return true;
    }

    bool Reader::decodeUnicodeCodePoint(Token& token, Location& current, Location end, unsigned int& unicode)
    {
        if (!decodeUnicodeEscapeSequence(token, current, end, unicode))
            return false;
        if (unicode >= 0xD800 && unicode <= 0xDBFF)
        {
            // surrogate pairs
            if (end - current < 6)
                return addError("additional six characters expected to parse unicode surrogate pair.", token, current);
            unsigned int surrogatePair;
            if (*(current++) == '\\' && *(current++) == 'u')
            {
                if (decodeUnicodeEscapeSequence(token, current, end, surrogatePair))
                {
                    unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
                }
                else
                    return false;
            }
            else
                return addError("expecting another \\u token to begin the second half of a unicode surrogate pair", token, current);
        }
        return true;
    }

    bool Reader::decodeUnicodeEscapeSequence(Token& token, Location& current, Location end, unsigned int& unicode)
    {
        if (end - current < 4)
            return addError("Bad unicode escape sequence in string: four digits expected.", token, current);
        unicode = 0;
        for (int index = 0; index < 4; ++index)
        {
            Char c = *current++;
            unicode *= 16;
            if (c >= '0' && c <= '9')
                unicode += c - '0';
            else if (c >= 'a' && c <= 'f')
                unicode += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                unicode += c - 'A' + 10;
            else
                return addError("Bad unicode escape sequence in string: hexadecimal digit expected.", token, current);
        }
        return true;
    }

    bool Reader::addError(const std::string& message, Token& token, Location extra)
    {
        ErrorInfo info;
        info.token_ = token;
        info.message_ = message;
        info.extra_ = extra;
        errors_.push_back(info);
        return false;
    }

    bool Reader::recoverFromError(TokenType skipUntilToken)
    {
        int errorCount = int(errors_.size());
        Token skip;
        while (true)
        {
            if (!readToken(skip))
                errors_.resize(errorCount); // discard errors caused by recovery
            if (skip.type_ == skipUntilToken || skip.type_ == tokenEndOfStream)
                break;
        }
        errors_.resize(errorCount);
        return false;
    }

    bool Reader::addErrorAndRecover(const std::string& message, Token& token, TokenType skipUntilToken)
    {
        addError(message, token);
        return recoverFromError(skipUntilToken);
    }

    bool Reader::expectToken(TokenType type, Token& token, const char* message)
    {
        readToken(token);
        if (token.type_ != type)
            return addError(message, token);
        return true;
    }

    bool Reader::readToken(Token& token)
    {
        token.start_ = current_;
        Char c = getNextChar();
        bool ok = true;
        switch (c)
        {
        case 'd':
            token.type_ = tokenDictBegin;
            break;
        case 'l':
            token.type_ = tokenListBegin;
            break;
        case 'e':
            token.type_ = tokenEnd;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            token.type_ = tokenString;
            ok = readString();
            break;
        case 'i':
            token.type_ = tokenNumber;
            getNextChar();
            ok = readNumber();
            break;
        case 0:
            token.type_ = tokenEndOfStream;
            break;
        default:
            ok = false;
            break;
        }
        if (!ok)
            token.type_ = tokenError;
        token.end_ = current_;
        return true;
    }


    bool Reader::match(Location pattern,
            int patternLength)
    {
        if (end_ - current_ < patternLength)
            return false;
        int index = patternLength;
        while (index--)
            if (current_[index] != pattern[index])
                return false;
        current_ += patternLength;
        return true;
    }

    bool Reader::readString()
    {
        int n = 0;
        readNumber(n);
        if (getNextChar() != ':')
        {
            return false;
        }
        while (current_ != end_)
        {
            if (n-- > 0) {
                getNextChar();
            }
            else break;
        }
        if (n > 0)return false;
        return true;
    }

    bool Reader::readNumber()
    {
        int a = 0;
        return readNumber(a);
    }

    bool Reader::readNumber(int& num)
    {
        num = 0;
        --current_;
        while (current_ != end_)
        {
            if (!(*current_ >= '0' && *current_ <= '9') &&
                (*current_ != '-'))
                break;
            if (*current_ == '-')num = -num;
            else num = num * 10 + ctoi(*current_);
            ++current_;
        }
        if (*current_ == 'e') {
            ++current_;
            return true;
        }
        return false;
    }

    Value&
        Reader::currentValue()
    {
        return *(nodes_.top());
    }

    Reader::Char Reader::getNextChar()
    {
        if (current_ == end_)
            return 0;
        return *current_++;
    }

    void Reader::getLocationLineAndColumn(Location location, int& line, int& column) const
    {
        Location current = begin_;
        Location lastLineStart = current;
        line = 0;
        while (current < location && current != end_)
        {
            Char c = *current++;
            if (c == '\r')
            {
                if (*current == '\n')
                    ++current;
                lastLineStart = current;
                ++line;
            }
            else if (c == '\n')
            {
                lastLineStart = current;
                ++line;
            }
        }
        // column & line start at 1
        column = int(location - lastLineStart) + 1;
        ++line;
    }

    std::string Reader::getLocationLineAndColumn(Location location) const
    {
        int line, column;
        getLocationLineAndColumn(location, line, column);
        char buffer[18 + 16 + 16 + 1];
        sprintf(buffer, "Line %d, Column %d", line, column);
        return buffer;
    }

    std::string Reader::getFormatedErrorMessages() const
    {
        std::string formattedMessage;
        for (Errors::const_iterator itError = errors_.begin();
            itError != errors_.end();
            ++itError)
        {
            const ErrorInfo& error = *itError;
            formattedMessage += "* " + getLocationLineAndColumn(error.token_.start_) + "\n";
            formattedMessage += "  " + error.message_ + "\n";
            if (error.extra_)
                formattedMessage += "See " + getLocationLineAndColumn(error.extra_) + " for detail.\n";
        }
        return formattedMessage;
    }

    std::istream& operator>>(std::istream& sin, Value& root)
    {
        // TODO: 在此处插入 return 语句
        Bencode::Reader reader;
        bool ok = reader.parse(sin, root);
        //JSON_ASSERT( ok );
        if (!ok) throw std::runtime_error(reader.getFormatedErrorMessages());
        return sin;
    }

    int ctoi(const char c) {
        switch (c)
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return (int)c - '0';
        default:
            return -1;
        }
    }
} // namespace Bencode