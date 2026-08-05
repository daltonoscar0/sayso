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
// A hair rather than a real penalty: the two readings only ever compete with
// each other, and a larger cost would leak into the classifier, where a bare
// tens word would start looking expensive to any other class.
constexpr float kSpacedCompoundPenalty = 0.01f;

// "negative" instead of "minus" for a leading '-'.
constexpr float kAltSignPenalty = 0.5f;

// Per digit, for the digit-by-digit reading of a number the compositional
// grammar cannot parse (over twelve digits, or a leading-zero string like
// "007"). Priced per digit rather than as a flat entry cost so that it stays
// below the per-character cost of leaving the token alone, while never
// outbidding a genuine reading of a short number.
constexpr float kDigitByDigitPerDigit = 0.5f;

// ---- inside the money grammar ----------------------------------------------

// Priority union, expressed as a weight. A currency's singular branch only
// accepts an amount of exactly one while its plural branch accepts anything,
// so charging the plural branch a hair makes the more specific branch win
// wherever both apply: "$1" verbalises as "one dollar", and "one dollars" is
// still read back as "$1". The same margin separates "one cent" from
// "one cents".
constexpr float kPluralPreferenceMargin = 0.01f;

// Currencies whose spoken forms collide: "ninety-nine cents" fits dollars and
// euros equally well. The dollar is canonical, so every other currency pays a
// hair; it changes nothing where the currency word is unambiguous.
constexpr float kNonDefaultCurrencyPenalty = 0.02f;

// A zero-dollar amount spelled out ("zero dollars and ninety-nine cents")
// rather than as bare cents ("ninety-nine cents").
constexpr float kZeroDollarsPenalty = 1.0f;

// Omitting "and" between the major and minor units.
constexpr float kNoAndPenalty = 0.4f;

// One-digit cents ("$2.5" for two dollars fifty), so that ITN writes "$2.50".
constexpr float kShortCentsPenalty = 0.5f;

// Writing ".00" for a whole amount. "$3" and "$3.00" are the same words, so
// without this the two branches tie and ITN would pick either.
constexpr float kExplicitZeroCentsPenalty = 0.05f;

// A magnitude written as a letter suffix ("$6.5m", "$26.6bn") rather than
// spelled out ("$6.5 million"). Both are read; the spelled form is the one ITN
// writes, being the one that survives being read aloud from the page.
constexpr float kLetterMagnitudePenalty = 0.4f;

// Writing a magnitude word at all, where plain digits would say the same
// thing. "fifty thousand dollars" fits "$50 thousand" and "$50000" equally and
// the two would otherwise tie; the hair makes ITN write the digits, which is
// the same canonicalisation that drops thousands separators. It changes
// nothing for "$2.5 million", whose words no plain numeral produces.
constexpr float kSpelledMagnitudePenalty = 0.05f;

// A space between a currency symbol and its amount, as in "$ 1". Written that
// way often enough to read, never worth producing.
constexpr float kCurrencySpacePenalty = 0.3f;

// ---- inside the date grammar -----------------------------------------------

// A zero-padded month or day ("03/14" rather than "3/14").
constexpr float kZeroPadPenalty = 0.3f;

// A written format other than the canonical M/D/YYYY. ITN therefore emits
// "3/14/2015" whatever spoken form it was given. Kept small: these only ever
// have to beat the canonical format's zero, and a large penalty would make a
// date span look expensive to the classifier, which would rather read
// "2015-03-14" as three cardinals separated by minus signs.
constexpr float kIsoFormatPenalty = 0.5f;
constexpr float kMonthNameFormatPenalty = 0.6f;
constexpr float kDayFirstFormatPenalty = 0.7f;

// Reading a year 2000-2009 as a digit pair ("twenty hundred", "twenty oh
// five") rather than as a cardinal ("two thousand", "two thousand five").
// Only that decade prefers the cardinal reading; 1900 is still "nineteen
// hundred" and 1905 "nineteen oh five".
constexpr float kTwoThousandsPenalty = 1.2f;

// A four-digit year read as a plain cardinal instead of as a digit pair,
// which is what makes 2015 "twenty fifteen" but 2005 "two thousand five".
constexpr float kCardinalYearPenalty = 0.6f;

// A *bare* four-digit number read as a cardinal ("one thousand nine hundred
// eighty-four") where the year reading ("nineteen eighty-four") also fits.
//
// Priority union by margin, once more: the year branch accepts only four
// digits inside the year range, so a hair on the general cardinal branch is
// enough to make the specific one win wherever both apply. A hair rather than
// a real penalty for two reasons — it is paid by every cardinal Sayso reads,
// and ITN must still read the cardinal wording straight back to the same
// digits.
//
// It applies to a bare span only. A number inside a larger semiotic object is
// a quantity, not a year, so MONEY, MEASURE, DECIMAL and FRACTION embed
// cardinal_core_fst() and never see this branch: "$1984" and "1984 m" stay
// "one thousand nine hundred eighty-four".
constexpr float kBareYearMargin = 0.01f;

// A month written short: "Jun 29, 2011", "Sept. 3, 1985". Read only — ITN
// emits M/D/YYYY, so no month name of any length reaches the written tape.
constexpr float kAbbreviatedMonthPenalty = 0.2f;

// A comma before the year in "March 14, 2015" is expected; leaving it out is
// accepted at a small cost.
constexpr float kMissingDateCommaPenalty = 0.3f;

// A day spoken as a cardinal ("march fourteen") rather than an ordinal.
constexpr float kCardinalDayPenalty = 0.8f;

