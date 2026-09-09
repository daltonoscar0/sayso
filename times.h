#pragma once
// Time grammar: "3:30 pm" <-> "three thirty p m".
//
// A clock time is two cardinals with a colon between them, except that the
// minute field is not read as a cardinal at all, "3:05" is "three oh five",
// not "three five", and "3:00" is "three o'clock", not "three zero zero". Each
// of those is a branch of the minute field rather than a rule applied to it.
//
// Whether a round hour is said at all depends on the meridiem, which is the
// one piece of real interaction in this grammar: "3:00" is "three o'clock",
// but "3:00 pm" is "three p m". Both readings are in both branches; a single
// weight leans one way in one branch and the other way in the other, so the
// preference is stated once and holds in both directions.
//
// The meridiem is spoken as two words, "p m", which is the convention the
// Google text normalisation data uses and what a speech recogniser emits.
#include "style.h"
#include "wfst.h"

namespace sayso {

const Cascade& time_grammar(Style style = Style::Plain);

// The last hour the grammar reads. Twenty-four-hour times are accepted, so
// "19:30" is "nineteen thirty"; a twelve-hour cap would refuse it.
constexpr int kLastHour = 23;

}  // namespace sayso
