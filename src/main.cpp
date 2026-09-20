#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cli/cli_options.hpp"
#include "cli/version.hpp"
#include "git_diff_pivot/change_selector.hpp"
#include "git_diff_pivot/diff_document.hpp"
#include "git_diff_pivot/output_renderer.hpp"
#include "git_diff_pivot/repeated_change_detector.hpp"
#include "git_diff_pivot/token_interner.hpp"
#include "git_diff_pivot/unified_diff_parser.hpp"

namespace {

void PrintUsage() {
    std::cout << "Usage: git-diff-pivot [options] [file]\n"
                 "Reads a textual Git diff from a file, or from stdin if no file is given,\n"
                 "and prints a compressed, review-oriented view of repeated changes.\n"
                 "\n"
                 "Options:\n"
                 "  --min-length <N>       Minimum common-change length in lines (default: 1)\n"
                 "  --min-occurrences <N>  Minimum occurrences for a common change (default: 2)\n"
                 "  --output <path>        Write output to this file instead of stdout\n"
                 "  --output-type <type>   Output format: txt, md, or json (default: txt, or\n"
                 "                         inferred from --output's file extension)\n"
                 "  -h, --help             Show this help message\n"
                 "  -v, --version          Show the product version\n";
}

void PrintVersion() { std::cout << git_diff_pivot::kProductVersion << '\n'; }

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    git_diff_pivot::CliOptions options;
    try {
        options = git_diff_pivot::ParseCliOptions(args);
    } catch (const std::exception& e) {
        std::cerr << "git-diff-pivot: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    if (options.helpRequested) {
        PrintUsage();
        return EXIT_SUCCESS;
    }

    if (options.versionRequested) {
        PrintVersion();
        return EXIT_SUCCESS;
    }

    std::ifstream inputFile;
    if (options.inputPath) {
        inputFile.open(*options.inputPath, std::ios::in);
        if (!inputFile) {
            std::cerr << "git-diff-pivot: Could not open diff input file: " << *options.inputPath << '\n';
            return EXIT_FAILURE;
        }
    }
    std::istream& diffInput = options.inputPath ? static_cast<std::istream&>(inputFile) : std::cin;

    git_diff_pivot::TokenInterner interner;
    const git_diff_pivot::DiffDocument document = git_diff_pivot::UnifiedDiffParser::Parse(diffInput, interner);

    const git_diff_pivot::RepeatedChangeDetector detector(options.minSequenceLength);
    const auto candidates = detector.Detect(document);

    const git_diff_pivot::ChangeSelector selector(options.minSequenceLength, options.minOccurrenceCount);
    const auto selection = selector.Select(document, candidates);

    std::ofstream outputFile;
    if (options.outputPath) {
        outputFile.open(*options.outputPath, std::ios::out | std::ios::trunc);
        if (!outputFile) {
            std::cerr << "git-diff-pivot: could not open output file: " << *options.outputPath << '\n';
            return EXIT_FAILURE;
        }
    }
    std::ostream& diffOutput = options.outputPath ? static_cast<std::ostream&>(outputFile) : std::cout;

    const git_diff_pivot::OutputRenderer renderer(selection, interner);
    renderer.Render(diffOutput, options.outputFormat);

    return EXIT_SUCCESS;
}
