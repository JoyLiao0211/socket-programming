# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -g

# Targets
TARGETS = server client

# Build all targets
all:
	@for target in $(TARGETS); do \
		$(CXX) $(CXXFLAGS) -o $$target $$target.cpp Parse.cpp; \
	done

# Clean up generated files
clean:
	rm -f $(TARGETS) *.o
