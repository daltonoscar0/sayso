// Sayso CLI: read stdin, write normalised text to stdout, one line in, one
// line out.
//
//   sayso normalize --direction tts   "$25"          -> "twenty-five dollars"
//   sayso normalize --direction itn   "twenty-five dollars" -> "$25"
//   sayso normalize --direction tts --explain        -> the CLASSIFY tagging
#include <iostream>
#include <string>

#include "normalizer.h"

using namespace sayso;

static void usage() {
    std::cerr << "usage: sayso normalize [--direction tts|itn] [--explain]\n"
              << "       reads lines from stdin, writes normalised lines to stdout\n";
}

int main(int argc, char** argv) {
    Direction direction = Direction::TTS;
    bool explain = false;

    int i = 1;
    if (i < argc && std::string(argv[i]) == "normalize") ++i;
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--direction" && i + 1 < argc) {
            std::string value = argv[++i];
            if (value == "tts") direction = Direction::TTS;
            else if (value == "itn") direction = Direction::ITN;
            else { usage(); return 2; }
        } else if (arg == "--explain") {
            explain = true;
        } else if (arg == "-h" || arg == "--help") {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (explain) {
            for (const Segment& s : analyse(line, direction))
                std::cout << " [" << s.source << "] " << class_name(s.type)
                          << " -> [" << s.output << "]\n";
        }
        std::cout << normalize(line, direction) << "\n";
    }
    return 0;
}
