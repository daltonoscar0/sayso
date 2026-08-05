#pragma once
// Date grammar: "3/14/2015" <-> "march fourteenth twenty fifteen".
//
// One canonical machine says how a date is *spoken*, from the M/D/YYYY form.
// A separate, much smaller machine says how a date may be *written*: it
// rewrites the ISO, month-name and day-first surface forms into the canonical
// one, character to character. The grammar is the composition of the two.
//
// That split is what keeps field reordering affordable. Two of the formats
// write a field before the field that is spoken first — ISO writes the year
// first, day-first writes the day before the month — and a transducer can
// only reorder by holding the field in its state, one path per value. Held in
// the rewrite machine, a day costs 31 paths and a year 200; held in the
// spoken grammar, each would cost a copy of the day and year verbalisers.
//
// Format costs live in weights.h. M/D/YYYY is free and therefore canonical:
// whatever spoken form ITN is given, that is the form it writes back.
#include "wfst.h"

namespace sayso {

// Which spoken order a date takes.
//
// `Canonical` is the original behaviour and the default: every written format
// reaches the same spoken form, "march fourteenth twenty fifteen", so one
// spoken form round-trips to one written form and ITN can canonicalise.
//
// `PreserveWrittenOrder` speaks a date the way it was written — "the
// fourteenth of march twenty fifteen" for "14 March 2015" and for an ISO date,
// month first for everything else. That is what Google's Kestrel does, and on
// a held-out corpus slice it is 84% of everything Sayso still disagrees with.
// It costs the property above: two written forms then reach two spoken forms,
// and ITN writes back whichever it was given.
//
// The two share a format stage and a speaking stage and differ only in the
// orientation between them, so the option does not double the grammar.
enum class DateSpeech { Canonical, PreserveWrittenOrder };

const Cascade& date_grammar(DateSpeech speech = DateSpeech::Canonical);

// The lowest and highest year an ISO-formatted date may carry. Each year is
// one path through the rewrite machine, so the range is a size decision.
constexpr int kIsoYearFrom = 1900;
constexpr int kIsoYearTo = 2099;

}  // namespace sayso
