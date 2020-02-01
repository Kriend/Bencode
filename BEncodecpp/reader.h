#ifndef BENCODE_READER_H_INCLUDED
#define BENCODE_READER_H_INCLUDED

#include "value.h"
#include <deque>
#include <stack>
#include <string>
#include <iostream>

namespace Bencode {
	class Reader {
    public:
        typedef char Char;
        typedef const Char* Location;

        /** \brief Constructs a Reader allowing all features
         * for parsing.
         */
        Reader();


        bool parse(const std::string& document,
            Value& root);

        bool parse(const char* beginDoc, const char* endDoc,
            Value& root);

        bool parse(std::istream& is,
            Value& root);

        std::string getFormatedErrorMessages() const;

    private:
        enum TokenType
        {
            tokenEndOfStream = 0,
            tokenDictBegin,
            tokenListBegin,
            tokenString,
            tokenNumber,
            tokenEnd,
            tokenError
        };

        class Token
        {
        public:
            TokenType type_;
            Location start_;
            Location end_;
        };

        class ErrorInfo
        {
        public:
            Token token_;
            std::string message_;
            Location extra_;
        };

        typedef std::deque<ErrorInfo> Errors;

        bool expectToken(TokenType type, Token& token, const char* message);
        bool readToken(Token& token);
        bool match(Location pattern,
            int patternLength);
        bool readString();
        bool readNumber();
        bool readNumber(int& num);
        bool readValue();
        bool readDict(Token& token);
        bool readList(Token& token);
        bool decodeNumber(Token& token);
        bool decodeString(Token& token);
        bool decodeString(Token& token, std::vector<char>& decoded);
        bool decodeString(Token& token, std::string& decoded);      // 因为使用string无法处理byte数据，暂时弃用，使用上面的vector
        bool decodeUnicodeCodePoint(Token& token,
            Location& current,
            Location end,
            unsigned int& unicode);
        bool decodeUnicodeEscapeSequence(Token& token,
            Location& current,
            Location end,
            unsigned int& unicode);
        bool addError(const std::string& message,
            Token& token,
            Location extra = 0);
        bool recoverFromError(TokenType skipUntilToken);
        bool addErrorAndRecover(const std::string& message,
            Token& token,
            TokenType skipUntilToken);
        Value& currentValue();
        Char getNextChar();
        void getLocationLineAndColumn(Location location,
            int& line,
            int& column) const;
        std::string getLocationLineAndColumn(Location location) const;

        typedef std::stack<Value*> Nodes;
        Nodes nodes_;
        Errors errors_;
        std::string document_;
        Location begin_;
        Location end_;
        Location current_;
        Location lastValueEnd_;
        Value* lastValue_;
	};

    std::istream& operator>>(std::istream&, Value&);
    int ctoi(const char c);
} // namespace Bencode


#endif // !BENCODE_READER_H_INCLUDED
