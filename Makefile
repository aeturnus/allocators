PROOT  = $(shell pwd)
SRCDIR = $(PROOT)/src
BINDIR = $(PROOT)/bin
OBJDIR = $(PROOT)/obj
LIBDIR = $(PROOT)/lib

CC = gcc
CXX = g++
LD = ld
AR = ar

CFLAGS = -Wall -Wextra -Wno-unused-parameter -g
LIBS   = -L$(LIBDIR)
INCLUDE = -I$(PROOT)/inc

SOURCES += $(shell find $(SRCDIR) -name '*.c')

OBJECTS  = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/$(MODULE)/%.o)

GTEST_DIR = test
GTEST_BIN = test_alloc
GTEST_SRC = $(GTEST_DIR)/*.cpp

$(OBJDIR)/$(MODULE)/%.o:$(SRCDIR)/%.cpp
	mkdir -p $(@D)	# generate the directory
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $(@)

$(OBJDIR)/$(MODULE)/%.o:$(SRCDIR)/%.c
	mkdir -p $(@D)	# generate the directory
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $(@)

$(BINDIR)/$(GTEST_BIN): $(OBJECTS) $(GTEST_SRC)
	mkdir -p $(BINDIR)
	$(CXX) $(CFLAGS) $(INCLUDE) -o $(BINDIR)/$(GTEST_BIN) $(OBJECTS) $(GTEST_SRC) $(LIBS) -lgtest 
	@echo "gtest suite built"

test: $(BINDIR)/$(GTEST_BIN)
	@$(BINDIR)/$(GTEST_BIN)

clean:
	rm $(BINDIR)/$(GTEST_BIN)
	rm -rf $(OBJDIR)/$(MODULE)

debug: $(BINDIR)/$(BIN)
	gdb $(BINDIR)/$(GTEST_BIN)

.PHONY: all clean test gtest mem debug
