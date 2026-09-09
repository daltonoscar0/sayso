// Sayso CLI: read stdin, write normalised text to stdout, one line in, one
// line out.
//
//   sayso normalize --direction tts   "$25"          -> "twenty-five dollars"
//   sayso normalize --direction itn   "twenty-five dollars" -> "$25"
//   sayso normalize --direction tts --explain        -> the CLASSIFY tagging
//   sayso normalize --date-order preserve            -> "14 March 2015" spoken
//                                                       "the fourteenth of march ..." 
#include <iostream>
#include <string>

#include "normalizer.h"
#include "report.h"

using namespace sayso;

static void usage() {
    std::cerr << "usage: sayso normalize [--direction tts|itn] [--explain]\n"
              << "                         [--date-order canonical|preserve]\n"
              << "                         [--style plain|phone] [--json]\n"
              << "       reads lines from stdin, writes normalised lines to stdout\n"
              << "\n"
              << "  --date-order canonical  every written date form reaches one\n"
              << "                          spoken form (default)\n"
              << "  --date-order preserve   a date written day first is spoken day\n"
              << "                          first: \"the fourteenth of march\"\n"
              << "  --style plain           the reading a written form always\n"
              << "                          supports (default)\n"
              << "  --style phone           the reading a voice gives it: \"four\n"
              << "                          ninety-nine\", digits said one at a time\n"
              << "  --json                  print the pipeline contract's StageResult,\n"
              << "                          one document per line of input\n";
}

int main(int argc, char** argv) {
    Direction direction = Direction::TTS;
    DateSpeech speech = DateSpeech::Canonical;
    Style style = Style::Plain;
    bool explain = false;
    bool as_json = false;

    int i = 1;
    if (i < argc && std::string(argv[i]) == "normalize") ++i;
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--direction" && i + 1 < argc) {
            std::string value = argv[++i];
            if (value == "tts") direction = Direction::TTS;
            else if (value == "itn") direction = Direction::ITN;
            else { usage(); return 2; }
        } else if (arg == "--date-order" && i + 1 < argc) {
            std::string value = argv[++i];
            if (value == "canonical") speech = DateSpeech::Canonical;
            else if (value == "preserve") speech = DateSpeech::PreserveWrittenOrder;
            else { usage(); return 2; }
        } else if (arg == "--style" && i + 1 < argc) {
            std::string value = argv[++i];
            if (value == "plain") style = Style::Plain;
            else if (value == "phone") style = Style::Phone;
            else { usage(); return 2; }
        } else if (arg == "--json") {
            as_json = true;
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
        if (as_json) {
            // One JSON document per line of input, so a caller can hand a whole
            // batch to one process. Building the grammars is most of what a
            // single call costs.
            std::cout << stage_result_json(line, direction, speech, style) << "\n";
            continue;
        }
        if (explain) {
            for (const Segment& s : analyse(line, direction, speech, style))
                std::cout << " [" << s.source << "] " << class_name(s.type)
                          << " -> [" << s.output << "]\n";
        }
        std::cout << normalize(line, direction, speech, style) << "\n";
    }
    return 0;
}
