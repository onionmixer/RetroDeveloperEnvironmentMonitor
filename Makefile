# Makefile for rdemonitor
# Retro Developer Environment Monitor

CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -std=c11 -D_POSIX_C_SOURCE=200809L \
         -Wno-stringop-truncation -Wno-format-truncation
LDFLAGS = -lncurses -lcjson

# Debug build
CFLAGS_DEBUG = -Wall -Wextra -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DDEBUG \
               -Wno-stringop-truncation -Wno-format-truncation

TARGET = rdemonitor
SRCDIR = src
OBJDIR = obj

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Header files for dependency tracking
HEADERS = $(wildcard $(SRCDIR)/*.h)

.PHONY: all clean debug install uninstall test

# Default target
all: $(TARGET)

# Link the final executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(TARGET)
	@echo "Debug build complete"

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET)
	@echo "Clean complete"

# Install to /usr/local/bin
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/$(TARGET)"

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstalled $(TARGET)"

# Test Phase 1 modules (compile only, no UI)
test-phase1: $(OBJDIR)/config.o $(OBJDIR)/network.o $(OBJDIR)/parser.o $(OBJDIR)/logger.o
	@echo "Phase 1 modules compiled successfully"

# Create sample config file
sample-config:
	@echo "# rdemonitor configuration file" > rdemonitor.config.sample
	@echo "# " >> rdemonitor.config.sample
	@echo "# Debug server address" >> rdemonitor.config.sample
	@echo "debug_address=localhost" >> rdemonitor.config.sample
	@echo "" >> rdemonitor.config.sample
	@echo "# Debug server port" >> rdemonitor.config.sample
	@echo "debug_port=6502" >> rdemonitor.config.sample
	@echo "" >> rdemonitor.config.sample
	@echo "# Log all received data to file" >> rdemonitor.config.sample
	@echo "log_all=false" >> rdemonitor.config.sample
	@echo "Created rdemonitor.config.sample"

# Show help
help:
	@echo "rdemonitor Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build rdemonitor (default)"
	@echo "  debug         - Build with debug symbols"
	@echo "  clean         - Remove build artifacts"
	@echo "  install       - Install to /usr/local/bin"
	@echo "  uninstall     - Remove from /usr/local/bin"
	@echo "  test-phase1   - Compile Phase 1 modules only"
	@echo "  sample-config - Create sample config file"
	@echo "  help          - Show this help"
	@echo ""
	@echo "Dependencies:"
	@echo "  - libncurses-dev (apt install libncurses-dev)"
	@echo "  - libcjson-dev (apt install libcjson-dev)"
