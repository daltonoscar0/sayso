CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2 -I.

SCALPEL_DIR = ../Scalpel
SCALPEL_SRC = $(SCALPEL_DIR)/tokenizer.cpp \
              $(SCALPEL_DIR)/char_classes.cpp

ENGINE_SRC  = wfst.cpp symbols.cpp
GRAMMAR_SRC = addresses.cpp names.cpp states.cpp phones.cpp proper.cpp \
              numbers.cpp money.cpp dates.cpp decades.cpp abbrev.cpp letters.cpp \
              ranges.cpp telephone.cpp decimals.cpp fractions.cpp times.cpp \
              measures.cpp
LIB_SRC     = $(ENGINE_SRC) $(GRAMMAR_SRC) text.cpp classify.cpp normalizer.cpp \
              report.cpp
HDRS        = $(wildcard *.h)

# ── Objects ──────────────────────────────────────────────────────────────────
#
# Every source is compiled once into build/ and linked into whatever needs it.
#
# It used to be that each binary named its own sources and the whole library was
# recompiled for each of them: seven test binaries times twenty translation
# units at -O2 is a little over half an hour, paid in full for a one-line change
# to one grammar. Nothing about that was load-bearing, and the object files were
# already gitignored. An incremental build is now a couple of seconds.
OBJDIR = build
vpath %.cpp . $(SCALPEL_DIR)
LIB_OBJ = $(addprefix $(OBJDIR)/,$(notdir $(LIB_SRC:.cpp=.o) $(SCALPEL_SRC:.cpp=.o)))

TESTS = wfst_tests number_tests money_date_tests quantity_tests clock_tests \
        lexical_tests pipeline_tests

.PHONY: all test eval clean

all: sayso $(TESTS)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Any header may be included by any source, so every object depends on all of
# them. Coarse, and still a great deal finer than recompiling per binary.
$(OBJDIR)/%.o: %.cpp $(HDRS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ── CLI (Scalpel front end + Sayso grammars) ─────────────────────────────────
sayso: $(OBJDIR)/sayso_main.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Engine tests (semiring, rational operations, composition, best path) ─────
# ── Cardinal and ordinal grammars, with round-trip properties ────────────────
# ── Currency and date grammars, with round-trip properties ───────────────────
# ── Decimal, fraction and measure grammars, with round-trip properties ───────
# ── Clock times, with round-trip properties ──────────────────────────────────
# ── Decades, abbreviations, acronyms, Roman numerals and ranges ──────────────
# ── Tokeniser bridge, CLASSIFY and end-to-end normalisation ──────────────────
#
# Each test binary links the whole library rather than the subset it uses. The
# subsets were a maintenance burden that bought nothing once the objects were
# shared: adding a grammar meant remembering which four of these lines had to
# learn about it, and forgetting was a link error at the end of a long build.
$(TESTS): %: $(OBJDIR)/%.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

# ── Evaluation harness (dev tooling; never linked into the library) ──────────
eval: $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o eval/generate_slice eval/generate_slice.cpp
	$(CXX) $(CXXFLAGS) -o eval/sayso_eval eval/eval_main.cpp $(LIB_OBJ)

clean:
	rm -rf $(OBJDIR)
	rm -f sayso $(TESTS) eval/generate_slice eval/sayso_eval
