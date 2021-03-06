// The MIT License (MIT)

// Copyright (c) 2016, Microsoft

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <iostream> // TODO: remove.

#include <cctype>
#include <istream>
#include <sstream>

#include "BitFunnel/Allocators/IAllocator.h"
#include "BitFunnel/Plan/TermMatchNode.h"
#include "BitFunnel/Utilities/StringBuilder.h"
#include "QueryParser.h"
#include "StringVector.h"


namespace BitFunnel
{
    QueryParser::QueryParser(std::istream& input, IAllocator& allocator)
        : m_input(input),
          m_allocator(allocator),
          m_currentPosition(0),
          m_haveChar(false)
    {
    }


    TermMatchNode const * QueryParser::Parse()
    {
        return ParseOr();
    }


    TermMatchNode const * QueryParser::ParseOr()
    {
        TermMatchNode::Builder builder(TermMatchNode::OrMatch, m_allocator);

        auto left = ParseAnd();
        builder.AddChild(left);

        for (;;)
        {
            SkipWhite();
            if (PeekChar() != '|')
            {
                break;
            }
            GetChar();
            auto child = ParseAnd();
            builder.AddChild(child);
        }
        return builder.Complete();
    }


    TermMatchNode const * QueryParser::ParseAnd()
    {
        TermMatchNode::Builder builder(TermMatchNode::AndMatch, m_allocator);
        char const * c_endOfAndProduction = ")|";

        auto leftSimple = ParseSimple();
        builder.AddChild(leftSimple);

        for (;;)
        {
            SkipWhite();
            char c = PeekChar();
            if (c == '&')
            {
                // The '&' operator indicates there must be another Simple operand.
                GetChar();
                auto child = ParseSimple();
                builder.AddChild(child);
            }
            else if (strchr(c_endOfAndProduction, c) == nullptr)
            {
                // The absense of a ')' or '|' indicates an implicit '&' operator
                // which forces us to stay in the And-production and process another
                // Simple operand.
                auto child = ParseSimple();
                builder.AddChild(child);
            }
            else
            {
                // Otherwise, we are done with the And-production.
                break;
            }
        }
        return builder.Complete();
    }


    TermMatchNode const * QueryParser::ParseSimple()
    {
        SkipWhite();
        if (PeekChar() == '-')
        {
            GetChar();
            SkipWhite();
            auto simpleNode = ParseSimple();
            TermMatchNode::Builder builder(TermMatchNode::NotMatch, m_allocator);
            builder.AddChild(simpleNode);
            return builder.Complete();
        }
        else if (PeekChar() == '(')
        {
            GetChar();
            auto orNode = ParseOr();
            SkipWhite();
            ExpectDelimeter(')');
            return orNode;
        }
        else
        {
            return ParseTerm();
        }
    }


    TermMatchNode const * QueryParser::ParseTerm()
    {
        // Default streamId is always 0.
        Term::StreamId streamId = 0;

        SkipWhite();
        if (PeekChar() == '"')
        {
            return ParsePhrase(streamId);
        }
        else
        {
            char const * left = ParseToken();

            if (PeekChar() == ':')
            {
                // 'left' turns out to be a StreamId.
                GetChar();
                streamId = StreamIdFromText(left);

                // Look for a phrase or term following the streamId.
                if (PeekChar() == '"')
                {
                    return ParsePhrase(streamId);
                }
                else
                {
                    char const * right = ParseToken();
                    return TermMatchNode::Builder::CreateUnigramNode(right, streamId, m_allocator);
                }
            }
            else
            {
                return TermMatchNode::Builder::CreateUnigramNode(left, streamId, m_allocator);
            }
        }
    }


    TermMatchNode const * QueryParser::ParsePhrase(Term::StreamId streamId)
    {
        ExpectDelimeter('"');

        const unsigned arbitraryInitialCapacity = 6;
        StringVector& grams =
            *new(m_allocator.Allocate(sizeof(StringVector)))
                StringVector(m_allocator, arbitraryInitialCapacity);

        for (;;)
        {
            SkipWhite();
            if (PeekChar() == '"')
            {
                ExpectDelimeter('"');
                break;
            }
            // TODO: consider how we handle escapes.
            char const * token = ParseToken();
            grams.AddString(token);
        }

        return TermMatchNode::Builder::CreatePhraseNode(grams, streamId, m_allocator);
    }


    char const * QueryParser::ParseToken()
    {
        char const * c_endOfToken = "&|():-\"";
        StringBuilder builder(m_allocator);

        while (!isspace(PeekChar()) && strchr(c_endOfToken, PeekChar()) == nullptr)
        {
            char temp = GetWithEscape();
            builder.push_back(temp);
        };
        char const* token = static_cast<char*>(builder);
        if (*token == '\0')
        {
            throw ParseError("Expected token.", m_currentPosition);
        }

        return token;
    }


    void QueryParser::ExpectDelimeter(char c)
    {
        if (PeekChar() != c)
        {
            // TODO: REVIEW: Check lifetime of c_str() passed to exception constructor.
            std::stringstream message;
            message << "Expected '" << c << "' Got '" << PeekChar() << "'";
            throw ParseError(message.str().c_str(), m_currentPosition);
        }
        else
        {
            GetChar();
        }
    }


    void QueryParser::SkipWhite()
    {
        while (isspace(PeekChar()))
        {
            GetChar();
        }
    }


    char QueryParser::GetWithEscape()
    {
        char c = PeekChar();
        char const * legalEscapes = "&|\\()\":-";
        if (c == '\\')
        {
            GetChar();
            c = PeekChar();
            if (strchr(legalEscapes, c) != nullptr)
            {
                return GetChar();
            }
            else
            {
                throw ParseError("Bad escape char",
                                 c);
            }
        }
        else
        {
            return GetChar();
        }
    }


    char QueryParser::GetChar()
    {
        char result = PeekChar();
        if (result == '\0')
        {
            throw ParseError("Attempting to read past NULL byte",
                             m_currentPosition);
        }
        ++m_currentPosition;
        m_haveChar = false;
        return result;
    }


    char QueryParser::PeekChar()
    {
        if (!m_haveChar)
        {
            int temp = m_input.get();
            // See https://github.com/BitFunnel/BitFunnel/issues/189.
            if (temp != -1)
            {
                m_nextChar = static_cast<char>(temp);
            }
            else
            {
                m_nextChar = '\0';
            }
            m_haveChar = true;
        }
        return m_nextChar;
    }


    Term::StreamId QueryParser::StreamIdFromText(char const * /*streamName*/) const
    {
        // TODO: Return correct stream id here.
        return 123;
    }


    //*************************************************************************
    //
    // QueryParser::ParseError
    //
    //*************************************************************************
    QueryParser::ParseError::ParseError(char const * message, size_t position)
        : RecoverableError(message),
          m_position(position)
    {
    }


    std::ostream& operator<< (std::ostream &out, const QueryParser::ParseError &e)
    {
        out << std::string(e.m_position, ' ') << '^' << std::endl;
        out << "Parser error (position = " << e.m_position << "): ";
        out << e.what();
        out << std::endl;
        return out;
    }
}
