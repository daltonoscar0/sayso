CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2 -I.

SCALPEL_DIR = ../Scalpel
SCALPEL_SRC = $(SCALPEL_DIR)/tokenizer.cpp \
              $(SCALPEL_DIR)/char_classes.cpp

ENGINE_SRC  = wfst.cpp symbols.cpp
GRAMMAR_SRC = numbers.cpp
LIB_SRC     = $(ENGINE_SRC) $(GRAMMAR_SRC) text.cpp classify.cpp normalizer.cpp

TESTS = wfst_tests number_tests

.PHONY: all test clean

all: sayso $(TESTS)

# ── CLI (Scalpel front end + Sayso grammars) ─────────────────────────────────
sayso: sayso_main.cpp $(LIB_SRC) $(SCALPEL_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Engine tests (semiring, rational operations, composition, best path) ─────
wfst_tests: wfst_tests.cpp $(ENGINE_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Cardinal and ordinal grammars, with round-trip properties ────────────────
number_tests: number_tests.cpp numbers.cpp $(ENGINE_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

clean:
	rm -f sayso $(TESTS)
