#pragma once
// How much a caller wants the spoken side to sound like speech.
//
// Sayso's default is the written-faithful reading: "$4.99" is "four dollars and
// ninety-nine cents", because that is the reading a written amount always
// supports and the one inverse normalisation can always undo. A conversational
// voice reading the same amount down a telephone says "four ninety-nine", and a
// caller who wants that is asking for a different reading of the same span, not
// a different grammar.
//
// So the option selects among branches that are all present in one machine,
// exactly as DateSpeech does. Both readings stay in the grammar and stay
// invertible; the weight on each moves. Nothing here is a second pipeline.
//
// The elided readings are the ones that lose information a written form had:
// "four ninety-nine" does not say "dollars", so under Plain they carry a cost
// and under Phone they do not.
namespace sayso {

enum class Style { Plain, Phone };

}  // namespace sayso
