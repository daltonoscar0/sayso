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
// Down from 0.5: stacked on kCommaCost it made the whole-span reading of
// "$1,234.5" dearer than MONEY("$1") plus PLAIN(",") plus DECIMAL("234.5"), so
// the amount came apart and a literal comma reached the spoken tape. Each
// feature was affordable alone and only their sum lost; 0.3 made it an exact
// tie, which is no better than losing.
constexpr float kShortCentsPenalty = 0.25f;

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
//
// The ISO one came down from 0.5 when RANGE arrived, so that a date is a
// cheaper reading of "2011-11-11" by a wider margin. The structural guard in
// ranges.cpp is what actually protects the date; this only widens the gap.
constexpr float kIsoFormatPenalty = 0.3f;
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
// a real penalty for two reasons, it is paid by every cardinal Sayso reads,
// and ITN must still read the cardinal wording straight back to the same
// digits.
//
// It applies to a bare span only. A number inside a larger semiotic object is
// a quantity, not a year, so MONEY, MEASURE, DECIMAL and FRACTION embed
// cardinal_core_fst() and never see this branch: "$1984" and "1984 m" stay
// "one thousand nine hundred eighty-four".
constexpr float kBareYearMargin = 0.01f;

// A month written short: "Jun 29, 2011", "Sept. 3, 1985". Read only, ITN
// emits M/D/YYYY, so no month name of any length reaches the written tape.
constexpr float kAbbreviatedMonthPenalty = 0.2f;

// A comma before the year in "March 14, 2015" is expected; leaving it out is
// accepted at a small cost.
constexpr float kMissingDateCommaPenalty = 0.3f;

// A day spoken as a cardinal ("march fourteen") rather than an ordinal.
constexpr float kCardinalDayPenalty = 0.8f;

// ---- inside the abbreviation and letter grammars -----------------------------

// A dotted acronym whose last letter has lost its stop: "U.S" for "U.S.". The
// path must still contain one stop, which is what keeps a bare "a" out.
constexpr float kUndottedFinalLetterPenalty = 0.3f;

// A small integer written as a Roman numeral. Read, never written: ITN's
// spoken "two" meets "2" at kBareYearMargin and "II" at this, so the digits
// win. That is the whole reason Roman numerals live inside the cardinal
// grammar rather than in a class of their own.
constexpr float kRomanNumeralPenalty = 0.5f;

// ---- inside the decade grammar ----------------------------------------------

// The apostrophe of "'70s", which marks the elided century. Read but not
// written: "70s" is the plainer form and the one ITN settles on.
constexpr float kElidedCenturyPenalty = 0.2f;

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
//
// A hair, for the same reason kAltUnitSpellingPenalty is one, an alternate
// spelling only has to break a tie, and anything larger loses a competition it
// has no business in. At 0.3, FRACTION("one thirty second") cost 0.4 against
// CARDINAL("one") plus ORDINAL("thirty second") at 0.3, so the de-hyphenated
// spelling a speech recogniser produces came back as "1 32nd".
constexpr float kAltDenominatorPenalty = 0.05f;

// A fraction written as one of Unicode's precomposed characters ("¾", "⅞")
// rather than with a slash. Read, because real text is full of them; not
// written, because "3/4" is the form that survives every encoding.
constexpr float kVulgarFractionPenalty = 0.4f;

// A mixed number written flush against its fraction ("2½"). Only reachable for
// a precomposed character, where it is unambiguous, allowing it for the
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
// reachable when a meridiem follows, without one, "10.30" is a decimal, and
// far more often than it is a time.
constexpr float kDotTimeSeparatorPenalty = 0.4f;

// ---- the readings a voice prefers and a page does not ------------------------
//
// Four weights the Style option moves. Each sits on a branch that *loses
// information the written form had* -- "four ninety-nine" does not say dollars,
// "six in the evening" does not say eighteen -- so under Style::Plain each is
// dear and the faithful reading wins, and under Style::Phone each is free.
//
// None of them selects a grammar. Both readings are in the machine either way
// and both stay invertible; only the cost moves. That is the same shape as
// DateSpeech, and for the same reason: a second grammar is a second thing to
// keep in step.

// Reading a twenty-four-hour clock as a twelve-hour one with the part of the
// day attached. Two units, which is comfortably more than TIME's own internal
// costs, so under Style::Plain "18:00" is "eighteen hundred" and nothing else.
constexpr float kSpokenClockPenalty = 2.0f;

// A price with its currency words dropped: "four ninety-nine".
constexpr float kElidedPriceUnits = 2.0f;

