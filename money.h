#pragma once
// Currency grammar: "$25.10" <-> "twenty-five dollars and ten cents".
//
// One machine per currency symbol, unioned. Each is written the natural way —
// read the symbol and emit nothing, read the amount, then emit the currency
// word — which means the currency name lands after the amount in speech even
// though the symbol precedes it in writing. That reordering is what makes
// currency worth an FST rather than a substitution.
#include "wfst.h"

namespace sayso {

const Cascade& money_grammar();

}  // namespace sayso
