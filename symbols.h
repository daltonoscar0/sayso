#pragma once
// Shared symbol table for Sayso's transducers.
//
// Every grammar in the library composes with every other, so they all have
// to agree on integer labels, hence one process-wide table rather than a
// per-FST one. Label 0 is reserved for epsilon.
//
// Two kinds of symbol live in the same table:
//   CHAR  one UTF-8 character of the *written* form ("2", "$", ",", " ")
//   WORD  one token of the *spoken* form ("twenty", "twenty-five", "dollars")
// The distinction is only used when serialising a label sequence back to
// text: words are joined with spaces, characters are not.
#include <string>
#include <unordered_map>
#include <vector>

namespace sayso {

using Label = int;
constexpr Label kEpsilon = 0;

enum class Kind { CHAR, WORD };

class SymbolTable {
  public:
    SymbolTable();

    Label get(const std::string& text, Kind kind);
    const std::string& text(Label l) const { return texts_[l]; }
    Kind kind(Label l) const { return kinds_[l]; }
    size_t size() const { return texts_.size(); }

  private:
    std::vector<std::string> texts_;
    std::vector<Kind> kinds_;
    std::unordered_map<std::string, Label> index_;
};

SymbolTable& symbols();

// Interning shorthands used all over the grammar tables.
Label ch(const std::string& utf8_char);
Label word(const std::string& w);

// "3/14" -> [ch("3"), ch("/"), ch("1"), ch("4")], UTF-8 aware so that
// "€" stays one symbol.
std::vector<Label> chars_of(const std::string& s);
std::vector<Label> words_of(const std::vector<std::string>& ws);

// Render a label sequence: adjacent CHARs concatenate, WORDs get spaces.
std::string render(const std::vector<Label>& labels);

}  // namespace sayso
