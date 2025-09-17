CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -Werror -pedantic -fsanitize=address -fsanitize=leak
TARGET = logger
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

test1: run
	@echo "=== Test 1: Program runs ==="
	@echo "OK"

test2: run
	@echo "=== Test 2: Log file created and contains INFO ==="
	@test -f app.log || (echo "app.log missing"; exit 1)
	@grep -q "INFO: regular info" app.log && echo "OK" || (echo "log content invalid"; exit 1)

test: test1 test2

clean:
	rm -f $(TARGET) app.log
