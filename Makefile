CXX = g++
CXXFLAGS = -O2 -std=c++17

code: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o code

.PHONY: clean
clean:
	rm -f code
