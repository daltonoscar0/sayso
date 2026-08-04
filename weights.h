#pragma once
// Sayso's whole weighting scheme, in one file.
//
// Weights live in the tropical semiring, so every number here is a *cost* in
// arbitrary units and the cheapest path wins. Two rules keep the table
// legible:
//
//   1. The canonical form of anything costs 0. Every variant Sayso must
//      *accept* but would rather not *produce* carries a positive cost. Since
//      inversion preserves weights, one number governs both directions: a
//      penalty on the written comma makes TTS accept "1,234" and makes ITN
//      emit "1234".
//   2. Costs are small (fractions of a unit) inside a grammar and larger
//      (whole units) between grammars, so a class never wins a span by
//      accumulating cheap internal decisions.
namespace sayso {

// ---- inside the number grammars --------------------------------------------

// Reading or writing a thousands separator. Positive, so "1234" is the
// canonical written form and "1,234" is merely accepted.
constexpr float kCommaCost = 0.75f;

// Leaving a tens-unit compound spaced ("twenty five") instead of hyphenated.
constexpr float kSpacedCompoundPenalty = 0.5f;

// "negative" instead of "minus" for a leading '-'.
constexpr float kAltSignPenalty = 0.5f;

// Per digit, for the digit-by-digit reading of a number the compositional
// grammar cannot parse (over twelve digits, or a leading-zero string like
// "007"). Priced per digit rather than as a flat entry cost so that it stays
// below the per-character cost of leaving the token alone, while never
// outbidding a genuine reading of a short number.
constexpr float kDigitByDigitPerDigit = 0.5f;

// ---- inside the money grammar ----------------------------------------------

// Plural currency word after an amount of exactly one ("one dollars").
// Accepted so the relation is total, never generated.
constexpr float kWrongNumberPenalty = 2.0f;

// A zero-dollar amount spelled out ("zero dollars and ninety nine cents")
// rather than as bare cents ("ninety nine cents").
constexpr float kZeroDollarsPenalty = 1.0f;

// Omitting "and" between dollars and cents.
constexpr float kNoAndPenalty = 0.4f;

// Writing the currency name instead of its symbol ("25 dollars" for "$25").
constexpr float kSpelledCurrencyPenalty = 1.5f;

// ---- inside the date grammar -----------------------------------------------

// A zero-padded month or day ("03/14" rather than "3/14").
constexpr float kZeroPadPenalty = 0.3f;

// A written format other than the canonical M/D/YYYY. ITN therefore emits
// "3/14/2015" whatever spoken form it was given.
constexpr float kIsoFormatPenalty = 2.0f;
constexpr float kMonthNameFormatPenalty = 2.5f;
constexpr float kDayFirstFormatPenalty = 3.0f;

// Reading a year 20xx as a digit pair ("twenty oh five") rather than as a
// cardinal ("two thousand five"). Only the 2000s decade prefers the cardinal
// reading; 1905 is still "nineteen oh five".
constexpr float kOhDecadePenalty = 1.2f;

// A four-digit year read as a plain cardinal instead of as a digit pair,
// which is what makes 2015 "twenty fifteen" but 2005 "two thousand five".
constexpr float kCardinalYearPenalty = 0.6f;

// A comma before the year in "March 14, 2015" is expected; leaving it out is
// accepted at a small cost.
constexpr float kMissingDateCommaPenalty = 0.3f;

// ---- the classifier ---------------------------------------------------------
//
// The classifier is a lattice over token spans: every path covers the whole
// sentence, so the costs below are directly comparable. A span is worth
// tagging when its grammar cost plus its class entry cost undercuts the cost
// of leaving its characters alone.

// Per character of a token left untouched. Long spans are therefore expensive
// to skip, which is what produces longest-match behaviour without a separate
// longest-match rule.
constexpr float kPlainPerChar = 1.0f;

// Entry cost per class. MONEY and DATE are cheaper than their parts so that
// "$25" beats PLAIN "$" plus CARDINAL "25", and "3/14/2015" beats three
// separate cardinals.
constexpr float kMoneyEntry = 0.05f;
constexpr float kDateEntry = 0.05f;
constexpr float kOrdinalEntry = 0.1f;
constexpr float kCardinalEntry = 0.2f;

}  // namespace sayso
