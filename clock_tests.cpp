// Clock time grammar tests, including a round-trip property over every
// minute of the day.
//
// Build:
//   g++ -std=c++17 -O2 -o clock_tests clock_tests.cpp times.cpp numbers.cpp \
//       money.cpp dates.cpp decimals.cpp fractions.cpp measures.cpp \
//       wfst.cpp symbols.cpp -I.
#include <iostream>
#include <string>
#include <vector>

#include "numbers.h"
#include "property.h"
#include "times.h"
#include "wfst.h"

using namespace sayso;

static int run = 0, failed = 0;

static void check(bool ok, const std::string& name) {
    ++run;
    if (!ok) { ++failed; std::cout << "[FAIL]" << name << "\n"; }
    else     {           std::cout << "[PASS]" << name << "\n"; }
}

static std::string tts(const std::string& written) {
    Path p = time_grammar().apply(chars_of(written));
    return p.found ? render(p.olabels) : "<none>";
}

static std::string itn(const std::string& spoken) {
    std::vector<std::string> words;
    std::string current;
    for (char c : spoken + " ") {
        if (c == ' ') { if (!current.empty()) words.push_back(current); current.clear(); }
        else current += c;
    }
    Path p = time_grammar().apply_inverse(words_of(words));
    return p.found ? render(p.olabels) : "<none>";
}

struct Case { const char* written; const char* spoken; };

static bool table(const std::vector<Case>& cases, bool both_ways) {
    bool ok = true;
    for (const Case& c : cases) {
        if (tts(c.written) != c.spoken) {
            ok = false;
            std::cout << "  tts " << c.written << " -> " << tts(c.written)
                      << " (want " << c.spoken << ")\n";
        }
        if (both_ways && itn(c.spoken) != c.written) {
            ok = false;
            std::cout << "  itn " << c.spoken << " -> " << itn(c.spoken)
                      << " (want " << c.written << ")\n";
        }
    }
    return ok;
}

static std::string two_digits(long long n) {
    return std::string(1, static_cast<char>('0' + n / 10)) +
           std::string(1, static_cast<char>('0' + n % 10));
}

// ---- the minute field -------------------------------------------------------

static void minute_tests() {
    check(table(
              {
                  {"3:30", "three thirty"},
                  {"10:15", "ten fifteen"},
                  {"1:45", "one forty-five"},
                  {"12:59", "twelve fifty-nine"},
              },
              true),
          "an ordinary minute is a two-digit cardinal");

    // A minute under ten is "oh five", not "five" and not "zero five".
    check(table(
              {
                  {"3:05", "three oh five"},
                  {"9:01", "nine oh one"},
                  {"11:09", "eleven oh nine"},
              },
              true),
          "a minute under ten takes 'oh'");

    check(table(
              {
                  {"3:00", "three o'clock"},
                  {"12:00", "twelve o'clock"},
                  {"1:00", "one o'clock"},
              },
              true),
          "a round hour standing alone is o'clock");
}

// ---- the hour field ---------------------------------------------------------

static void hour_tests() {
    check(table(
              {
                  {"19:30", "nineteen thirty"},
                  {"23:59", "twenty-three fifty-nine"},
                  {"0:30", "zero thirty"},
              },
              true),
          "twenty-four-hour times are read");

    // A round hour on a twenty-four-hour clock is "hundred", not "o'clock",
    // "18:00" is eighteen hundred. Each reading stays available to the other
    // clock at a cost, so neither is refused.
    check(table(
              {
                  {"18:00", "eighteen hundred"},
                  {"17:00", "seventeen hundred"},
                  {"22:00", "twenty-two hundred"},
              },
              true),
          "a round hour past twelve is 'hundred'");

    check(tts("10:00") == "ten o'clock" && tts("18:00") == "eighteen hundred" &&
              itn("ten hundred") == "10:00" && itn("eighteen o'clock") == "18:00",
          "each clock's reading is still available to the other");

    // Padding costs, so ITN writes the unpadded hour.
    check(tts("03:30") == "three thirty" && itn("three thirty") == "3:30",
          "a zero-padded hour is read, an unpadded one is written");

    check(tts("24:00") == "<none>" && tts("3:60") == "<none>" &&
              tts("3:5") == "<none>",
          "the hour and minute ranges are part of the grammar");
}

