#include "parse.h"

namespace anybuf::parse
{
    bool Token::is_identifier() const noexcept
    {
        auto res = _text.size() > 0 && (('A' <= _text[0] && _text[0] <= 'Z') || ('a' <= _text[0] && _text[0] <= 'z'));
        if (res)
        {
            for (auto &&ch : _text)
            {
                if (!(('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z') || ('0' <= ch && ch <= '9') || ch == '_'))
                {
                    res = false;
                    break;
                }
            }
        }
        return res;
    }
    bool Token::is_keyword() const noexcept
    {
        return true;
    }
    bool Token::is_comment() const noexcept
    {
        auto size = _text.size();
        return size > 1 && _text[0] == '/' && (_text[1] == '/' || (_text[1] == '*' && size > 3 && _text[size - 2] == '*') && _text[size - 1] == '/');
    }

    /**
     * read all tokens of a file content
     * @param content file content
     * @return tokens
     */
    Tokens read(const std::string &content) noexcept
    {
        Tokens tokens;
        for (int index = 0, row = 1, col = 1,
                 size = content.size(),
                 beg_index = -1, beg_row = 0, beg_col = 0,
                 comment = 0,
                 end_index = -1;
             index < size; ++index, ++col)
        {
            auto ch = content[index];
            if (beg_index == -1 && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
            {
                beg_index = index, beg_row = row, beg_col = col;
            }
            if (beg_index != -1)
            {
                if (index + 1 < size)
                {
                    auto ch2 = content[index + 1];
                    if (index == beg_index)
                    {
                        // 0 no comment, 1 single line comment, 2 multiline comment
                        comment = ch != '/' ? 0 : ch2 == '/' ? 1 : ch2 == '*' ? 2 : 0;
                        if (comment != 0)
                        {
                            ++index, ++col;
                            continue;
                        }
                    }
                    if (comment == 1)
                    {
                        if (ch2 == '\r' || ch2 == '\n')
                        {
                            end_index = index, comment = 0;
                        }
                    }
                    else if (comment == 2)
                    {
                        if (ch == '*' && ch2 == '/')
                        {
                            end_index = index + 1, comment = 0;
                            ++index, ++col;
                        }
                    }
                    else if (!(('0' <= ch && ch <= '9') || ('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z') || ch == '_') ||
                             !(('0' <= ch2 && ch2 <= '9') || ('A' <= ch2 && ch2 <= 'Z') || ('a' <= ch2 && ch2 <= 'z') || ch2 == '_'))
                    {
                        end_index = index;
                    }
                }
                else
                {
                    end_index = index;
                }
            }

            if (end_index != -1)
            {
                tokens.push_back(Token(content.substr(beg_index, end_index - beg_index + 1), beg_row, beg_col));
                beg_index = -1, end_index = -1;
            }

            // \n - UNIX   \r\n - DOS   \r - Mac
            if (ch == '\r' || ch == '\n')
            {
                row++;
                col = 0;
                if (ch == '\r' && index + 1 < size && content[index + 1] == '\n')
                {
                    index++;
                }
            }
        }
        return tokens;
    }

} // namespace anybuf::parse
