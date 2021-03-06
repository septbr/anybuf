#include <filesystem>
#include <iterator>

#include "anybuf.hpp"

namespace anybuf
{
    void reader::scan(const std::string &path) noexcept
    {
        auto file = std::ifstream(path);
        auto data = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

        auto &src = _sources[path];
        decltype(data.size()) tpos = std::string::npos, trow = 1, tcol = 1, flag = 0 /** 1:line comment, 2:block comment, 3:string */;
        for (decltype(data.size()) pos = 0, size = data.size(), row = 1, col = 1; pos < size; ++pos, ++col)
        {
            auto ch = data[pos];
            if (tpos == std::string::npos && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
                tpos = pos, trow = row, tcol = col;

            if (tpos != std::string::npos && (pos == size - 1 || !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')))
            {
                decltype(tpos) tend = std::string::npos;
                if (pos < size - 1)
                {
                    if (pos == tpos)
                    {
                        if (ch == '/' && (data[pos + 1] == '/' || data[pos + 1] == '*'))
                            flag = data[++pos] == '/' ? 1 : 2, ++col;
                        else if (ch == '"')
                            flag = 3;
                        else
                            tend = pos;
                    }
                    else if (flag == 1)
                    {
                        if (ch == '\r' || ch == '\n')
                            tend = pos - 1;
                    }
                    else if (flag == 2)
                    {
                        if (ch == '*' && data[pos + 1] == '/')
                            ++col, tend = ++pos;
                    }
                    else if (flag == 3)
                    {
                        if (ch == '\\')
                            ++pos, ++col;
                        else if (ch == '"')
                            tend = pos;
                    }
                    else
                    {
                        tend = pos - 1;
                        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
                            --pos, --col;
                    }
                }

                if (tend != std::string::npos || pos == size - 1)
                {
                    src.tokens.push_back(token(trow, tcol, data.substr(tpos, tend - tpos + 1)));
                    tpos = std::string::npos, flag = 0;
                }
            }

            // \n - UNIX   \r\n - DOS   \r - Mac
            if (ch == '\r' || ch == '\n')
            {
                row++;
                col = 0;
                if (ch == '\r' && pos + 1 < size && data[pos + 1] == '\n')
                    pos++;
            }
        }
    }

    void reader::read_comment(context &context) noexcept
    {
        if (context.pos - 1 >= context.size || !context.last().is_comment())
            context.comment = -1;
        while (!context.eof() && context.curr().is_comment())
            context.comment = context.pos++;
    }
    bool reader::read_to_next(context &context, bool (*where)(const token &), const char *desc) noexcept
    {
        if (context.eof())
            return false;

        read_comment(context);
        if (context.eof())
        {
            _errors.push_back(context.error(context.size - 1, "error at end"));
            return false;
        }
        else if (where && !where(context.curr()))
        {
            _errors.push_back(context.error(desc));
            return false;
        }
        return true;
    }
    bool reader::read_number(context &context, uint64_t &value, bool &negative) noexcept
    {
        if (!read_to_next(context))
            return false;

        negative = context.curr() == "-";
        if ((context.curr() == "+" || context.curr() == "-") && !read_to_next(++context))
            return false;

        value = 0;
        std::string_view text = context.curr().text;
        auto is_hex = text[0] == '0' && text.size() > 2 && (text[1] | 32) == 'x';
        for (std::string_view::size_type i = 0, size = text.size(); i < size; ++i)
        {
            if (!is_hex || i > 1)
            {
                auto ch = text[i];
                auto n = ch - '0', nx = (ch | 32) - 'a';
                if (n >= (i == 0 && size > 1 ? 1 : 0) && n <= 9)
                    value = value * (is_hex ? 16 : 10) + n;
                else if (is_hex && nx >= 0 && nx <= 5)
                    value = value * 16 + nx + 10;
                else
                {
                    _errors.push_back(context.error("invalid integer"));
                    return false;
                }
            }
        }
        ++context;
        return true;
    }
    content_node *reader::read_scopes(context &context, const std::vector<std::size_t> &names, bool upward) const
    {
        for (std::size_t size = context.scopes.size(), i = size; i <= size; --i)
        {
            std::vector<content_node *> nodes; // module, struct
            if (i == 0)
                nodes = _nodes;
            else
            {
                if (auto node = context.scopes[i - 1]; node->type() == node_type::emodule)
                {
                    struct emodule_searcher
                    {
                        std::vector<content_node *> path;
                        std::vector<emodule_node *> modules;
                        emodule_searcher(emodule_node *node)
                        {
                            path.push_back(node);
                            for (auto parent = node->parent; parent; parent = parent->parent)
                                path.insert(path.begin(), parent);
                        }
                        void search(std::vector<content_node *> nodes, std::size_t index = 0)
                        {
                            for (auto node : nodes)
                            {
                                if (node->type() == node_type::emodule && node->name == path[index]->name)
                                {
                                    if (index >= path.size() - 1)
                                    {
                                        modules.push_back(dynamic_cast<emodule_node *>(node));
                                    }
                                    else
                                    {
                                        search(dynamic_cast<emodule_node *>(node)->members, ++index);
                                    }
                                }
                            }
                        }
                    };
                    auto searcher = emodule_searcher(dynamic_cast<emodule_node *>(node));
                    searcher.search(_nodes);

                    for (auto node : searcher.modules)
                    {
                        for (auto member : node->members)
                            nodes.push_back(member);
                    }
                }
                else
                {
                    for (auto member : dynamic_cast<estruct_node *>(node)->members)
                    {
                        if (member->type() != node_type::estruct_member)
                            nodes.push_back(member);
                    }
                }
            }

            for (auto node : nodes)
            {
                std::vector<content_node *> sub_nodes = {node};
                std::size_t i = 0;
                for (auto node : sub_nodes)
                {
                    while (i < names.size())
                    {
                        if (context[names[i]].text != node->name)
                            break;
                        else
                        {
                            if (i == names.size() - 1)
                                return node;

                            if (node->type() == node_type::emodule)
                                sub_nodes = dynamic_cast<emodule_node *>(node)->members;
                            if (node->type() == node_type::estruct)
                                sub_nodes = dynamic_cast<estruct_node *>(node)->members;
                            ++i;
                        }
                    }
                }
            }

            if (!upward)
                break;
        }
        return nullptr;
    }
    type_node *reader::read_type(context &context) noexcept
    {
        std::vector<type_node *> stack;
        while (read_to_next(context))
        {
            decltype(context.pos) comma = -1;
            auto curr = stack.size() > 0 ? stack[stack.size() - 1] : nullptr;
            if (context.curr() == ",")
            {
                comma = context.pos;
                if (auto error = !read_to_next(++context); error || !context.curr().is_entity_name(false))
                {
                    _errors.push_back(context.error(error ? comma : context.pos));
                    if (curr)
                        stack[0]->free();
                    return nullptr;
                }
            }
            auto &identitfier = context.curr().text;
            int type = identitfier == "["                          ? -2
                       : identitfier == "]"                        ? -3
                       : identitfier == "<"                        ? -4
                       : identitfier == ">"                        ? -5
                       : identitfier == keyword(identity::u8)      ? (int)identity::u8
                       : identitfier == keyword(identity::u16)     ? (int)identity::u16
                       : identitfier == keyword(identity::u32)     ? (int)identity::u32
                       : identitfier == keyword(identity::u64)     ? (int)identity::u64
                       : identitfier == keyword(identity::i8)      ? (int)identity::i8
                       : identitfier == keyword(identity::i16)     ? (int)identity::i16
                       : identitfier == keyword(identity::i32)     ? (int)identity::i32
                       : identitfier == keyword(identity::i64)     ? (int)identity::i64
                       : identitfier == keyword(identity::f32)     ? (int)identity::f32
                       : identitfier == keyword(identity::f64)     ? (int)identity::f64
                       : identitfier == keyword(identity::boolean) ? (int)identity::boolean
                       : identitfier == keyword(identity::str)     ? (int)identity::str
                                                                   : -1;
            if (comma != -1 && ((curr->format != identity::map && curr->format != identity::tuple) || curr->values.size() == 0))
            {
                _errors.push_back(context.error(comma));
                if (curr)
                    stack[0]->free();
                return nullptr;
            }

            if (type == -2 || type == -4) // [, <
            {
                bool error = false;
                auto node = new type_node;
                node->format = type == -2 ? identity::tuple : identity::map;
                if (curr)
                {
                    if (curr->format == identity::tuple && curr->format == identity::map)
                    {
                        if (curr->format == identity::map && curr->values.size() >= 2)
                            error = true;
                        else
                            curr->values.push_back(node);
                    }
                    else
                        error = true;
                }
                if (error)
                {
                    delete node;
                    _errors.push_back(context.error("invaild type"));
                    if (curr)
                        stack[0]->free();
                    return nullptr;
                }
                stack.push_back(node);
            }
            else
            {
                type_node *node = curr;
                if (type == -3 || type == -5) // ] >
                {
                    if (curr &&
                        ((curr->format == identity::tuple && curr->values.size() > 0 && type == -3) ||
                         (curr->format == identity::map && curr->values.size() == 2 && type == -5)))
                    {
                        stack.pop_back();
                    }
                    else
                    {
                        _errors.push_back(context.error("invaild type"));
                        if (curr)
                            stack[0]->free();
                        return nullptr;
                    }
                }
                else
                {
                    node = new type_node;
                    node->format = type == -1 ? identity::estruct : (identity)type;

                    if (node->format == identity::estruct)
                    {
                        content_node *elem = nullptr;
                        std::vector<decltype(context.pos)> names;
                        if (read_to_next(
                                context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                        {
                            names.push_back(context.pos);
                            while (read_to_next(++context) && context.curr() == ".")
                            {
                                if (!read_to_next(
                                        ++context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                                    break;
                                names.push_back(context.pos);
                            }
                            context.pos = names[names.size() - 1];
                        }

                        if (names.size() > 0 && !context.eof())
                        {
                            elem = read_scopes(context, names, true);
                            if (!elem)
                                _errors.push_back(context.error("doesn't exist"));
                            else if (auto type = elem->type(); type == node_type::eenum || type == node_type::estruct)
                                node->format = type == node_type::eenum ? identity::eenum : identity::estruct;
                            else
                                _errors.push_back(context.error("invaild type"));
                        }
                        if (!elem)
                        {
                            delete node;
                            if (curr)
                                stack[0]->free();
                            return nullptr;
                        }
                        node->values.push_back(elem);
                    }
                    if (!curr)
                        curr = node;
                    else
                        curr->values.push_back(node);
                }

                /** array */
                auto pos = context.pos;
                bool forward = true;
                do
                {
                    forward = false;
                    read_comment(++context);
                    if (!context.eof() && context.curr() == "[")
                    {
                        read_comment(++context);
                        if (!context.eof() && context.curr() == "]")
                        {
                            auto node_copy = new type_node;
                            node_copy->format = node->format;
                            node_copy->values = std::move(node->values);
                            node->format = identity::array;
                            node->values.push_back(node_copy);
                            pos = context.pos;
                            forward = true;
                        }
                    }
                } while (forward);
                context.pos = pos;
            }

            ++context;
            if (curr && stack.size() == 0)
                return curr;
        }
        return nullptr;
    }

    bool reader::read_import(context &context) noexcept
    {
        read_comment(context);
        if (!context.eof() && context.curr() == keyword(identity::import))
        {
            if (!read_to_next(
                    ++context, [](const token &token) { return token.is_string(); }, "invaild path"))
                return false;

            auto pos_path = context.pos;
            if (!read_to_next(
                    ++context, [](const token &token) { return token == ";"; }, "missing \";\""))
                return false;

            std::filesystem::path import_path(context[pos_path].text.substr(1, context[pos_path].text.size() - 2));
            if (import_path.is_relative())
                import_path = std::filesystem::path(context.path).replace_filename(import_path).lexically_normal();
            if (!import_path.is_absolute() || !std::filesystem::is_regular_file(import_path))
            {
                _errors.push_back(context.error(pos_path, "doesn't exist"));
                return false;
            }
            auto import_path_string = import_path.lexically_normal().string();
            if (_sources.find(import_path_string) == _sources.end())
                scan(import_path_string);

            auto &import_src = _sources[import_path_string];
            if (import_src.status == source::status_type::reading)
            {
                _errors.push_back(context.error(pos_path, "import cycling"));
                return false;
            }

            if (!read(import_path_string, import_src))
                return false;

            ++context.pos;
        }

        return true;
    }
    bool reader::read_emodule(context &context) noexcept
    {
        read_comment(context);
        if (!context.eof() && context.curr() == keyword(identity::emodule))
        {
            std::vector<decltype(context.pos)> names;
            if (!read_to_next(
                    ++context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                return false;
            names.push_back(context.pos);
            while (read_to_next(++context) && context.curr() == ".")
            {
                if (!read_to_next(
                        ++context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                    return false;
                names.push_back(context.pos);
            }
            if (!read_to_next(
                    context, [](const token &token) { return token == "{"; }, "missing \"{\""))
                return false;

            auto *scope = context.scopes.size() > 0 ? context.scopes[context.scopes.size() - 1] : nullptr;
            for (auto &name : names)
            {
                auto node = new emodule_node;
                node->src = context.path;
                node->row = context[name].row;
                node->col = context[name].col;
                node->comment = context.comment != -1 ? context[context.comment].text : "";
                node->name = context[name].text;
                node->parent = scope;
                _nodes.push_back(node);

                if (scope)
                    dynamic_cast<emodule_node *>(scope)->members.push_back(node);
                scope = node;
            }
            context.scopes.push_back(scope);

            ++context;
            while (read_to_next(context))
            {
                if (context.curr() == keyword(identity::emodule))
                {
                    if (!read_emodule(context))
                        return false;
                }
                else if (context.curr() == keyword(identity::eenum))
                {
                    if (!read_eenum(context))
                        return false;
                }
                else if (context.curr() == keyword(identity::estruct))
                {
                    if (!read_estruct(context))
                        return false;
                }
                else
                {
                    break;
                }
            }
            if (!read_to_next(
                    context, [](const token &token) { return token == "}"; }, "missing \"}\""))
                return false;
            ++context;
            context.scopes.pop_back();
        }

        return true;
    }
    bool reader::read_eenum(context &context) noexcept
    {
        read_comment(context);
        if (!context.eof() && context.curr() == keyword(identity::eenum))
        {
            decltype(context.comment) comment = context.comment;

            if (!read_to_next(
                    ++context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                return false;
            if (read_scopes(context, {context.pos}))
            {
                _errors.push_back(context.error("redefinition"));
                return false;
            }

            auto node = new eenum_node;
            node->src = context.path;
            node->row = context.curr().row;
            node->col = context.curr().col;
            node->comment = comment != -1 ? context[comment].text : "";
            node->name = context.curr().text;
            node->parent = context.scopes.size() > 0 ? context.scopes[context.scopes.size() - 1] : nullptr;
            node->format = identity::i32;
            if (node->parent)
            {
                if (node->parent->type() == node_type::emodule)
                    dynamic_cast<emodule_node *>(node->parent)->members.push_back(node);
                else if (node->parent->type() == node_type::estruct)
                    dynamic_cast<estruct_node *>(node->parent)->members.push_back(node);
            }
            else
                _nodes.push_back(node);

            if (!read_to_next(++context))
                return false;
            if (context.curr() == ":")
            {
                if (!read_to_next(++context))
                    return false;
                auto const &type = context.curr().text;
                if (type == keyword(identity::i8))
                    node->format = identity::i8;
                else if (type == keyword(identity::i16))
                    node->format = identity::i16;
                else if (type == keyword(identity::i32))
                    node->format = identity::i32;
                else if (type == keyword(identity::i64))
                    node->format = identity::i64;
                else if (type == keyword(identity::u8))
                    node->format = identity::u8;
                else if (type == keyword(identity::u16))
                    node->format = identity::u16;
                else if (type == keyword(identity::u32))
                    node->format = identity::u32;
                else if (type == keyword(identity::u64))
                    node->format = identity::u64;
                else
                {
                    _errors.push_back(context.error("error value type of enum"));
                    return false;
                }
                if (!read_to_next(++context))
                    return false;
            }
            if (context.curr() != "{")
            {
                _errors.push_back(context.error("missing \"{\""));
                return false;
            }
            while (read_to_next(++context) && context.curr().is_entity_name())
            {
                auto const name = context.pos;
                for (auto const &member : node->members)
                {
                    if (member->name == context[name].text)
                    {
                        _errors.push_back(context.error("redefinition"));
                        return false;
                    }
                }

                auto member = new eenum_member_node;
                member->src = context.path;
                member->row = context[name].row;
                member->col = context[name].col;
                member->comment = context.comment != -1 ? context[context.comment].text : "";
                member->name = context[name].text;
                member->parent = node;

                if (!read_to_next(++context))
                    return false;
                if (context.curr() == "=")
                {
                    uint64_t value = 0;
                    bool negative = false;
                    if (!read_number(++context, value, negative))
                        return false;
                    if (negative ? value > -1LL * INT32_MIN : value > INT32_MAX)
                    {
                        _errors.push_back(context.error(name, "out of value range"));
                        return false;
                    }
                    member->value = negative ? -1LL * value : value;
                    if (!read_to_next(context))
                        return false;
                }
                else
                {
                    if (node->members.size() > 0)
                    {
                        auto const &last = node->members[node->members.size() - 1];
                        member->value = last->value + 1;
                        if (member->value > UINT32_MAX)
                        {
                            _errors.push_back(context.error(name, "out of integer range"));
                            return false;
                        }
                    }
                }

                for (auto const &mem : node->members)
                {
                    if (mem->value == member->value)
                    {
                        _errors.push_back(context.error(name, "value already exists"));
                        return false;
                    }
                }

                int64_t min, max;
                switch (node->format)
                {
                case identity::i8:
                    min = INT8_MIN, max = INT8_MAX;
                    break;
                case identity::i16:
                    min = INT16_MIN, max = INT16_MAX;
                    break;
                case identity::i32:
                    min = INT32_MIN, max = INT32_MAX;
                    break;
                case identity::u8:
                    min = 0, max = UINT8_MAX;
                    break;
                case identity::u16:
                    min = 0, max = UINT16_MAX;
                    break;
                case identity::u32:
                    min = 0, max = UINT32_MAX;
                    break;
                }
                if (member->value < min || member->value > max)
                {
                    _errors.push_back(context.error(name, "out of value range"));
                    return false;
                }

                if (context.curr() == "}")
                    break;
                else if (context.curr() != ",")
                {
                    _errors.push_back(context.error("missing \",\""));
                    return false;
                }
                node->members.push_back(member);
            }

            if (!read_to_next(
                    context, [](const token &token) { return token == "}"; }, "missing \"}\""))
                return false;

            ++context;
        }

        return true;
    }
    bool reader::read_estruct(context &context) noexcept
    {
        read_comment(context);
        if (!context.eof() && context.curr() == keyword(identity::estruct))
        {
            decltype(context.comment) comment = context.comment;

            if (!read_to_next(
                    ++context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                return false;
            if (read_scopes(context, {context.pos}))
            {
                _errors.push_back(context.error("redefinition"));
                return false;
            }

            auto node = new estruct_node;
            node->src = context.path;
            node->row = context.curr().row;
            node->col = context.curr().col;
            node->comment = comment != -1 ? context[comment].text : "";
            node->name = context.curr().text;
            node->parent = context.scopes.size() > 0 ? context.scopes[context.scopes.size() - 1] : nullptr;
            if (node->parent)
            {
                if (node->parent->type() == node_type::emodule)
                    dynamic_cast<emodule_node *>(node->parent)->members.push_back(node);
                else if (node->parent->type() == node_type::estruct)
                    dynamic_cast<estruct_node *>(node->parent)->members.push_back(node);
            }
            else
                _nodes.push_back(node);

            if (!read_to_next(++context))
                return false;
            if (context.curr() == ":")
            {
                do
                {
                    std::vector<decltype(context.pos)> names;
                    if (!read_to_next(
                            ++context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                        return false;
                    names.push_back(context.pos);

                    while (read_to_next(++context) && context.curr() == ".")
                    {
                        if (!read_to_next(
                                ++context, [](const token &token) { return token.is_entity_name(); }, "invaild name"))
                            return false;
                        names.push_back(context.pos);
                    }

                    auto base = read_scopes(context, names, true);
                    if (!base)
                    {
                        _errors.push_back(context.error(names[names.size() - 1], "doesn't exist"));
                        return false;
                    }
                    else if (base->type() != node_type::estruct)
                    {
                        _errors.push_back(context.error(names[names.size() - 1], "invaild struct"));
                        return false;
                    }
                    node->bases.push_back(dynamic_cast<estruct_node *>(base));
                } while (!context.eof() && context.curr() == ",");
            }

            if (!read_to_next(
                    context, [](const token &token) { return token == "{"; }, "missing \"{\""))
                return false;
            context.scopes.push_back(node);
            if (!read_to_next(++context))
                return false;

            while (!context.eof() && context.curr() != "}")
            {
                if (context.curr() == keyword(identity::eenum))
                {
                    if (!read_eenum(context))
                        return false;
                }
                else if (context.curr() == keyword(identity::estruct))
                {
                    if (!read_estruct(context))
                        return false;
                }
                else
                {
                    if (!context.curr().is_entity_name())
                    {
                        _errors.push_back(context.error("invaild name"));
                        return false;
                    }
                    decltype(context.comment) comment = context.comment, name = context.pos;
                    for (auto const &member : node->members)
                    {
                        if (member->name == context[name].text)
                        {
                            _errors.push_back(context.error("redefinition"));
                            return false;
                        }
                    }

                    auto member = new estruct_member_node;
                    member->src = context.path;
                    member->row = context[name].row;
                    member->col = context[name].col;
                    member->comment = comment != -1 ? context[comment].text : "";
                    member->name = context[name].text;
                    member->parent = node;
                    node->members.push_back(member);

                    if (!read_to_next(++context))
                        return false;
                    if (context.curr() == "?") // ?:
                    {
                        ++context;
                        if (context.eof())
                        {
                            _errors.push_back(context.error(context.size - 1));
                            return false;
                        }
                        member->optional = true;
                    }
                    if (context.curr() != ":") // :
                    {
                        _errors.push_back(context.error());
                        return false;
                    }
                    if (!read_to_next(++context))
                        return false;

                    uint64_t value = 0;
                    bool negative = false;
                    if (!read_number(context, value, negative))
                        return false;
                    if (negative || value > 255)
                    {
                        _errors.push_back(context.error(context.pos - 1, "the index should be between 0 and 255"));
                        return false;
                    }
                    for (auto const &member : node->members)
                    {
                        if (member->type() == node_type::estruct_member)
                        {
                            if (dynamic_cast<estruct_member_node *>(member)->index == value)
                            {
                                _errors.push_back(context.error(context.pos - 1, "the index has been repeated"));
                                return false;
                            }
                        }
                    }
                    member->index = value;

                    if (!read_to_next(context))
                        return false;
                    if (context.curr().row == context.last().row && context.curr().col == context.last().col + 1)
                    {
                        _errors.push_back(context.error()); // need a blank character
                        return false;
                    }

                    /* xx xx[], [xx], <xx,xxx> */
                    auto type = read_type(context);
                    if (!type)
                        return false;
                    member->format = type;

                    if (!read_to_next(
                            context, [](const token &token) { return token == ";"; }, "missing \";\""))
                        return false;

                    ++context;
                }
            }

            if (!read_to_next(
                    context, [](const token &token) { return token == "}"; }, "missing \"}\""))
                return false;

            ++context;
            context.scopes.pop_back();
        }

        return true;
    }
    bool reader::read(const std::string &path, reader::source &src) noexcept
    {
        switch (src.status)
        {
        case source::status_type::none:
            src.status = source::status_type::reading;
            break;
        case source::status_type::read:
            return true;
        }

        context context(path, src);
        do
        {
            if (!read_import(context))
                return false;
            read_comment(context);
        } while (!context.eof() && context.curr() == keyword(identity::import));

        while (!context.eof())
        {
            read_comment(context);
            if (context.eof())
                break;

            if (context.curr() == keyword(identity::emodule))
            {
                if (!read_emodule(context))
                    return false;
            }
            else if (context.curr() == keyword(identity::eenum))
            {
                if (!read_eenum(context))
                    return false;
            }
            else if (context.curr() == keyword(identity::estruct))
            {
                if (!read_estruct(context))
                    return false;
            }
            else
            {
                _errors.push_back(context.error());
                return false;
            }
        }

        return true;
    }

    void reader::load(const std::string &path) noexcept
    {
        auto absolute_path = std::filesystem::absolute(path).lexically_normal();
        auto status = std::filesystem::status(absolute_path);
        if (status.type() == std::filesystem::file_type::directory)
        {
            for (auto &entry : std::filesystem::recursive_directory_iterator(absolute_path))
            {
                if (entry.is_regular_file())
                {
                    auto path = entry.path().string();
                    if (auto size = path.size(); size > 7 && path[size - 7] == '.' &&
                                                 path[size - 6] | 32 == 'a' && path[size - 5] | 32 == 'n' &&
                                                 path[size - 4] | 32 == 'y' && path[size - 3] | 32 == 'b' &&
                                                 path[size - 2] | 32 == 'u' && path[size - 1] | 32 == 'f')
                        scan(path);
                }
            }
        }
        else if (status.type() == std::filesystem::file_type::regular)
        {
            scan(absolute_path.string());
        }
    }

    bool reader::read() noexcept
    {
        if (_errors.size() > 0)
            return false;

        for (auto &[path, src] : _sources)
        {
            if (!read(path, src))
                break;
        }
        return _errors.size() == 0;
    }
    void reader::clear() noexcept
    {
        _errors.clear();
        for (auto node : _nodes)
            node->free();
        _nodes.clear();
        _sources.clear();
    }

} // namespace anybuf

namespace anybuf
{
    writer *writer::create(std::string language, const std::string &path, const std::string &package) noexcept
    {
        while (language.size())
        {
            if (std::isspace(language[0]))
                language.erase(0);
            else
                break;
        }
        while (language.size())
        {
            if (std::isspace(language[language.size() - 1]))
                language.erase(language.size() - 1);
            else
                break;
        }

        for (auto &ch : language)
            ch |= 32;

        if (language == "c")
            return new c_writer(path, package);
        else if (language == "cpp" || language == "c++")
            return new cpp_writer(path, package);
        else if (language == "csharp" || language == "c#")
            return new csharp_writer(path, package);
        else if (language == "java")
            return new java_writer(path, package);
        else if (language == "go")
            return new go_writer(path, package);
        else if (language == "rust")
            return new rust_writer(path, package);
        else if (language == "ts" || language == "js" || language == "typescript" || language == "javascript")
            return new typescript_writer(path, package);
        else if (language == "python")
            return new python_writer(path, package);
        else if (language == "lua")
            return new lua_writer(path, package);
        return nullptr;
    }

#pragma region C
    bool c_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool c_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool c_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool c_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion C

#pragma region C++
    bool cpp_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool cpp_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool cpp_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool cpp_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion C++

#pragma region CSharp
    bool csharp_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool csharp_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool csharp_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool csharp_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion CSharp

#pragma region Java
    bool java_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool java_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool java_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool java_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion Java

#pragma region Go
    bool go_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool go_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool go_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool go_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion Go

#pragma region Rust
    bool rust_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool rust_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool rust_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool rust_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion Rust

#pragma region Typescript
    bool typescript_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool typescript_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool typescript_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool typescript_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion Typescript

#pragma region Python
    bool python_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool python_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool python_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool python_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion Python

#pragma region Lua
    bool lua_writer::write_type(std::ofstream &stream, type_node *node) noexcept
    {
        return true;
    }
    bool lua_writer::write_emodule(std::ofstream &stream, emodule_node *node) noexcept
    {
        return true;
    }
    bool lua_writer::write_eenum(std::ofstream &stream, eenum_node *node) noexcept
    {
        return true;
    }
    bool lua_writer::write_estruct(std::ofstream &stream, estruct_node *node) noexcept
    {
        return true;
    }
#pragma endregion Lua
}