// ---- inside the decimal grammar ---------------------------------------------
//
// A decimal's leading zero can go missing on either tape, and the two losses
// need different costs so that ITN has something to prefer. Speaking "0.5" as
// "point five" is the commoner elision, so it is the cheaper one; that makes
// "point five" write back as "0.5" rather than ".5", while ".5" is still read.
constexpr float kUnspokenLeadingZeroPenalty = 0.4f;
constexpr float kUnwrittenLeadingZeroPenalty = 0.5f;

// ---- inside the fraction grammar --------------------------------------------

// "a half" where "one half" is canonical, and the other way round inside a
// mixed number: "two and a half" is what English says, "two and one half" is
// merely accepted. One weight, applied in whichever direction the branch needs.
constexpr float kArticleNumeratorPenalty = 0.25f;

// A denominator word Sayso reads but will not write: "fourths" for "quarters",
// and the spaced "thirty seconds" for "thirty-seconds".
constexpr float kAltDenominatorPenalty = 0.3f;

// A fraction written as one of Unicode's precomposed characters ("¾", "⅞")
// rather than with a slash. Read, because real text is full of them; not
// written, because "3/4" is the form that survives every encoding.
constexpr float kVulgarFractionPenalty = 0.4f;

// A mixed number written flush against its fraction ("2½"). Only reachable for
// a precomposed character, where it is unambiguous — allowing it for the
// slashed form would make "21/2" read as "two and a half".
constexpr float kMixedNoSpacePenalty = 0.2f;

// ---- inside the time grammar ------------------------------------------------

// A round hour left unsaid. Which way this leans depends on the meridiem:
// "3:00" is "three o'clock", but "3:00 pm" is "three p m", so the cost sits on
// the other branch in each case and one number expresses both preferences.
constexpr float kBareHourPenalty = 0.35f;

// "p.m." where "pm" is canonical, and "3:30pm" where "3:30 pm" is.
constexpr float kDottedMeridiemPenalty = 0.4f;
constexpr float kMeridiemSpacingPenalty = 0.3f;

// A round hour said the wrong way round for its clock. "10:00" is "ten
// o'clock" and "18:00" is "eighteen hundred"; each reading is available to the
// other clock at this cost, so neither is refused and the ordinary one wins.
constexpr float kWrongClockStylePenalty = 0.5f;

// A time written with no minute field at all: "5pm" for "5:00 pm". Read,
// because that is how it is usually written; not written, because a time with
// its minutes spelled out is the form ITN can always produce.
constexpr float kOmittedMinutePenalty = 0.3f;

// The British "10.30pm", where a full stop separates the fields. Only
// reachable when a meridiem follows — without one, "10.30" is a decimal, and
// far more often than it is a time.
constexpr float kDotTimeSeparatorPenalty = 0.4f;

// ---- inside the measure grammar ---------------------------------------------

// A unit spaced the way its neighbours are not: "5km" for "5 km", "50 %" for
// "50%". Each unit declares which side of this it sits on.
constexpr float kMeasureSpacingPenalty = 0.3f;

// A unit spelling Sayso reads but will not write, so that ITN settles on one
// of them: "lbs" where "lb" is canonical, "km2" where "km²" is. Without it the
// two tie and which one comes out changes between builds.
//
// A hair, because that is all a tie needs, and because anything larger loses a
// competition it has no business being in: the alternates are exactly the
// spellings that end in a digit, so "40.285 km2" at a third of a unit was
// dearer than MEASURE("40.285 km") plus CARDINAL("2") and split in two.
constexpr float kAltUnitSpellingPenalty = 0.05f;

// A unit whose *spoken* form belongs to another class as well. "three pounds"
// is sterling far more often than it is weight, and CLASSIFY has no context to
// tell them apart, so the measure reading pays enough to lose that tie. It is
// not enough to lose to PLAIN, so "5 lb" is still weight — nothing writes
// sterling that way.
constexpr float kCurrencyHomographPenalty = 0.25f;

// A unit written in folded case ("°c", "mhz") rather than the case it is
// conventionally spelled with ("°C", "MHz"). CLASSIFY folds a span's case
// before any grammar sees it, so the folded spelling is the only one TTS can
// ever match and the canonical one is the only one ITN will ever write; the
// weight is what makes the second of those true. Small, because it is paid on
// every reading of the units that have a capital in them and means nothing
// beyond breaking that tie.
constexpr float kFoldedCasePenalty = 0.1f;

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

// Entry cost per class. The composite classes are cheaper than their parts so
// that "$25" beats PLAIN "$" plus CARDINAL "25", "3/14/2015" beats three
// separate cardinals, and "5 km" beats CARDINAL "5" plus PLAIN "km".
//
// Only the ordering matters, and only against the alternatives that actually
// overlap. CARDINAL is dearest because every other numeric class contains a
// cardinal somewhere, so a class that has read more of the span than CARDINAL
// could should not have to win on grammar cost alone. FRACTION sits above the
// rest because its span shape — number, slash, number — is a prefix of a date.
constexpr float kMoneyEntry = 0.05f;
constexpr float kDateEntry = 0.05f;
constexpr float kTimeEntry = 0.05f;
constexpr float kMeasureEntry = 0.05f;
constexpr float kDecimalEntry = 0.05f;
constexpr float kFractionEntry = 0.1f;
constexpr float kOrdinalEntry = 0.1f;
constexpr float kCardinalEntry = 0.2f;

}  // namespace sayso
