// Google test includes
#include "gtest/gtest.h"

// GeNN includes
#include "type.h"

// GeNN transpiler includes
#include "transpiler/errorHandler.h"
#include "transpiler/scanner.h"

using namespace GeNN;
using namespace GeNN::Transpiler;

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
class TestErrorHandler : public ErrorHandlerBase
{
public:
    TestErrorHandler() : m_Error(false)
    {}

    bool hasError() const { return m_Error; }

    virtual void error(size_t line, std::string_view message) override
    {
        report(line, "", message);
    }

    virtual void error(const Token &token, std::string_view message) override
    {
        if(token.type == Token::Type::END_OF_FILE) {
            report(token.line, " at end", message);
        }
        else {
            report(token.line, " at '" + std::string{token.lexeme} + "'", message);
        }
    }

private:
    void report(size_t line, std::string_view where, std::string_view message)
    {
        std::cerr << "[line " << line << "] Error" << where << ": " << message << std::endl;
        m_Error = true;
    }

    bool m_Error;
};
}   // Anonymous namespace

//--------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------
TEST(Scanner, DecimalInt)
{
    TestErrorHandler errorHandler;
    const auto tokens = Scanner::scanSource("1234 4294967295U -2345 -2147483647", errorHandler);
    ASSERT_FALSE(errorHandler.hasError());

    ASSERT_EQ(tokens.size(), 7);
    ASSERT_EQ(tokens[0].type, Token::Type::INT32_NUMBER);
    ASSERT_EQ(tokens[1].type, Token::Type::UINT32_NUMBER);
    ASSERT_EQ(tokens[2].type, Token::Type::MINUS);
    ASSERT_EQ(tokens[3].type, Token::Type::INT32_NUMBER);
    ASSERT_EQ(tokens[4].type, Token::Type::MINUS);
    ASSERT_EQ(tokens[5].type, Token::Type::INT32_NUMBER);
    ASSERT_EQ(tokens[6].type, Token::Type::END_OF_FILE);

    ASSERT_EQ(tokens[0].lexeme, "1234");
    ASSERT_EQ(tokens[1].lexeme, "4294967295");
    ASSERT_EQ(tokens[3].lexeme, "2345");
    ASSERT_EQ(tokens[5].lexeme, "2147483647");
}
//--------------------------------------------------------------------------
TEST(Scanner, HexInt)
{
    TestErrorHandler errorHandler;
    const auto tokens = Scanner::scanSource("0x1234 0xFFFFFFFFU -0x1234 -0x7FFFFFFF", errorHandler);
    ASSERT_FALSE(errorHandler.hasError());

    ASSERT_EQ(tokens.size(), 7);
    ASSERT_EQ(tokens[0].type, Token::Type::INT32_NUMBER);
    ASSERT_EQ(tokens[1].type, Token::Type::UINT32_NUMBER);
    ASSERT_EQ(tokens[2].type, Token::Type::MINUS);
    ASSERT_EQ(tokens[3].type, Token::Type::INT32_NUMBER);
    ASSERT_EQ(tokens[4].type, Token::Type::MINUS);
    ASSERT_EQ(tokens[5].type, Token::Type::INT32_NUMBER);
    ASSERT_EQ(tokens[6].type, Token::Type::END_OF_FILE);

    ASSERT_EQ(tokens[0].lexeme, "0x1234");
    ASSERT_EQ(tokens[1].lexeme, "0xFFFFFFFF");
    ASSERT_EQ(tokens[3].lexeme, "0x1234");
    ASSERT_EQ(tokens[5].lexeme, "0x7FFFFFFF");
}
//--------------------------------------------------------------------------
TEST(Scanner, DecimalFloat)
{
    TestErrorHandler errorHandler;
    const auto tokens = Scanner::scanSource("1.0 0.2 100.0f 0.2f -12.0d -0.0004f", errorHandler);
    ASSERT_FALSE(errorHandler.hasError());

    ASSERT_EQ(tokens.size(), 9);
    ASSERT_EQ(tokens[0].type, Token::Type::SCALAR_NUMBER);
    ASSERT_EQ(tokens[1].type, Token::Type::SCALAR_NUMBER);
    ASSERT_EQ(tokens[2].type, Token::Type::FLOAT_NUMBER);
    ASSERT_EQ(tokens[3].type, Token::Type::FLOAT_NUMBER);
    ASSERT_EQ(tokens[4].type, Token::Type::MINUS);
    ASSERT_EQ(tokens[5].type, Token::Type::DOUBLE_NUMBER);
    ASSERT_EQ(tokens[6].type, Token::Type::MINUS);
    ASSERT_EQ(tokens[7].type, Token::Type::FLOAT_NUMBER);
    ASSERT_EQ(tokens[8].type, Token::Type::END_OF_FILE);

    ASSERT_EQ(tokens[0].lexeme, "1.0");
    ASSERT_EQ(tokens[1].lexeme, "0.2");
    ASSERT_EQ(tokens[2].lexeme, "100.0");
    ASSERT_EQ(tokens[3].lexeme, "0.2");
    ASSERT_EQ(tokens[5].lexeme, "12.0");
    ASSERT_EQ(tokens[7].lexeme, "0.0004");
}
//--------------------------------------------------------------------------
TEST(Scanner, String)
{
    TestErrorHandler errorHandler;
    const auto tokens = Scanner::scanSource("\"hello world\" \"pre-processor\"", errorHandler);
    ASSERT_FALSE(errorHandler.hasError());

    ASSERT_EQ(tokens.size(), 3);
    ASSERT_EQ(tokens[0].type, Token::Type::STRING);
    ASSERT_EQ(tokens[1].type, Token::Type::STRING);
    ASSERT_EQ(tokens[2].type, Token::Type::END_OF_FILE);

    ASSERT_EQ(tokens[0].lexeme, "\"hello world\"");
    ASSERT_EQ(tokens[1].lexeme, "\"pre-processor\"");
}
