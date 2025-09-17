CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address -fsanitize=leak -pthread

SRC = main.cpp
BIN = app

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

run: $(BIN)
	./$(BIN) --producers 2 --consumers 2 --events 2000 --capacity 128

test: $(BIN)
	@echo "=== Test 1: Program runs ==="
	./$(BIN) --producers 2 --consumers 2 --events 1000 --capacity 128 > out.txt
	@if [ $$? -eq 0 ]; then echo "OK"; else echo "FAIL"; fi

	@echo "=== Test 2: Output contains 'sent=' ==="
	@grep -q "sent=" out.txt && echo "OK" || echo "FAIL"

clean:
	rm -f $(BIN) out.txt
