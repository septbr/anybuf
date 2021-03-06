#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace anybuf
{
    enum class identity
    {
        u8 = 1,
        u16,
        u32,
        u64,
        i8,
        i16,
        i32,
        i64,
        f32,
        f64,

        boolean,
        str,

        array,
        tuple,
        map,

        emodule,
        eenum,
        estruct,

        import
    };
    inline const char *keyword(identity identifier)
    {
        switch (identifier)
        {
        case identity::u8:
            return "u8";
        case identity::u16:
            return "u16";
        case identity::u32:
            return "u32";
        case identity::u64:
            return "u64";
        case identity::i8:
            return "i8";
        case identity::i16:
            return "i16";
        case identity::i32:
            return "i32";
        case identity::i64:
            return "i64";
        case identity::f32:
            return "f32";
        case identity::f64:
            return "f64";
        case identity::boolean:
            return "bool";
        case identity::str:
            return "str";
        case identity::emodule:
            return "module";
        case identity::eenum:
            return "enum";
        case identity::estruct:
            return "struct";
        case identity::import:
            return "import";
        default:
            return nullptr;
        }
    }

    enum class node_type
    {
        type,

        emodule,
        eenum,
        eenum_member,
        estruct,
        estruct_member
    };
    class node
    {
    private:
        node_type _type;

    protected:
        node(node_type type) noexcept : _type(type) {}
        node(const node &) = delete;
        node(node &&) = delete;
        node &operator=(const node &) = delete;
        node &operator=(node &&) = delete;

    public:
        virtual ~node() noexcept {}

        node_type type() const noexcept { return _type; }
        virtual void free() noexcept {}
    };

    class type_node : public node
    {
    public:
        /** identity exclude emodule, import */
        identity format;
        /* enum, struct, type_node */
        std::vector<node *> values;

    public:
        type_node() noexcept : node(node_type::type) {}

        void free() noexcept override;
    };

    class content_node : public node
    {
        using node::node;

    public:
        std::string src;
        std::size_t row;
        std::size_t col;

        std::string_view comment;
        std::string_view name;

    public:
        content_node *parent = nullptr;
    };
    class emodule_node : public content_node
    {
    public:
        std::vector<content_node *> members;

    public:
        emodule_node() noexcept : content_node(node_type::emodule) {}

        void free() noexcept override;
    };
    class eenum_member_node : public content_node
    {
    public:
        int64_t value = 0;

    public:
        eenum_member_node() noexcept : content_node(node_type::eenum_member) {}
    };
    class eenum_node : public content_node
    {
    public:
        std::vector<eenum_member_node *> members;

    public:
        /** u8, u16, u32, i8, i16, i32 */
        identity format = identity::i32;

    public:
        eenum_node() noexcept : content_node(node_type::eenum) {}

        void free() noexcept override;
    };
    class estruct_member_node : public content_node
    {
    public:
        uint8_t index;
        type_node *format = nullptr;
        bool optional = false;

    public:
        estruct_member_node() noexcept : content_node(node_type::estruct_member) {}

        void free() noexcept override;
    };
    class estruct_node : public content_node
    {
    public:
        /** estruct_member, eenum, estruct */
        std::vector<content_node *> members;
        std::vector<estruct_node *> bases;

    public:
        estruct_node() noexcept : content_node(node_type::estruct) {}

        void free() noexcept override;
    };

    inline void type_node::free() noexcept
    {
        for (auto value : values)
        {
            if (value->type() == node_type::type)
                dynamic_cast<type_node *>(value)->free();
        }
    }
    inline void emodule_node::free() noexcept
    {
        for (auto member : members)
            member->free();
        members.clear();
    }
    inline void eenum_node::free() noexcept
    {
        for (auto member : members)
            member->free();
        members.clear();
    }
    inline void estruct_member_node::free() noexcept
    {
        if (format)
        {
            format->free();
            format = nullptr;
        }
    }
    inline void estruct_node::free() noexcept
    {
        for (auto member : members)
            member->free();
        members.clear();
        bases.clear();
    }
} // namespace anybuf

