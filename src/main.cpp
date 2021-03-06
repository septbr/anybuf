#include <iostream>
#include <string>
#include <vector>

#ifdef WIN32
#include <Windows.h>
// set console utf8 code
auto _ = [] { return SetConsoleOutputCP(CP_UTF8); }();
#endif

#include "anybuf.hpp"

void print_node(anybuf::node *node)
{
    auto path = [](anybuf::content_node *node) {
        auto path = std::string(node->name);
        for (auto parent = node->parent; parent != nullptr; parent = parent->parent)
            path = std::string(parent->name) + "." + path;
        return path;
    };

    auto type = node->type();
    if (type == anybuf::node_type::emodule)
    {
        auto emodule = dynamic_cast<anybuf::emodule_node *>(node);
        std::cout << "module: " << path(emodule) << std::endl;
        for (auto member : emodule->members)
            print_node(member);
    }
    else if (type == anybuf::node_type::estruct)
    {
        auto estruct = dynamic_cast<anybuf::estruct_node *>(node);
        std::cout << "struct: " << path(estruct);

        if (estruct->bases.size() > 0)
            std::cout << " : ";
        for (auto i = 0; i < estruct->bases.size(); ++i)
            std::cout << (i > 0 ? ", " : "") << path(estruct->bases[i]);
        std::cout << std::endl;

        for (auto member : estruct->members)
            print_node(member);

        if (!estruct->parent || estruct->parent->type() != anybuf::node_type::estruct)
            std::cout << std::endl;
    }
    else if (type == anybuf::node_type::eenum)
    {
        auto eenum = dynamic_cast<anybuf::eenum_node *>(node);
        std::cout << "enum: " << path(eenum) << " : " << anybuf::keyword(eenum->format) << std::endl;
        for (auto member : eenum->members)
            print_node(member);

        if (!eenum->parent || eenum->parent->type() != anybuf::node_type::estruct)
            std::cout << std::endl;
    }
    else if (type == anybuf::node_type::estruct_member)
    {
        auto member = dynamic_cast<anybuf::estruct_member_node *>(node);
        std ::cout << path(member) << (member->optional ? "?:" : ":") << (int)member->index << " ";
        print_node(member->format);
        std::cout << std::endl;
    }
    else if (type == anybuf::node_type::eenum_member)
    {
        auto member = dynamic_cast<anybuf::eenum_member_node *>(node);
        std ::cout << path(member) << ": " << member->value << std::endl;
    }
    else if (type == anybuf::node_type::type)
    {
        auto type = dynamic_cast<anybuf::type_node *>(node);

        if (type->format == anybuf::identity::tuple)
            std::cout << "[";
        else if (type->format == anybuf::identity::map)
            std::cout << "<";

        auto comma = false;
        for (auto node : type->values)
        {
            if (comma)
                std::cout << ", ";
            comma = true;
            if (node->type() == anybuf::node_type::type)
                print_node(node);
            else
                std::cout << path(dynamic_cast<anybuf::content_node *>(node));
        }

        if (type->format == anybuf::identity::tuple)
            std::cout << "]";
        else if (type->format == anybuf::identity::map)
            std::cout << ">";
        else if (type->format == anybuf::identity::array)
            std::cout << "[]";
        else if (type->format != anybuf::identity::eenum && type->format != anybuf::identity::estruct)
            std::cout << anybuf::keyword(type->format);
    }
}

int main(int argc, char **argv)
{
    anybuf::reader reader;
    reader.load("../doc");
    if (reader.read())
    {
        auto const &nodes = reader.nodes();
        for (auto const node : nodes)
            print_node(node);

        auto writer = anybuf::writer::create("cpp", "../doc/example.hpp");
        writer->write(nodes);
        anybuf::writer::destroy(writer);
    }
    else
    {
        for (auto const &error : reader.errors())
            std::cout << error << std::endl;
    }
    return 0;
}