// The hair the *unit-bearing* reading pays under Style::Phone, and only when a
// cents field is present. Without it the two readings of "$4.99" enter at the
// same cost and tie, and which one came out would depend on the order the
// branches were unioned. It is deliberately not paid by "$5" or "$0.99", each
// of which has one reading and should keep it.
constexpr float kPricePreferenceMargin = 0.01f;

// A fraction read with an article and no numerator: "half" for 1/2, "a third"
// for 1/3. It is the reading a voice gives a bare fraction and the reading a
// page never wants, since "one half" is what round-trips and "half" has to be
// told what it was.
//
// Twelve rather than the two the other three take, and the size is derived
// rather than chosen. The branch that says "half a cup" reads the noun as well
// as the fraction, and every character it reads is a character PLAIN no longer
// charges for -- so the branch arrives with a discount equal to the length of
// the noun, and the penalty has to be larger than that or the elided reading
// wins under Style::Plain anyway. It was 2.0 first, and "1/2 cup" came out
// "half a cup" on a page: 0.1 + 2.0 beat 0.1 + 3.0 for leaving "cup" alone.
// The longest noun in the table is "tablespoon", so twelve clears it.
constexpr float kBareFractionArticle = 12.0f;

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
// not enough to lose to PLAIN, so "5 lb" is still weight, nothing writes
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

// ---- inside the address grammar ---------------------------------------------

// A named street ending in an ambiguous suffix, with no number in front of it:
// "Wall St", "Lee Dr".
//
// The first attempt at this was four units, sized to beat PLAIN plus ABBREV
// over the same characters, and it was the wrong instrument. A cost that has to
// exceed the length of the street name cannot exist: "Lee" is three characters
// and "Mount" is five, so any value that keeps "Mount St. Helens" a saint also
// throws away "Lee Dr", and the two differ by nothing a weight can see.
//
// What separates them is word order, which is structure, not cost. An
// abbreviation *before* a capitalised name is a title and belongs to NAME; one
// *after* it is a suffix and belongs here. "Dr. Lee" has no parse in this
// grammar and "Lee Dr" has none in that one, so the ambiguity the class was
// written for mostly dissolves before any weight is consulted.
//
// What is left is the handful of spans both classes can read, where a saint or
// a doctor is the likelier reading of a capital beside "St" or "Dr". A hair is
// all that needs, and a hair is all this is.
constexpr float kUnanchoredAmbiguousSuffix = 0.5f;

// A street name passed through character for character costs nothing, unlike
// every other span Sayso claims. It can afford to because the name must be
// capitalised, which is a structural bound rather than a priced one: "band on
// Reading Rd" has no parse in which "band" and "on" are name words, so there is
// no greedy reading left for a weight to have to discourage.

// ---- inside the phone grammar -----------------------------------------------

// Reading a digit string one digit at a time where the number grammars would
// read it as a number, under Style::Plain.
//
// This is the one weight in the file that a caller moves. Under Style::Phone it
// is zero, and the digit-by-digit reading of a bare "5551212121" wins on
// entering cheaper than CARDINAL; under Style::Plain it stands, and CARDINAL
// wins. Both readings are in the machine either way, which is the point: the
// style option does not select a grammar, it moves a cost inside one.
//
// Bounded below by CARDINAL, which enters at 0.2 and reads a plain digit run
// for almost nothing, and above by PLAIN, which would charge one per character
// for leaving a seven-digit string alone. Three sits clear of both.
constexpr float kWrittenNumberReading = 3.0f;