// ---- the meridiem -----------------------------------------------------------

static void meridiem_tests() {
    check(table(
              {
                  {"3:30 pm", "three thirty p m"},
                  {"11:45 am", "eleven forty-five a m"},
                  {"7:05 pm", "seven oh five p m"},
              },
              true),
          "the meridiem is spoken as two words");

    // Whether a round hour is said at all depends on the meridiem: "3:00" is
    // "three o'clock" but "3:00 pm" is "three p m". One weight, leaning
    // opposite ways in the two branches.
    check(tts("3:00") == "three o'clock" && tts("3:00 pm") == "three p m" &&
              itn("three p m") == "3:00 pm" && itn("three o'clock") == "3:00",
          "a round hour is o'clock alone and unsaid before a meridiem");

    check(itn("three o'clock p m") == "3:00 pm",
          "'o'clock' before a meridiem is read, though not written");

    // Variants Sayso accepts but will not produce.
    check(tts("3:30 p.m.") == "three thirty p m" &&
              tts("3:30pm") == "three thirty p m" &&
              itn("three thirty p m") == "3:30 pm",
          "dotted and unspaced meridiems are read, 'pm' with a space is written");

    // A bare hour with a meridiem and no minute field at all, how a time is
    // usually written. ITN puts the minutes back.
    check(table(
              {
                  {"10am", "ten a m"},
                  {"5pm", "five p m"},
              },
              false),
          "a bare hour takes a meridiem with no minutes");

    check(tts("6 p.m.") == "six p m" && itn("five p m") == "5:00 pm",
          "a bare hour is read, a time with its minutes is written");

    // The British full stop, and only before a meridiem: "10.30" on its own is
    // a decimal far more often than it is a time, and CLASSIFY sees no context.
    check(tts("10.30pm") == "ten thirty p m" && tts("10.30 am") == "ten thirty a m",
          "a full stop separates the fields when a meridiem follows");
}

// ---- properties -------------------------------------------------------------

static void property_tests() {
    std::string witness;

    // Every minute of the day, with and without a meridiem. The key is
    // hour*100 + minute, doubled to carry the meridiem flag.
    prop::Property<long long> clock_round_trip;
    clock_round_trip.generate = [](prop::Rng& r) {
        long long hour = r.in_range(0, kLastHour);
        long long minute = r.in_range(0, 59);
        return (hour * 100 + minute) * 2 + (r.in_range(0, 1) ? 1 : 0);
    };
    clock_round_trip.shrink = [](const long long& key) {
        std::vector<long long> out;
        long long clock = key / 2, meridiem = key % 2;
        long long hour = clock / 100, minute = clock % 100;
        for (long long smaller : prop::shrink_integer(minute))
            if (smaller >= 0) out.push_back((hour * 100 + smaller) * 2 + meridiem);
        for (long long smaller : prop::shrink_integer(hour))
            if (smaller >= 0) out.push_back((smaller * 100 + minute) * 2 + meridiem);
        if (meridiem) out.push_back(clock * 2);
        return out;
    };
    // A meridiem only makes sense on a twelve-hour clock, so it is dropped
    // above hour twelve rather than generating "19:30 pm".
    auto written_form = [](const long long& key) {
        long long clock = key / 2, hour = clock / 100, minute = clock % 100;
        std::string out = std::to_string(hour) + ":" + two_digits(minute);
        if (key % 2 && hour >= 1 && hour <= 12) out += " pm";
        return out;
    };
    clock_round_trip.show = written_form;
    clock_round_trip.holds = [written_form](const long long& key) {
        std::string written = written_form(key);
        std::string spoken = tts(written);
        return spoken != "<none>" && itn(spoken) == written;
    };
    check(prop::check_property(clock_round_trip, 20260808, 600, witness),
          "property: itn(tts(time)) == time over the whole day" +
              (witness.empty() ? "" : " [" + witness + "]"));
}

int main() {
    minute_tests();
    hour_tests();
    meridiem_tests();
    property_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
