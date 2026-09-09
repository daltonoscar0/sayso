#include "states.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "proper.h"
#include "weights.h"

namespace sayso {

namespace {

struct State {
    const char* code;
    const char* spoken;
};

// The fifty states, the District, and the two commonwealths a US voice reads
// often enough to matter. Codes are USPS Publication 28, Appendix B, "Two-Letter
// State and Possession Abbreviations".
const std::vector<State>& states() {
    static const std::vector<State> table = {
        {"AL", "Alabama"},       {"AK", "Alaska"},        {"AZ", "Arizona"},
        {"AR", "Arkansas"},      {"CA", "California"},    {"CO", "Colorado"},
        {"CT", "Connecticut"},   {"DE", "Delaware"},      {"FL", "Florida"},
        {"GA", "Georgia"},       {"HI", "Hawaii"},        {"ID", "Idaho"},
        {"IL", "Illinois"},      {"IN", "Indiana"},       {"IA", "Iowa"},
        {"KS", "Kansas"},        {"KY", "Kentucky"},      {"LA", "Louisiana"},
        {"ME", "Maine"},         {"MD", "Maryland"},      {"MA", "Massachusetts"},
        {"MI", "Michigan"},      {"MN", "Minnesota"},     {"MS", "Mississippi"},
        {"MO", "Missouri"},      {"MT", "Montana"},       {"NE", "Nebraska"},
        {"NV", "Nevada"},        {"NH", "New Hampshire"}, {"NJ", "New Jersey"},
        {"NM", "New Mexico"},    {"NY", "New York"},      {"NC", "North Carolina"},
        {"ND", "North Dakota"},  {"OH", "Ohio"},          {"OK", "Oklahoma"},
        {"OR", "Oregon"},        {"PA", "Pennsylvania"},  {"RI", "Rhode Island"},
        {"SC", "South Carolina"},{"SD", "South Dakota"},  {"TN", "Tennessee"},
        {"TX", "Texas"},         {"UT", "Utah"},          {"VT", "Vermont"},
        {"VA", "Virginia"},      {"WA", "Washington"},    {"WV", "West Virginia"},
        {"WI", "Wisconsin"},     {"WY", "Wyoming"},       {"DC", "Washington D C"},
        {"PR", "Puerto Rico"},   {"GU", "Guam"},
    };
    return table;
}

const char* kZipDigit[10] = {"oh",   "one", "two",   "three", "four",
                             "five", "six", "seven", "eight", "nine"};

// Five digits, said one at a time, and the ZIP+4 that sometimes follows.
Wfst zip_code() {
    std::vector<Wfst> digits;
    for (int d = 0; d <= 9; ++d)
        digits.push_back(
            rule(std::string(1, static_cast<char>('0' + d)), {kZipDigit[d]}));
    Wfst digit = union_of(digits);
    Wfst five = concat_of({digit, digit, digit, digit, digit});
    Wfst plus_four =
        concat_of({chars_in("-"), digit, digit, digit, digit});
    return concat_of({five, optional_of(plus_four)});
}

Wfst build_state() {
    // The comma is the whole guard. A postal code is only a state when a place
    // name and a comma stand in front of it; on its own it is two letters and
    // LETTERS is right about it.
    //
    // The codes are unioned before the place name is concatenated onto them, so
    // that this grammar holds one copy of the capitalised-word machine rather
    // than fifty-two. See addresses.cpp.
    Wfst head = concat_of({proper_name(3), identical(","), gap()});
    std::vector<Wfst> codes;
    for (const State& s : states()) codes.push_back(rule(s.code, {s.spoken}));
    return connect(concat_of({head, union_of(codes),
                              optional_of(concat_of({gap(), zip_code()}))}));
}

}  // namespace

const Cascade& state_grammar() {
    static const Wfst core = build_state();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

// Every ARPAbet string below was read out of the CMU Pronouncing Dictionary
// (cmudict 0.7b), not written from memory, and the entries are quoted as that
// file has them. Three of the first guesses this table was drafted with were
// wrong -- Arkansas ends AA2 and not AO2, Boise is B OY1 Z IY0 and not S, and
// Arizona takes IH0 rather than AH0 -- which is the argument for looking every
// one of them up.
//
// "reading" is the reason the table exists. cmudict's default entry is the
// verb, R IY1 D IH0 NG, and the town is the second variant, R EH1 D IH0 NG. A
// G2P model handed the bare word takes the first, so a verbaliser that expands
// its way into a place name and says nothing about it has made the sentence
// worse rather than better.
//
// The list is deliberately short. Anything longer is a pronunciation
// dictionary, and shipping one of those from the text normaliser would be
// doing lede's job with none of lede's evidence.
const std::vector<PronunciationHint>& pronunciation_hints() {
    static const std::vector<PronunciationHint> table = {
        // The states whose spelling misleads, and the one that makes the point.
        {"Arkansas",      "AA1 R K AH0 N S AA2",        "AR-kuhn-saw"},
        {"Kansas",        "K AE1 N Z AH0 S",            "KAN-zuhs"},
        {"Illinois",      "IH2 L AH0 N OY1",            "il-uh-NOY"},
        {"Connecticut",   "K AH0 N EH1 T AH0 K AH0 T",  "kuh-NET-i-kut"},
        {"Missouri",      "M AH0 Z UH1 R IY0",          "muh-ZUR-ee"},
        {"Louisiana",     "L UW0 IY2 Z IY0 AE1 N AH0",  "loo-ee-zee-AN-uh"},
        {"Massachusetts", "M AE2 S AH0 CH UW1 S AH0 T S", "mas-uh-CHOO-sits"},
        {"Michigan",      "M IH1 SH IH0 G AH0 N",       "MISH-i-guhn"},
        {"Oregon",        "AO1 R AH0 G AH0 N",          "OR-uh-guhn"},
        {"Nevada",        "N AH0 V AA1 D AH0",          "nuh-VAH-duh"},
        {"Maryland",      "M EH1 R AH0 L AH0 N D",      "MER-uh-luhnd"},
        {"Delaware",      "D EH1 L AH0 W EH2 R",        "DEL-uh-wair"},
        {"Hawaii",        "HH AH0 W AY1 IY2",           "huh-WY-ee"},
        {"Iowa",          "AY1 AH0 W AH0",              "EYE-uh-wuh"},
        {"Ohio",          "OW0 HH AY1 OW0",             "oh-HY-oh"},
        {"Idaho",         "AY1 D AH0 HH OW2",           "EYE-duh-hoh"},
        {"Vermont",       "V ER0 M AA1 N T",            "vur-MONT"},
        {"Arizona",       "EH2 R IH0 Z OW1 N AH0",      "er-i-ZOH-nuh"},

        // The place names a letter-to-sound model reliably mangles.
        {"Reading",       "R EH1 D IH0 NG",             "RED-ing"},
        {"Worcester",     "W UH1 S T ER0",              "WUUS-tur"},
        {"Greenwich",     "G R EH1 N IH0 CH",           "GREN-ich"},
        {"Leicester",     "L EH1 S T ER0",              "LES-tur"},
        {"Gloucester",    "G L AA1 S T ER0",            "GLOS-tur"},
        {"Norwich",       "N AO1 R W IH0 CH",           "NOR-wich"},
        {"Warwick",       "W AO1 R W IH0 K",            "WOR-wik"},
        {"Thames",        "T EH1 M Z",                  "TEMZ"},
        {"Edinburgh",     "EH1 D AH0 N B ER0 OW0",      "ED-in-bur-oh"},
        {"Berkeley",      "B ER1 K L IY0",              "BURK-lee"},
        {"Houston",       "HH Y UW1 S T AH0 N",         "HYOO-stuhn"},
        {"Spokane",       "S P OW0 K AE1 N",            "spoh-KAN"},
        {"Boise",         "B OY1 Z IY0",                "BOY-zee"},
        {"Tucson",        "T UW1 S AA2 N",              "TOO-sahn"},
        {"Yosemite",      "Y OW0 S EH1 M IH0 T IY0",    "yoh-SEM-i-tee"},
        {"Albuquerque",   "AE1 L B AH0 K ER0 K IY0",    "AL-buh-kur-kee"},
        {"Poughkeepsie",  "P AH0 K IH1 P S IY0",        "puh-KIP-see"},
        {"Schuylkill",    "S K Y UW1 L K IH2 L",        "SKOOL-kil"},
        {"Quincy",        "K W IH1 N S IY0",            "KWIN-see"},
        {"Cairo",         "K AY1 R OW0",                "KY-roh"},
        {"Lima",          "L AY1 M AH0",                "LY-muh"},
        {"Willamette",    "W AH0 L AE1 M AH0 T",        "wuh-LAM-uht"},
        {"Sioux",         "S UW1",                      "SOO"},
        {"Moines",        "M OY1 N Z",                  "MOYNZ"},
        {"Biloxi",        "B AH0 L AH1 K S IY0",        "buh-LUK-see"},
        {"Tempe",         "T EH1 M P IY0",              "TEM-pee"},
        {"Louisville",    "L UW1 IY0 V IH2 L",          "LOO-ee-vil"},
        {"Peabody",       "P IY1 B AA2 D IY0",          "PEE-bah-dee"},
        {"Billerica",     "B IH2 L ER0 IY1 K AH0",      "bil-uh-REE-kuh"},
        {"Chelmsford",    "CH EH1 L M S F ER0 D",       "CHELMS-furd"},
        {"Natchitoches",  "N AE0 K AH0 T AO1 SH",       "nak-uh-TOSH"},
        {"Nacogdoches",   "N AH0 K AA1 G D AH0 CH IH0 Z", "nuh-KOG-duh-chiz"},
        {"Beaufort",      "B OW1 F ER0 T",              "BOH-furt"},
        {"Gila",          "G IY1 L AH0",                "HEE-luh"},
        {"Refugio",       "R AH0 F Y UW1 JH IY0 OW0",   "ruh-FYOO-ree-oh"},
        {"Monticello",    "M AA2 N T AH0 S EH1 L OW0",  "mon-tuh-SEL-oh"},
        {"Lafayette",     "L AA2 F IY0 EH1 T",          "lah-fee-ET"},
        {"Delhi",         "D EH1 L IY0",                "DEL-ee"},
    };
    return table;
}

const PronunciationHint* pronunciation_hint(const std::string& w) {
    for (const PronunciationHint& h : pronunciation_hints()) {
        const char* a = h.word;
        size_t i = 0;
        bool same = true;
        for (; a[i] && i < w.size(); ++i) {
            char x = a[i], y = w[i];
            if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
            if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
            if (x != y) { same = false; break; }
        }
        if (same && !a[i] && i == w.size()) return &h;
    }
    return nullptr;
}

}  // namespace sayso
