CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CORE := core/calibration.cpp core/device.cpp core/ledger.cpp core/protocol.cpp core/sha256.cpp
TESTS := tests/test_calibration.cpp tests/test_crypto.cpp tests/test_device.cpp tests/test_main.cpp

.PHONY: test clean

test: scentstation_tests
	./scentstation_tests

scentstation_tests: $(CORE) $(TESTS)
	$(CXX) $(CXXFLAGS) -Icore -Itests $(CORE) $(TESTS) -o $@

clean:
	rm -f scentstation_tests
