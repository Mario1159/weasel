# Weasel Engine — Linux Artifact Makefile
# Usage:
#   sudo make install              # Install to /usr/local (default)
#   make install PREFIX=~/.local   # User-local install
#   make run                       # Run from the extracted archive

PREFIX    ?= /usr/local
BINDIR     = $(PREFIX)/bin
LIBDIR     = $(PREFIX)/lib
SHAREDIR   = $(PREFIX)/share/weasel
INCLUDEDIR = $(PREFIX)/include
CMAKE_DIR  = $(PREFIX)/lib/cmake/Weasel

BINARIES   = bin/weasel bin/weasel-cli bin/weasel-mcp-server
LIBRARIES  = lib/libwsl.so
# Tracy must be installed alongside libwsl.so: it's a SHARED library
# (libTracyClient.so) and both `weasel` and `libwsl.so` are dynamically
# linked against it. Without libTracyClient.so in libdir, the
# `Profiler::s_instance` assertion fires at startup (each binary would
# carry its own static copy).  Wildcard covers the SONAME symlink
# libTracyClient.so -> libTracyClient.so.<version>.
LIBRARIES += lib/libTracyClient.so*
SHAREDIRS  = share/weasel/compiled_shaders \
             share/weasel/icons \
             share/weasel/otf

.PHONY: all install uninstall run check

all:
	@echo "Targets:"
	@echo "  make install [PREFIX=/usr/local]  — install system-wide"
	@echo "  make uninstall [PREFIX=/usr/local] — remove installed files"
	@echo "  make run                          — run from this directory"

check:
	@test -f bin/weasel || { echo "Error: bin/weasel not found"; exit 1; }
	@test -f bin/weasel-cli || { echo "Error: bin/weasel-cli not found"; exit 1; }
	@test -f bin/weasel-mcp-server || { echo "Error: bin/weasel-mcp-server not found"; exit 1; }
	@test -f lib/libwsl.so || { echo "Error: lib/libwsl.so not found"; exit 1; }
	@test -f lib/libTracyClient.so || { echo "Error: lib/libTracyClient.so not found (Tracy must be built as a shared library)"; exit 1; }
	@test -d lib/cmake/Weasel || { echo "Error: lib/cmake/Weasel not found"; exit 1; }

install: check
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(SHAREDIR) \
		$(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(CMAKE_DIR)
	install -Dm755 bin/weasel $(DESTDIR)$(BINDIR)/weasel
	install -Dm755 bin/weasel-cli $(DESTDIR)$(BINDIR)/weasel-cli
	install -Dm755 bin/weasel-mcp-server $(DESTDIR)$(BINDIR)/weasel-mcp-server
	install -Dm644 lib/libwsl.so $(DESTDIR)$(LIBDIR)/libwsl.so
	# Install libTracyClient.so + SONAME symlink
	install -Dm644 lib/libTracyClient.so.* $(DESTDIR)$(LIBDIR)/
	cp -r share/weasel/* $(DESTDIR)$(SHAREDIR)/
	cp -r lib/cmake/Weasel/* $(DESTDIR)$(CMAKE_DIR)/
	cp -r include/* $(DESTDIR)$(INCLUDEDIR)/
	@echo "Installed to $(PREFIX)"
	@echo "Run: $(BINDIR)/weasel"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/weasel
	rm -f $(DESTDIR)$(BINDIR)/weasel-cli
	rm -f $(DESTDIR)$(BINDIR)/weasel-mcp-server
	rm -f $(DESTDIR)$(LIBDIR)/libwsl.so
	rm -f $(DESTDIR)$(LIBDIR)/libTracyClient.so*
	rm -rf $(DESTDIR)$(SHAREDIR)
	rm -rf $(DESTDIR)$(CMAKE_DIR)
	@echo "Uninstalled from $(PREFIX)"

run: check
	./bin/weasel
