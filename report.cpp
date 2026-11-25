#include "report.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "states.h"

namespace sayso {

namespace {

std::string escape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string quoted(const std::string& s) { return "\"" + escape(s) + "\""; }

// Costs are tropical-semiring weights, so they are already the -log the
// contract's `score` allows. Six decimals is well past where any two paths in
// this grammar differ.
std::string number(float value) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", static_cast<double>(value));
    return buf;
}

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}

// A token as the hint table would have it: no trailing punctuation, since a
// verbaliser copies punctuation through and a pronunciation is about the word.
std::string bare_word(const std::string& token) {
    size_t begin = 0, end = token.size();
    auto punctuation = [](char c) {
        return c == ',' || c == '.' || c == ';' || c == ':' || c == '!' ||
               c == '?' || c == '"' || c == '(' || c == ')';
    };
    while (begin < end && punctuation(token[begin])) ++begin;
    while (end > begin && punctuation(token[end - 1])) --end;
    return token.substr(begin, end - begin);
}

// The classes whose spans are proper names, and which lede should therefore
// leave to itself rather than run a heteronym model over. "Reading" inside
// "Reading Road" is a place, and every piece of evidence a disambiguator would
// use says it is the verb.
bool is_name_span(SemioticClass type) {
    return type == SemioticClass::NAME || type == SemioticClass::ADDRESS ||
           type == SemioticClass::STATE;
}

// Everything else the classifier weighed over characters this decision covers.
//
// Overlap rather than an identical span, because the competitions worth showing
// are mostly between spans of different lengths: "1/2/2026" is DATE over five
// tokens against FRACTION over three, and an exact-span filter would report the
// ambiguity as uncontested.
std::vector<const Reading*> losers(const Segment& winner,
                                   const std::vector<Reading>& considered) {
    std::vector<const Reading*> out;
    for (const Reading& r : considered) {
        if (r.end <= winner.begin || r.begin >= winner.end) continue;
        if (r.begin == winner.begin && r.end == winner.end &&
            r.type == winner.type && r.output == winner.output)
            continue;
        out.push_back(&r);
    }
    std::sort(out.begin(), out.end(), [](const Reading* a, const Reading* b) {
        if (a->cost != b->cost) return a->cost < b->cost;
        return a->output < b->output;
    });
    // Enough to show what the decision was between. The tail of a long span's
    // losers is every sub-reading of every prefix, which is noise.
    constexpr size_t kMost = 6;
    if (out.size() > kMost) out.resize(kMost);
    return out;
}

}  // namespace

std::string stage_result_json(const std::string& input, Direction dir,
                              DateSpeech speech, Style style) {
    Analysis analysis = analyse_fully(input, dir, speech, style);

    std::string json = "{";
    json += "\"stage\":\"sayso\",";
    json += "\"input\":" + quoted(input) + ",";
    json += "\"output\":" + quoted(analysis.output) + ",";

    json += "\"tokens\":[";
    bool first = true;
    for (size_t i = 0; i < analysis.output.size();) {
        while (i < analysis.output.size() && analysis.output[i] == ' ') ++i;
        size_t begin = i;
        while (i < analysis.output.size() && analysis.output[i] != ' ') ++i;
        if (i > begin) {
            if (!first) json += ",";
            json += quoted(analysis.output.substr(begin, i - begin));
            first = false;
        }
    }
    json += "],";

    json += "\"decisions\":[";
    first = true;
    for (size_t s = 0; s < analysis.segments.size(); ++s) {
        const Segment& segment = analysis.segments[s];
        // Copying a token through unchanged is not a decision.
        if (segment.type == SemioticClass::PLAIN) continue;
        if (segment.source == segment.output) continue;

        if (!first) json += ",";
        first = false;
        json += "{";
        json += "\"span\":[" + std::to_string(segment.start_byte) + "," +
                std::to_string(segment.end_byte) + "],";
        // Where this decision's output landed in `output`. The serialiser copies
        // the text between segments through verbatim, so a segment's offsets on
        // the spoken side are not its offsets on the written side and cannot be
        // derived from them -- which is what `Analysis::placements` is for, and
        // until now only `name_spans` used it. A consumer that wants to show a
        // reader which words this stage changed needs to point at the spoken
        // side, and reconstructing the arithmetic downstream would be a second
        // copy of the serialiser.
        for (const Placement& p : analysis.placements) {
            if (p.segment != s) continue;
            json += "\"out_span\":[" + std::to_string(p.out_begin) + "," +
                    std::to_string(p.out_end) + "],";
            break;
        }
        json += "\"surface\":" + quoted(segment.source) + ",";
        json += "\"result\":" + quoted(segment.output) + ",";
        json += "\"kind\":" + quoted(lower(class_name(segment.type))) + ",";
        json += "\"rule\":" +
                quoted(std::string("sayso.") + lower(class_name(segment.type))) + ",";

        json += "\"alternatives\":[";
        bool first_alternative = true;
        for (const Reading* r : losers(segment, analysis.considered)) {
            if (!first_alternative) json += ",";
            first_alternative = false;
            // The span comes with the score because the two are only
            // meaningful together: these readings cover different numbers of
            // characters, so a cheaper one is not a better one. "2nd Dr" reads
            // as ADDRESS "second drive" for 0.05 against ORDINAL "second" for
            // 0.1, and loses anyway, because the sentence still has to pay for
            // "Lee" and the address reading has eaten the doctor.
            json += "{\"result\":" + quoted(r->output) +
                    ",\"score\":" + number(r->cost) +
                    ",\"kind\":" + quoted(lower(class_name(r->type))) +
                    ",\"span\":[" + std::to_string(r->start_byte) + "," +
                    std::to_string(r->end_byte) + "]}";
        }
        json += "],";

        json += "\"score\":" + number(segment.cost) + ",";
        json += "\"note\":null";
        json += "}";
    }
    json += "],";

    json += "\"meta\":{";
    json += "\"style\":" +
            quoted(style == Style::Phone ? "phone" : "plain") + ",";
    json += "\"direction\":" + quoted(dir == Direction::TTS ? "tts" : "itn") + ",";
    json += "\"date_order\":" +
            quoted(speech == DateSpeech::Canonical ? "canonical" : "preserve") + ",";

    // Words in the output that a letter-to-sound model is known to get wrong.
    json += "\"pron_hints\":{";
    first = true;
    std::vector<std::string> reported;
    for (size_t i = 0; i < analysis.output.size();) {
        while (i < analysis.output.size() && analysis.output[i] == ' ') ++i;
        size_t begin = i;
        while (i < analysis.output.size() && analysis.output[i] != ' ') ++i;
        if (i <= begin) continue;
        std::string token = bare_word(analysis.output.substr(begin, i - begin));
        const PronunciationHint* hint = pronunciation_hint(token);
        if (hint == nullptr) continue;
        if (std::find(reported.begin(), reported.end(), token) != reported.end())
            continue;
        reported.push_back(token);
        if (!first) json += ",";
        first = false;
        json += quoted(token) + ":{\"arpabet\":" + quoted(hint->arpabet) +
                ",\"respell\":" + quoted(hint->respell) + "}";
    }
    json += "},";

    // Character spans of the output that are proper names.
    json += "\"name_spans\":[";
    first = true;
    for (const Placement& p : analysis.placements) {
        if (!is_name_span(analysis.segments[p.segment].type)) continue;
        if (!first) json += ",";
        first = false;
        json += "[" + std::to_string(p.out_begin) + "," +
                std::to_string(p.out_end) + "]";
    }
    json += "]";

    json += "}";
    json += "}";
    return json;
}

}  // namespace sayso
