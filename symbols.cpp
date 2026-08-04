#include "symbols.h"

#include "../Scalpel/utf8.h"

namespace sayso {

namespace {

// Kind is part of the key: the written character "-" and a hypothetical
// spoken word "-" must not collide.
std::string key_of(const std::string& text, Kind kind) {
    return (kind == Kind::CHAR ? "c\t" : "w\t") + text;
}

}  // namespace

SymbolTable::SymbolTable() {
    texts_.push_back("<eps>");
    kinds_.push_back(Kind::CHAR);
    index_[key_of("<eps>", Kind::CHAR)] = kEpsilon;
}

Label SymbolTable::get(const std::string& text, Kind kind) {
    std::string k = key_of(text, kind);
    auto it = index_.find(k);
    if (it != index_.end()) return it->second;
    Label l = static_cast<Label>(texts_.size());
    texts_.push_back(text);
    kinds_.push_back(kind);
    index_[k] = l;
    return l;
}

SymbolTable& symbols() {
    static SymbolTable table;
    return table;
}

Label ch(const std::string& utf8_char) { return symbols().get(utf8_char, Kind::CHAR); }
Label word(const std::string& w) { return symbols().get(w, Kind::WORD); }

std::vector<Label> chars_of(const std::string& s) {
    std::vector<Label> out;
    size_t i = 0, len = 0;
    while (i < s.size()) {
        uint32_t cp = utf8::decode(s, i, len);
        std::string one;
        utf8::append(one, cp);
        out.push_back(ch(one));
        i += len;
    }
    return out;
}

std::vector<Label> words_of(const std::vector<std::string>& ws) {
    std::vector<Label> out;
    out.reserve(ws.size());
    for (const std::string& w : ws) out.push_back(word(w));
    return out;
}

std::string render(const std::vector<Label>& labels) {
    std::string out;
    bool prev_word = false;
    for (Label l : labels) {
        if (l == kEpsilon) continue;
        bool is_word = symbols().kind(l) == Kind::WORD;
        if (!out.empty() && (is_word || prev_word)) out += ' ';
        out += symbols().text(l);
        prev_word = is_word;
    }
    return out;
}

}  // namespace sayso
