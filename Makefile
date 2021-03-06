#
# Compiler flags
#
CXX    = clang++
CXXFLAGS = -Wall -Wextra -std=c++14
#LDFLAGS = -pthread

#
# Project files
#
SRCS = tests.cpp
SRCDIR = src
OBJS = $(SRCS:.cpp=.o)
EXE  = xmltests

#
# Debug build settings
#
DBGDIR = bin/debug
DBGEXE = $(DBGDIR)/$(EXE)
DBGOBJS = $(addprefix $(DBGDIR)/, $(OBJS))
DBGCXXFLAGS = -g -O0 -DDEBUG

#
# Release build settings
#
RELDIR = bin/release
RELEXE = $(RELDIR)/$(EXE)
RELOBJS = $(addprefix $(RELDIR)/, $(OBJS))
RELCXXFLAGS = -O3 -DNDEBUG

.PHONY: all clean debug prep release remake

# Default build
all: prep release

#
# Debug rules
#
debug: $(DBGEXE)

$(DBGEXE): $(DBGOBJS)
	$(CXX) -o $(DBGEXE) $^ $(LDFLAGS)

$(DBGDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $(DBGCXXFLAGS) -o $@ $<

#
# Release rules
#

release: $(RELEXE)

$(RELEXE): $(RELOBJS)
	$(CXX) -o $(RELEXE) $^ $(LDFLAGS)

$(RELDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $(RELCXXFLAGS) -o $@ $<

#
# Other rules
#
prep:
	@mkdir -p $(DBGDIR) $(RELDIR)

remake: clean all

clean:
	rm -f $(RELEXE) $(RELOBJS) $(DBGEXE) $(DBGOBJS)
