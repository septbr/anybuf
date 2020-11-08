#ifndef __ANYBUF_PARSE_H__
#define __ANYBUF_PARSE_H__

#include <string>
#include <vector>
#include <utility>

namespace anybuf::parse
{
    enum class Keyword : char
    {
        I8 = 1,
        I16,
        I32,
        I64,
        U8,
        U16,
        U32,
        U64,
        F32,
        F64,
        BOOL,
        STR,
        STRUCT,
        ENUM,
        MODULE
    };
    inline std::string to_string(Keyword word) noexcept
    {
        switch (word)
        {
        case Keyword::I8:
            return "i8";
        case Keyword::I16:
            return "i16";
        case Keyword::I32:
            return "i32";
        case Keyword::I64:
            return "i64";
        case Keyword::U8:
            return "u8";
        case Keyword::U16:
            return "u16";
        case Keyword::U32:
            return "u32";
        case Keyword::U64:
            return "u64";
        case Keyword::F32:
            return "f32";
        case Keyword::F64:
            return "f64";
        case Keyword::BOOL:
            return "bool";
        case Keyword::STR:
            return "str";
        case Keyword::STRUCT:
            return "struct";
        case Keyword::ENUM:
            return "enum";
        case Keyword::MODULE:
            return "module";
        default:
            return "";
        }
    }
    inline bool operator==(Keyword word, const std::string &text) noexcept { return to_string(word) == text; }
    inline bool operator==(Keyword word, const char *text) noexcept { return to_string(word) == text; }
    inline bool operator==(const std::string &text, Keyword word) noexcept { return to_string(word) == text; }
    inline bool operator==(const char *text, Keyword word) noexcept { return to_string(word) == text; }
    inline bool operator!=(Keyword word, const std::string &text) noexcept { return to_string(word) != text; }
    inline bool operator!=(Keyword word, const char *text) noexcept { return to_string(word) != text; }
    inline bool operator!=(const std::string &text, Keyword word) noexcept { return to_string(word) != text; }
    inline bool operator!=(const char *text, Keyword word) noexcept { return to_string(word) != text; }

    class Token
    {
    private:
        std::string _text;
        int _row, _col;

    public:
        Token(const std::string &text, int row, int col) : _text(text), _row(row), _col(col) {}
        Token(std::string &&text, int row, int col) : _text(text), _row(row), _col(col) {}
        Token(Token &&token) : _text(std::move(token._text)), _row(token._row), _col(token._col) {}
        Token(const Token &token) : _text(token._text), _row(token._row), _col(token._col) {}

        std::string text() const noexcept { return _text; }
        int row() const noexcept { return _row; }
        int col() const noexcept { return _col; }
        bool is_identifier() const noexcept;
        bool is_keyword() const noexcept;
        bool is_comment() const noexcept;
    };
    inline std::string to_string(Token token) noexcept { return token.text(); }
    inline bool operator==(const Token &token, const std::string &text) noexcept { return token.text() == text; }
    inline bool operator==(const Token &token, const char *text) noexcept { return token.text() == text; }
    inline bool operator==(const std::string &text, const Token &token) noexcept { return token.text() == text; }
    inline bool operator==(const char *text, const Token &token) noexcept { return token.text() == text; }
    inline bool operator!=(const Token &token, const std::string &text) noexcept { return token.text() != text; }
    inline bool operator!=(const Token &token, const char *text) noexcept { return token.text() != text; }
    inline bool operator!=(const std::string &text, const Token &token) noexcept { return token.text() != text; }
    inline bool operator!=(const char *text, const Token &token) noexcept { return token.text() != text; }
    inline bool operator==(Keyword word, const Token &token) noexcept { return to_string(word) == token.text(); }
    inline bool operator==(const Token &token, Keyword word) noexcept { return to_string(word) == token.text(); }
    inline bool operator!=(const Token &token, Keyword word) noexcept { return to_string(word) != token.text(); }
    inline bool operator!=(Keyword word, const Token &token) noexcept { return to_string(word) != token.text(); }

    using Tokens = std::vector<Token>;
    using Index = Tokens::size_type;

    class Node
    {
    public:
        enum class Type
        {
            MODULE,
            STRUCT,
            ENUM,
            STRUCT_ITEM,
            ENUM_ITEM
        };

    private:
        Type _type;

    public:
        Index index;
        Index comment;
        std::vector<Index> children;

        Type type() const { return _type; }

    protected:
        Node(Type type) : _type(type) {}
    };
    class Module : public Node
    {
    public:
        Module() : Node(Type::MODULE) {}
    };
    class Struct : public Node
    {
    public:
        Struct() : Node(Type::STRUCT) {}
    };
    class StructItem : public Node
    {
    public:
        StructItem() : Node(Type::STRUCT_ITEM) {}
    };
    class Enum : public Node
    {
    public:
        Enum() : Node(Type::ENUM) {}
    };
    class EnumItem : public Node
    {
    public:
        EnumItem() : Node(Type::ENUM_ITEM) {}
    };

    /**
     * read all tokens of a file content
     * @param content file content
     * @return tokens
     */
    Tokens read(const std::string &content) noexcept;
} // namespace anybuf::parse

#endif // __ANYBUF_PARSE_H__