// Reading a digit run flat, with no pause where a phone number's groups would
// be. A hair, and only ever paid where a grouped reading of the same digits
// also fits, which is what makes ten run-together digits a phone number rather
// than ten unknown ones.
constexpr float kUngroupedDigitsPenalty = 0.01f;

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
// rest because its span shape, number, slash, number, is a prefix of a date.
constexpr float kMoneyEntry = 0.05f;
constexpr float kDateEntry = 0.05f;
// Dearer than CARDINAL, and that ordering is the whole point. Read backwards,
// a clock time and a year are the same words: "nineteen forty five" fits
// TIME("19:45") and CARDINAL("1945") equally, and at 0.05 the clock won every
// one of them, 588 of the 1000 years in 1100-2099 failed to round-trip, and
// 14% of the corpus's spoken sentences grew a colon. Years are overwhelmingly
// the commoner reading of a bare pair, so CARDINAL takes it and TIME keeps
// everything a cardinal cannot span: "3:30" written, "three thirty p m"
// spoken. The cost is that a bare spoken "twelve thirty" writes as 1230.
constexpr float kTimeEntry = 0.25f;
constexpr float kDecadeEntry = 0.05f;
constexpr float kAbbreviationEntry = 0.05f;
constexpr float kLettersEntry = 0.05f;
// Much dearer than the classes it overlaps, and the one entry cost here that
// is derived rather than chosen. A range's shape, number, separator, number,
// is the *prefix* of an ISO date, so RANGE plus a trailing cardinal has to
// lose to DATE over the same characters:
//
//   DATE("2011-11-11")                 = kDateEntry + kIsoFormatPenalty = 0.35
//   RANGE("2011-11") + CARDINAL("-11") = kRangeEntry + 0.21
//
// so kRangeEntry must exceed 0.14. It is also bounded above, because an
// unspaced range must outbid the reading where the second operand is a
// negative number:
//
//   CARDINAL("1") + CARDINAL("-10")    = 0.42
//
// leaving a window of (0.14, 0.42). Both bounds were found the hard way:
// "2011-11-11" read as "twenty eleven to eleven minus eleven" until the lower
// one was respected, and "1-10" as "one minus ten" until the upper one was.
// Zero-padding had hidden the first, "2015-03-14" was only ever safe because
// reading "03" costs the cardinal grammar a digit-by-digit fallback.
//
// A genuine spaced range wins on a different margin entirely: leaving "5 - 0"
// alone costs PLAIN 1.4.
constexpr float kRangeEntry = 0.25f;

// Ordinary, because the guard that keeps dates safe is structural: four
// hyphen-joined groups, which no date has. See telephone.cpp for why a weight
// could not have done it.
constexpr float kTelephoneEntry = 0.1f;
constexpr float kMeasureEntry = 0.05f;
constexpr float kDecimalEntry = 0.05f;
constexpr float kFractionEntry = 0.1f;
constexpr float kOrdinalEntry = 0.1f;
constexpr float kCardinalEntry = 0.2f;

// The classes a conversational voice needs, all of them composite: each reads
// its own context, so each is cheap for the same reason MONEY and DATE are.
// A composite must undercut the sum of its parts, and the part these overlap
// is ABBREV at 0.05 plus a PLAIN word beside it, so they enter level with
// ABBREV and win on having read the word.
constexpr float kAddressEntry = 0.05f;
// A hair under LETTERS, which is the only class NAME ever ties with. Both read
// dotted capitals, and where both match, NAME has matched strictly more: three
// initials and their stops, against LETTERS' rule for dotted shapes in general.
// The more constrained reading should win, and at equal entry costs the winner
// was whichever the registry happened to list first.
constexpr float kNameEntry = 0.04f;

// A degree after a name -- "MD", "PhD" -- which is also a postal code, and the
// postal reading is the commoner one after a comma. Enough to put NAME above
// STATE's 0.1 entry and no more.
constexpr float kDegreeSuffixPenalty = 0.2f;

// Each capitalised word a proper-name class passes through without saying
// anything about it.
//
// A tie-break, and the smallest one in this file. "Rev Dr King" parses two ways
// for exactly the same cost: two titles and one name, or one title and two
// names, because "Dr" is a capital followed by a lowercase letter and so is a
// perfectly good name word. Both readings are right about the characters and
// only one is right about the sentence. Charging a hair for a word the grammar
// merely carried, and nothing for one it expanded, prefers the reading that
// explains more of what it read -- so "Reverend Doctor King" beats "Reverend Dr
// King" by half a hundredth of a unit.
//
// It has to stay far below every margin in the classifier, since ADDRESS and
// STATE carry names too and a street with three words in it should not become
// measurably dearer than one with two.
constexpr float kUnexplainedNameWord = 0.005f;
// Dearer, because a bare two-letter span is a great deal likelier to be an
// acronym or a word than a state: "IN", "OR", "HI" and "ME" are all postal
// codes. The grammar answers that structurally by demanding "City, XX", and
// this keeps the class off anything shorter.
constexpr float kStateEntry = 0.1f;
// A hair under TELEPHONE, whose job it shares and whose guard is likewise
// structural rather than priced.
//
// The two overlap on exactly one shape: a number written with a country code,
// "1-555-121-2121", which has the four hyphen-joined groups TELEPHONE's ISBN
// guard looks for. At equal entry costs TELEPHONE won it on registry order and
// read it flat, with no grouping and no pauses. Where both parse, PHONE has
// matched the stricter shape -- three digits, three digits, four digits -- so
// PHONE should have it. An ISBN does not parse here at all: "978-0-131-40221-8"
// has a one-digit second group and every group in this grammar is fixed width.
constexpr float kPhoneEntry = 0.09f;

}  // namespace sayso
