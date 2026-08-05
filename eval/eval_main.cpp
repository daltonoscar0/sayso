// Sayso's evaluation harness: sentence-level and per-class accuracy in both
// directions, plus a throughput number.
//
// Slice format, one example per line, '#' for comments:
//   semiotic class <TAB> written <TAB> spoken [<TAB> canonical written]
//
// Two scoring modes.
//
// Strict (default) is exact string match after collapsing whitespace.
//
// Relaxed (--relaxed) additionally folds away the conventions that two
// correct normalisers may simply disagree about: hyphens inside compounds
// ("thirty-fourth" vs "thirty fourth"), the British "and" in "nine hundred and
// forty two", "o" for "oh" in "eleven o two", and letter case in "degrees
// Celsius" — none of which is a distinction speech makes. It also strips
// hyphens from the ITN *input*,
// because inverse normalisers are built for ASR output, which has none.
// Relaxed is the only mode in which a cross-system number means anything;
// strict measures conformance to this slice's conventions, which are Sayso's.
//
// The optional fourth column exists because inverse normalisation is only
// defined up to a canonical written form. "march fourteenth twenty fifteen"
// is written 3/14/2015, 2015-03-14, "March 14, 2015" and "14 March 2015";
// Sayso reads all four and writes the first, so scoring ITN against whichever
// form the corpus happened to use measures format preference, not accuracy.
// Where the column is present ITN is scored against it. Where it is absent —
// the real Google TN slice has no such column — ITN is scored against the
// written column, and the report says so.
//
// Build:
//   g++ -std=c++17 -O2 -o sayso_eval eval_main.cpp ../normalizer.cpp \
//       ../classify.cpp ../text.cpp ../numbers.cpp ../money.cpp ../dates.cpp \
//       ../wfst.cpp ../symbols.cpp ../../Scalpel/tokenizer.cpp \
//       ../../Scalpel/char_classes.cpp -I..
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../normalizer.h"

using namespace sayso;

namespace {

struct Example {
    std::string semiotic_class, written, spoken, canonical;
};

// Fold hyphens to spaces and drop a standalone "and", then squeeze.
std::string fold_style(const std::string& s);

std::string squeeze(const std::string& s) {
    std::string out;
    bool in_space = true;
    for (char c : s) {
        bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r';
        if (space) {
            if (!in_space) out += ' ';
        } else {
            out += c;
        }
        in_space = space;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string fold_style(const std::string& s) {
    std::string spaced;
    for (char c : s) spaced += (c == '-' ? ' ' : c);
    std::string out;
    size_t i = 0;
    while (i < spaced.size()) {
        size_t j = spaced.find(' ', i);
        if (j == std::string::npos) j = spaced.size();
        std::string token = spaced.substr(i, j - i);
        // Case is not a spoken distinction, so "degrees Celsius" and "degrees
        // celsius" are the same reading. ASCII only, which is safe byte at a
        // time: every byte of a multi-byte UTF-8 sequence has its high bit set.
        for (char& c : token)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (token != "and") {
            if (!out.empty()) out += ' ';
            // "eleven o two" and "eleven oh two" are two spellings of one
            // spoken word, not two readings of the time.
            out += (token == "o" ? "oh" : token);
        }
        i = j + 1;
    }
    return out;
}

std::string dehyphenate(const std::string& s) {
    std::string out;
    for (char c : s) out += (c == '-' ? ' ' : c);
    return out;
}

// Drop whole tokens that are nothing but punctuation.
//
// The Google TN reference marks punctuation "sil" — written but not spoken —
// so its spoken column has none at all, while Sayso copies punctuation through
// verbatim, which is what a TTS front end must do. Comparing the two without
// this makes almost every sentence a mismatch on a comma, and measures a
// difference of format rather than of normalisation. The written column keeps
// its punctuation, so the ITN direction needs the same treatment for the
// opposite reason: nothing in the spoken input says where the commas were.
std::string drop_punctuation(const std::string& s) {
    static const std::string marks = ".,;:!?()[]{}\"'`-—–…";
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        std::string token = s.substr(i, j - i);
        bool all_marks = !token.empty();
        for (char c : token)
            if (marks.find(c) == std::string::npos) all_marks = false;
        if (!all_marks) {
            if (!out.empty()) out += ' ';
            out += token;
        }
        i = j + 1;
    }
    return out;
}

std::vector<Example> load(const std::string& path) {
    std::vector<Example> examples;
    std::ifstream in(path);
    if (!in) return examples;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream fields(line);
        Example e;
        if (!std::getline(fields, e.semiotic_class, '\t')) continue;
        if (!std::getline(fields, e.written, '\t')) continue;
        if (!std::getline(fields, e.spoken, '\t')) continue;
        if (!std::getline(fields, e.canonical, '\t')) e.canonical = e.written;
        examples.push_back(e);
    }
    return examples;
}

struct Tally {
    int right = 0, total = 0;
    double percent() const { return total ? 100.0 * right / total : 0.0; }
};

void report(const std::string& title, const Tally& overall,
            const std::map<std::string, Tally>& by_class) {
    std::cout << "\n" << title << "\n";
    std::cout << std::string(title.size(), '-') << "\n";
    std::cout << std::left << std::setw(12) << "class" << std::right << std::setw(8)
              << "n" << std::setw(12) << "accuracy" << "\n";
    for (const auto& [name, tally] : by_class)
        std::cout << std::left << std::setw(12) << name << std::right << std::setw(8)
                  << tally.total << std::setw(11) << std::fixed << std::setprecision(1)
                  << tally.percent() << "%\n";
    std::cout << std::left << std::setw(12) << "ALL" << std::right << std::setw(8)
              << overall.total << std::setw(11) << std::fixed << std::setprecision(1)
              << overall.percent() << "%\n";
}

void show_errors(const std::vector<std::string>& errors, size_t limit) {
    if (errors.empty()) return;
    std::cout << "\nfirst " << std::min(limit, errors.size()) << " of " << errors.size()
              << " mismatches:\n";
    for (size_t i = 0; i < errors.size() && i < limit; ++i)
        std::cout << "  " << errors[i] << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string path = "data/google_tn_slice.tsv";
    bool relaxed = false;
    size_t error_limit = 10;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--relaxed") relaxed = true;
        // How many mismatches to print. Ten is enough to notice a pattern;
        // categorising one takes all of them.
        else if (arg.rfind("--errors=", 0) == 0) error_limit = std::stoul(arg.substr(9));
        else path = arg;
    }
    std::vector<Example> examples = load(path);
    if (examples.empty()) {
        std::cerr << "no examples in " << path << "\n"
                  << "run ./generate_slice to write a synthetic one, or place the "
                     "real slice; see eval/README.md\n";
        return 1;
    }
    bool has_canonical = false;
    for (const Example& e : examples)
        if (e.canonical != e.written) has_canonical = true;
    std::cout << "slice: " << path << "  (" << examples.size() << " examples)\n"
              << "scoring: "
              << (relaxed ? "relaxed (hyphens, \"and\", \"o\"/\"oh\" and case folded away)"
                          : "strict (exact match)")
              << "\n"
              << "ITN scored against: "
              << (has_canonical ? "the canonical written column"
                                : "the written column (no canonical column present)")
              << "\n";