namespace anybuf
{
    class reader final
    {
    private:
        struct token
        {
            std::size_t row;
            std::size_t col;
            std::string text;

            token(std::size_t row, std::size_t col, const std::string &text) noexcept : row(row), col(col), text(text) {}

            bool is_string() const noexcept { return text.size() > 1 && text[0] == '"' && text[text.size() - 1] == '"'; }
            bool is_line_comment() const noexcept { return text.size() > 1 && text[0] == '/' && text[1] == '/'; }
            bool is_block_comment() const noexcept { return text.size() > 1 && text[0] == '/' && text[1] == '*'; }
            bool is_comment() const noexcept { return is_line_comment() || is_block_comment(); }
            bool is_entity_name(bool strict = true) const noexcept
            {
                for (decltype(text.size()) i = 0; i < text.size(); ++i)
                {
                    if (text[i] != '_' && (text[i] < 'A' || text[i] > 'Z') && (text[i] < 'a' || text[i] > 'z') && (i == 0 || (text[i] < '0' || text[i] > '9')))
                        return false;
                }
                if (!strict)
                    return text.size() > 0;

                return text.size() > 0 &&
                       text != keyword(identity::i8) && text != keyword(identity::i16) && text != keyword(identity::i32) && text != keyword(identity::i64) &&
                       text != keyword(identity::u8) && text != keyword(identity::u16) && text != keyword(identity::u32) && text != keyword(identity::u64) &&
                       text != keyword(identity::f32) && text != keyword(identity::f64) &&
                       text != keyword(identity::boolean) && text != keyword(identity::str) &&
                       text != keyword(identity::emodule) && text != keyword(identity::eenum) && text != keyword(identity::estruct) &&
                       text != keyword(identity::import);
            }

            bool operator==(const std::string &token) const noexcept { return text == token; }
            bool operator==(const std::string_view &token) const noexcept { return text == token; }
            bool operator==(const char *token) const noexcept { return text == token; }
            bool operator!=(const std::string &token) const noexcept { return text != token; }
            bool operator!=(const std::string_view &token) const noexcept { return text != token; }
            bool operator!=(const char *token) const noexcept { return text != token; }
        };
        struct source
        {
            enum class status_type
            {
                none,
                reading,
                read
            };

            std::vector<token> tokens;
            status_type status = status_type::none;
        };
        struct context
        {
            const std::string path;
            source &src;
            const std::vector<token>::size_type size;

            std::vector<token>::size_type pos;
            std::vector<token>::size_type comment;

            /** module, struct */
            std::vector<content_node *> scopes;

            context(const std::string &path, source &src) : path(path), src(src), size(src.tokens.size()), pos(0), comment(-1) {}

            const token &curr() const { return src.tokens[pos]; }
            const token &next() const { return src.tokens[pos + 1]; }
            const token &last() const { return src.tokens[pos - 1]; }
            bool eof() const noexcept { return pos >= size; }
            std::string error(std::vector<token>::size_type pos, const char *desc = "syntax error") const
            {
                auto &token = src.tokens[pos];
                return path + ':' + std::to_string(token.row) + ':' + std::to_string(token.col) +
                       ' ' + (token.is_string() ? token.text : "\"" + token.text + "\"") +
                       ": " + desc;
            }
            std::string error(const char *desc = "syntax error") const { return error(pos, desc); }
            const token &operator[](std::size_t index) const { return src.tokens[index]; }
            context &operator++()
            {
                ++pos;
                return *this;
            }
        };

    private:
        std::map<std::string, source> _sources;
        std::vector<content_node *> _nodes;

        std::vector<std::string> _errors;

    private:
        /** scan the file */
        void scan(const std::string &path) noexcept;

