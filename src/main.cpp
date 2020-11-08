#ifdef WIN32
#include <Windows.h>
// set console utf8 code
auto _ = [] { return SetConsoleOutputCP(CP_UTF8); }();
#endif

#include <iostream>
#include <string>
#include <vector>
#include <tuple>

#include "parse.h"

int main(int argc, char **argv)
{
    auto tokens = anybuf::parse::read(R"(((((let data = require('fs').readFileSync('./json_tmp', 'utf8');
// xsxs //*//*a/
/**
 * anybuf::parse::read_tokens
 */ 
/**xsssxs*/
/*xcy*/JSON.parse(data);
struct)))))");
    for(auto token : tokens)
        std::cout << token.text() << std::endl;

    anybuf::parse::Node node(1);
    std::cout << node.index() << std::endl;

    return 0;
}