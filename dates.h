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

const Cascade& date_grammar();

// The lowest and highest year an ISO-formatted date may carry. Each year is
// one path through the rewrite machine, so the range is a size decision.
constexpr int kIsoYearFrom = 1900;
constexpr int kIsoYearTo = 2099;

}  // namespace sayso