        /** read comment node */
        void read_comment(context &context) noexcept;
        [[nodiscard]] bool read_to_next(context &context, bool (*where)(const token &) = nullptr, const char *desc = "syntax error") noexcept;
        [[nodiscard]] bool read_number(context &context, uint64_t &value, bool &negative) noexcept;
        [[nodiscard]] content_node *read_scopes(context &context, const std::vector<std::size_t> &names, bool upward = false) const;
        [[nodiscard]] type_node *read_type(context &context) noexcept;
        /** read import node */
        [[nodiscard]] bool read_import(context &context) noexcept;
        /** read module node */
        [[nodiscard]] bool read_emodule(context &context) noexcept;
        /** read enum node */
        [[nodiscard]] bool read_eenum(context &context) noexcept;
        /** read struct node */
        [[nodiscard]] bool read_estruct(context &context) noexcept;
        /** read the source */
        [[nodiscard]] bool read(const std::string &path, source &src) noexcept;

    public:
        reader() noexcept {}
        reader(const reader &) = delete;
        reader(reader &&) = delete;
        reader &operator=(const reader &) = delete;
        reader &operator=(reader &&) = delete;
        ~reader() noexcept { clear(); }

        /**
         * load a file or a directory
         * @param path path of the file
         */
        void load(const std::string &path) noexcept;
        /** read sources */
        [[nodiscard]] bool read() noexcept;
        /** clear */
        void clear() noexcept;

        /** module, enum, struct */
        const std::vector<content_node *> &nodes() const noexcept { return _nodes; }
        /** errors */
        const std::vector<std::string> &errors() const noexcept { return _errors; }
    };
} // namespace anybuf

namespace anybuf
{
    class writer
    {
    protected:
        std::string path;
        std::string package;

        std::vector<std::string> _errors;

    protected:
        virtual bool write_type(std::ofstream &stream, type_node *node) noexcept = 0;
        virtual bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept = 0;
        virtual bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept = 0;
        virtual bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept = 0;

    public:
        /** 
         * @param path output file path
         * @param package root namespace or empty 
         */
        writer(const std::string &path, const std::string &package = "") noexcept : path(path), package(package) {}
        /**
         * write
         */
        bool write(const std::vector<content_node *> &nodes)
        {
            std::ofstream stream(path);
            if (!stream)
            {
                _errors.push_back(path + ": open failed");
                return false;
            }
            for (auto node : nodes)
            {
                auto type = node->type();
                if (type == node_type::emodule)
                {
                    if (!write_emodule(stream, dynamic_cast<emodule_node *>(node)))
                        return false;
                }
                else if (type == node_type::eenum)
                {
                    if (!write_eenum(stream, dynamic_cast<eenum_node *>(node)))
                        return false;
                }
                else if (type == node_type::estruct)
                {
                    if (!write_estruct(stream, dynamic_cast<estruct_node *>(node)))
                        return false;
                }
            }
            return true;
        }
        /** errors */
        const std::vector<std::string> &errors() const noexcept { return _errors; }

    public:
        /** 
         * create a program language writer
         * @param language program language
         * @param path output file path
         * @param package root namespace or empty 
         */
        static writer *create(std::string language, const std::string &path, const std::string &package = "") noexcept;
        static void destroy(writer *writer) noexcept { delete writer; }
    };
} // namespace anybuf

namespace anybuf
{
    /** C program language */
    class c_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** C++ program language */
    class cpp_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** CSharp program language */
    class csharp_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** Java program language */
    class java_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** Go program language */
    class go_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** Rust program language */
    class rust_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** Typescript program language */
    class typescript_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** Python program language */
    class python_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };

    /** Lua program language */
    class lua_writer : public writer
    {
    public:
        using writer::writer;

    protected:
        bool write_type(std::ofstream &stream, type_node *node) noexcept override;
        bool write_emodule(std::ofstream &stream, emodule_node *node) noexcept override;
        bool write_eenum(std::ofstream &stream, eenum_node *node) noexcept override;
        bool write_estruct(std::ofstream &stream, estruct_node *node) noexcept override;
    };
} // namespace anybuf