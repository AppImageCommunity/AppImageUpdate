#include <argagg/argagg.hpp>

// needed to make compiler happy, doesn't do anything
int main() {
    argagg::parser argparser{{}};
    argagg::parser_results args;
    return 0;
}
