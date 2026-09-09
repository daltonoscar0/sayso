#pragma once
// Decade grammar: "1970s" <-> "nineteen seventies", "'80s" <-> "eighties".
//
// A decade is a year's leading pair followed by a pluralised tens word, which
// makes it very nearly the year grammar with a different tail, and exactly
// like it in the one place English is irregular. Speech pairs the digits, so
// 1900s is "nineteen hundreds", except across the millennium, where 2000s is
// "two thousands" and not "twenty hundreds". The same weight that keeps 2005
// out of "twenty oh five" keeps 2000s out of "twenty hundreds".
//
// The Google TN data files decades under DATE. Sayso keeps them apart because
// its DATE grammar is a machine for M/D/YYYY, three fields, reordered between
// writing and speech, and a decade has none of that structure.
#include "wfst.h"

namespace sayso {

const Cascade& decade_grammar();

}  // namespace sayso
