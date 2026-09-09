#include "text.h"

#include "../Scalpel/tokenizer.h"
#include "../Scalpel/utf8.h"

namespace sayso {

namespace {

bool is_space(unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// Everything the tokeniser skipped between `from` and `to` comes back as
// PUNCT, one token per run of non-space bytes.
void recover_gap(const std::string& input, size_t from, size_t to,
                 std::vector<Token>& out) {
    size_t i = from;
    while (i < to) {
        if (is_space(static_cast<unsigned char>(input[i]))) { ++i; continue; }
        size_t start = i;
        while (i < to && !is_space(static_cast<unsigned char>(input[i]))) ++i;
        out.push_back(Token(input.substr(start, i - start), TokenType::PUNCT,
                            static_cast<int>(start), static_cast<int>(i)));
    }
}

// Scalpel attaches a trailing full stop to a short token, on the reasonable
// guess that it is an abbreviation: "1980s." comes back as [1980] [s.], and
// "5 km." as [5] [km.]. A span is replaced whole, so any class claiming such a
// token would swallow the sentence's full stop with it, and none of them can
// claim it at all, because "1980s." is not a decade and "km." is not a unit.
// Both were silently failing wherever they ended a sentence.
//
// A token that already contains a full stop is left alone, which is exactly
// the set that wanted the guess: "U.S.", "e.g." and "p.m." keep their dots and
// stay one token.
void split_trailing_stop(std::vector<Token>& out, const Token& t, size_t start) {
    const std::string& text = t.text;
    size_t end = start + text.size();
    bool splittable = text.size() >= 2 && text.back() == '.' &&
                      text.find('.') == text.size() - 1;
    if (!splittable) {
        out.push_back(Token(text, t.type, static_cast<int>(start),
                            static_cast<int>(end)));
        return;
    }
    out.push_back(Token(text.substr(0, text.size() - 1), t.type,
                        static_cast<int>(start), static_cast<int>(end - 1)));
    out.push_back(Token(".", TokenType::PUNCT, static_cast<int>(end - 1),
                        static_cast<int>(end)));
}

}  // namespace

std::vector<Token> tokenize(const std::string& input) {
    Tokenizer scalpel;
    std::vector<Token> raw = scalpel.tokenize(input);

    std::vector<Token> out;
    out.reserve(raw.size());
    size_t cursor = 0;
    for (const Token& t : raw) {
        size_t start = static_cast<size_t>(t.start_index);
        if (start > input.size() || input.compare(start, t.text.size(), t.text) != 0)
            continue;  // defensive: an offset we cannot trust is not usable
        if (start > cursor) recover_gap(input, cursor, start, out);
        size_t end = start + t.text.size();
        split_trailing_stop(out, t, start);
        cursor = end;
    }
    if (cursor < input.size()) recover_gap(input, cursor, input.size(), out);
    return out;
}

std::string fold_case(const std::string& s) {
    std::string out;
    size_t i = 0, len = 0;
    while (i < s.size()) {
        utf8::append(out, utf8::to_lower(utf8::decode(s, i, len)));
        i += len;
    }
    return out;
}

size_t codepoint_length(const std::string& s) {
    size_t i = 0, len = 0, n = 0;
    while (i < s.size()) {
        utf8::decode(s, i, len);
        i += len;
        ++n;
    }
    return n;
}

}  // namespace sayso
