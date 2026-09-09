#pragma once
// The Scalpel bridge.
//
// Sayso does not tokenise. It runs Scalpel's FSM tokeniser and repairs one
// known gap: Scalpel's START state only reacts to LETTER, DIGIT, PUNCT and
// PERIOD, so characters it classifies as HYPHEN, APOSTROPHE or OTHER are
// dropped when they begin a token, which is exactly what '-' in "2015-03-14"
// and '€' in "€40" do. Rather than fork the tokeniser, this adapter compares
// token offsets against the input and re-inserts anything that went missing.
#include <string>
#include <vector>

#include "../Scalpel/token.h"

namespace sayso {

// Tokens with byte offsets that actually bracket their text: end_index is one
// past the last byte, unlike Scalpel's, which is one before the next
// character.
std::vector<Token> tokenize(const std::string& input);

// ASCII/Latin-1 case folding, applied before a span reaches a grammar so the
// grammars can be written in one case.
std::string fold_case(const std::string& s);

size_t codepoint_length(const std::string& s);

}  // namespace sayso