    Tally tts_all, itn_all;
    std::map<std::string, Tally> tts_class, itn_class;
    std::vector<std::string> tts_errors, itn_errors;

    auto scoring_started = std::chrono::steady_clock::now();
    auto compare = [&](const std::string& a) {
        return relaxed ? drop_punctuation(fold_style(squeeze(a))) : squeeze(a);
    };
    // The written side keeps its case and hyphens — those are real distinctions
    // there — but loses standalone punctuation for the reason above.
    auto compare_written = [&](const std::string& a) {
        return relaxed ? drop_punctuation(squeeze(a)) : squeeze(a);
    };
    for (const Example& e : examples) {
        std::string got = compare(normalize(e.written, Direction::TTS));
        std::string want = compare(e.spoken);
        ++tts_all.total;
        ++tts_class[e.semiotic_class].total;
        if (got == want) {
            ++tts_all.right;
            ++tts_class[e.semiotic_class].right;
        } else {
            tts_errors.push_back(e.semiotic_class + ": [" + e.written + "] -> [" + got +
                                 "] want [" + want + "]");
        }

        std::string spoken_in = relaxed ? dehyphenate(e.spoken) : e.spoken;
        got = compare_written(normalize(spoken_in, Direction::ITN));
        want = compare_written(e.canonical);
        ++itn_all.total;
        ++itn_class[e.semiotic_class].total;
        if (got == want) {
            ++itn_all.right;
            ++itn_class[e.semiotic_class].right;
        } else {
            itn_errors.push_back(e.semiotic_class + ": [" + e.spoken + "] -> [" + got +
                                 "] want [" + want + "]");
        }
    }

    double cold_seconds = std::chrono::duration<double>(
                              std::chrono::steady_clock::now() - scoring_started)
                              .count();

    report("TTS verbalisation (written -> spoken)", tts_all, tts_class);
    show_errors(tts_errors, error_limit);
    report("ITN (spoken -> written)", itn_all, itn_class);
    show_errors(itn_errors, error_limit);

    // Two throughput numbers, because they differ by an order of magnitude and
    // quoting only one would be misleading.
    //
    // Cold: the scoring pass above, every sentence seen for the first time, in
    // both directions. This is the cost of classifying genuinely novel text —
    // every span of every sentence asked of every grammar.
    //
    // Warm: the slice cycled to 10,000 sentences, which is what a real corpus
    // looks like once the span cache has seen its vocabulary of numbers,
    // amounts and dates.
    const int kSentences = 10000;
    auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < kSentences; ++i)
        normalize(examples[i % examples.size()].written, Direction::TTS);
    double warm_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    long cold_rate = static_cast<long>(2 * examples.size() / cold_seconds);
    std::cout << "\nthroughput\n"
              << "  cold  " << 2 * examples.size() << " novel sentences in " << std::fixed
              << std::setprecision(2) << cold_seconds << "s (" << cold_rate
              << " sentences/s)\n"
              << "  warm  " << kSentences << " sentences in " << std::fixed
              << std::setprecision(2) << warm_seconds << "s ("
              << static_cast<long>(kSentences / warm_seconds) << " sentences/s)\n";
    return 0;
}
