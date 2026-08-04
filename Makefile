CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2 -I.

SCALPEL_DIR = ../Scalpel
SCALPEL_SRC = $(SCALPEL_DIR)/tokenizer.cpp \
              $(SCALPEL_DIR)/char_classes.cpp

ENGINE_SRC  = wfst.cpp symbols.cpp
GRAMMAR_SRC = numbers.cpp money.cpp dates.cpp
LIB_SRC     = $(ENGINE_SRC) $(GRAMMAR_SRC) text.cpp classify.cpp normalizer.cpp
HDRS        = $(wildcard *.h)

TESTS = wfst_tests number_tests money_date_tests pipeline_tests

.PHONY: all test clean

all: sayso $(TESTS)

# ── CLI (Scalpel front end + Sayso grammars) ─────────────────────────────────
sayso: sayso_main.cpp $(LIB_SRC) $(SCALPEL_SRC) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cpp,$^)

# ── Engine tests (semiring, rational operations, composition, best path) ─────
wfst_tests: wfst_tests.cpp $(ENGINE_SRC) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cpp,$^)

# ── Cardinal and ordinal grammars, with round-trip properties ────────────────
number_tests: number_tests.cpp numbers.cpp $(ENGINE_SRC) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cpp,$^)

# ── Currency and date grammars, with round-trip properties ───────────────────
money_date_tests: money_date_tests.cpp $(GRAMMAR_SRC) $(ENGINE_SRC) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cpp,$^)

# ── Tokeniser bridge, CLASSIFY and end-to-end normalisation ──────────────────
pipeline_tests: pipeline_tests.cpp $(LIB_SRC) $(SCALPEL_SRC) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cpp,$^)

test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

clean:
	rm -f sayso $(TESTS)
