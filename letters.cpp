#include "letters.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

// Acronyms spelled out, as a table drawn from the corpus rather than as a rule
// over capitals.
//
// A general "two or more capitals" rule is 70% right on this shard: 85,781
// tokens are spelled out and 37,015 are not, and the second group is Roman
// numerals, acronyms English pronounces as words (FIFA, RAF, NASA) and
// caps-lock prose (THE, OF). Seventy percent would make it by a wide margin
// the least reliable thing Sayso does. These 396 entries are every all-caps
// type among the corpus's 400 commonest that the reference spells out at least
// 95% of the time; they cover 51,756 tokens, and the four rejected are led by
// "IV", which is a Roman numeral as often as it is an abbreviation.
//
// This is the one branch that needs the span unfolded: "US" is an acronym and
// "us" is a pronoun. See Grammar::wants_case.
const char* kAcronyms[] = {
    "PDF", "ISBN", "US", "TV", "UK", "BBC", "USA", "CD", "FC", "DVD", "IUCN",
    "FM", "LLC", "ABC", "NFL", "DC", "EP", "DNA", "NY", "MP", "CEO", "TERYT",
    "CBS", "CDP", "NBA", "CA", "DJ", "PA", "PC", "MA", "NBC", "MD", "AFL",
    "UFC", "ESPN", "NSW", "MTV", "LP", "LA", "SS", "UN", "WWE", "NJ", "EU",
    "NHL", "BC", "SC", "HMS", "AP", "AC", "FA", "PS", "RNA", "CIA", "PM",
    "HD", "IATA", "DS", "CBC", "NC", "OS", "IBM", "CNN", "AD", "GM", "IT",
    "GP", "MS", "FBI", "MLB", "AA", "CT", "OCLC", "USSR", "JP", "AFC", "JM",
    "FCC", "UCLA", "IGN", "BA", "ITV", "TNA", "PGA", "NZ", "ATP", "MC",
    "HIV", "CO", "GT", "PBS", "SP", "ID", "PR", "TD", "NYC", "SF", "JL",
    "GOP", "RPM", "MVP", "USC", "BMW", "IBGE", "SJ", "MJ", "LGBT", "RL",
    "AJ", "SK", "CE", "AAA", "JC", "URL", "ISSN", "UC", "NRHP", "IFPI",
    "GDP", "AYP", "VFL", "JD", "SH", "RTE", "HP", "CM", "UCI", "JS", "IP",
    "IAAF", "ICC", "CH", "FL", "FK", "USAF", "HM", "SD", "FDA", "EC", "HBO",
    "NHS", "HTML", "HC", "JJ", "CC", "VHS", "CP", "CB", "RC", "MG", "USGS",
    "NH", "FAA", "AFB", "MN", "RFC", "CS", "NRL", "ACM", "RJ", "TX", "BP",
    "CRC", "PJ", "BT", "USDA", "RCA", "GAA", "SL", "LH", "DB", "XML", "UAE",
    "BMC", "NWA", "MB", "IPC", "NIH", "TSR", "RS", "CR", "DL", "AI", "LDS",
    "CSI", "RIAA", "WR", "DM", "MGM", "ML", "HQ", "RB", "VA", "MBE", "JG",
    "PPV", "FIA", "OR", "JF", "API", "NME", "ICF", "CW", "NT", "MF", "ACC",
    "VC", "TH", "NDP", "AVN", "BK", "GMA", "MLS", "MMA", "WWF", "ODI", "MLA",
    "PMID", "CJ", "PT", "CN", "WNBA", "RW", "VH", "LM", "HS", "RR", "CPU",
    "NGC", "EPA", "JH", "CL", "LD", "JK", "WW", "SI", "MM", "LG", "DW",
    "NPR", "KC", "CF", "QB", "ICAO", "CMT", "NS", "HR", "MV", "SB", "NL",
    "DP", "FIVB", "RBI", "SV", "IR", "SG", "USD", "PSSA", "RF", "CV", "WB",
    "AK", "HT", "AFP", "GPS", "GB", "GR", "CBE", "BB", "CFL", "ITF", "BMX",
    "SW", "JW", "HK", "GS", "MH", "TM", "FY", "NGO", "CG", "INE", "IAU",
    "XM", "AMC", "RAAF", "AOL", "WTA", "IDF", "TL", "TF", "JB", "MCA", "BS",
    "AV", "JPL", "LJ", "LC", "IOC", "RP", "KM", "NSA", "USL", "RPG", "NYU",
    "MTA", "LS", "AF", "NK", "KR", "XLS", "IIHF", "NXT", "BCE", "MGC", "WCW",
    "SQL", "XP", "RHS", "FX", "NOAA", "PSP", "BYU", "QS", "CTV", "ISU",
    "FDP", "PH", "UV", "MRI", "VIP", "BM", "UHF", "TB", "CDC", "LSU", "PBA",
    "CDBS", "TJ", "CMLL", "TT", "VP", "BMG", "TNT", "NIOSH", "CPI", "NCIS",
    "CBN", "USB", "EJ", "KK", "DH", "IK", "RN", "GE", "MK", "TN", "PF", "KY",
    "RT", "VM", "SBS", "ECW", "GC", "PP", "DT", "SMS", "SSR", "UBS", "KL",
    "CNS", "MCC", "IMDB", "NMR", "RG", "GJ", "MSNBC", "DK", "NM", "JSTOR",
    "MX", "DD", "HL", "WJ", "SNL", "PG", "GSM", "BJP", "TKO", "HTTP", "UNC",
    "HJ", "NAACP", "IQ", "HMAS", "WM",
};

Wfst acronym_table() {
    std::vector<Wfst> parts;
    for (const char* a : kAcronyms) {
        std::vector<std::string> letters;
        for (const char* c = a; *c; ++c)
            letters.push_back(std::string(1, static_cast<char>(*c - 'A' + 'a')));
        parts.push_back(rule(a, letters));
    }
    return union_of(parts);
}

Wfst build_letters() {
    // A letter carrying its full stop. Every path must contain at least one of
    // these, which is what keeps a bare "a" or "I" out of the grammar.
    // Both cases, because this grammar is handed the span unfolded — see the
    // note on the acronym table. The spoken side is lowercase either way.
    std::vector<Wfst> dotted, bare;
    for (char c = 'a'; c <= 'z'; ++c) {
        std::string lower(1, c);
        std::string upper(1, static_cast<char>(c - 'a' + 'A'));
        for (const std::string& written : {lower, upper}) {
            dotted.push_back(rule(written + ".", {lower}));
            bare.push_back(rule(written, {lower}));
        }
    }
    Wfst with_stop = union_of(dotted);

    // Two letters minimum, and it is the sentence-final full stop that forces
    // it. Scalpel gives "the 1980s." the tokens [1980] [s] [.], and a
    // one-letter rule reads the last two of those as a dotted initial — which
    // both spells out the plural of a decade and eats the sentence's full
    // stop, since a claimed span is replaced whole. "vitamin C." loses its
    // stop the same way. Requiring two letters costs the bare initial of
    // "J. Smith" and keeps every acronym.
    //
    // "U.S." and "e.g." are the ordinary shape; "U.S" merely drops the last
    // stop, so the final letter may go bare — but only after one that did not.
    return connect(union_of({
        concat_of({with_stop, with_stop, closure(with_stop)}),
        concat_of({with_stop, closure(with_stop),
                   reweight(union_of(bare), kUndottedFinalLetterPenalty)}),
        acronym_table(),
    }));
}

}  // namespace

const Cascade& letters_grammar() {
    static const Wfst core = build_letters();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
