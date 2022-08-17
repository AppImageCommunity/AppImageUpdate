// system headers
#include <iostream>

// library headers
#include <argagg/argagg.hpp>

// local headers
#include "signaturevalidator.h"
#include "util/updatableappimage.h"
#include "util/util.h"

using namespace appimage::update;
using namespace appimage::update::signing;
using namespace appimage::update::util;

int main(int argc, char** argv) {
    argagg::parser parser{{
        {"help", {"-h", "--help"}, "Display this help text."},
    }};

    argagg::parser_results args;

    try {
        args = parser.parse(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    const auto showUsage = [argv, &parser]() {
        std::cerr << "Validate signatures within AppImage. For testing purposes." << std::endl << std::endl;
        std::cerr << "Usage: " << argv[0] << " [options...] [<path to AppImage>]" << std::endl << std::endl;
        std::cerr << parser;
    };

    if (args["help"]) {
        showUsage();
        return EXIT_SUCCESS;
    }

    if (args.pos.size() != 1) {
        showUsage();
        return EXIT_FAILURE;
    }

    UpdatableAppImage appImage(args.pos.front());

    if (appImage.readSignature().empty()) {
        std::cerr << "Error: AppImage not signed" << std::endl;
        return 1;
    }

    SignatureValidator validator;
    const auto result = validator.validate(appImage);

    std::cerr << "Validation result: ";
    switch (result.type()) {
        case SignatureValidationResult::ResultType::SUCCESS: {
            std::cerr << "validation successful";
            break;
        }
        case SignatureValidationResult::ResultType::WARNING: {
            std::cerr << "validation yielded warning state";
            break;
        }
        case SignatureValidationResult::ResultType::ERROR: {
            std::cerr << "validation failed";
            break;
        }
    }
    std::cerr << std::endl;

    if (!result.keyFingerprints().empty()) {
        std::cerr << "Signatures found with key fingerprints: " << join(result.keyFingerprints(), ", ") << std::endl;
    }

    std::cerr << "====================" << std::endl;

    std::cerr << "Validator report:" << std::endl
              << result.message() << std::endl;

    return EXIT_SUCCESS;
}
