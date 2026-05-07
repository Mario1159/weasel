# Weasel Engine — Linux Artifact Makefile
# Usage:
#   sudo make install              # Install to /usr/local (default)
#   make install PREFIX=~/.local   # User-local install
#   make run                       # Run from the extracted archive

PREFIX   ?= /usr/local
BINDIR    = $(PREFIX)/bin
LIBDIR    = $(PREFIX)/lib
SHAREDIR  = $(PREFIX)/share/weasel

BINARIES  = bin/weasel bin/weasel-cli
LIBRARIES = lib/libwsl.so
SHAREDIRS = share/weasel/compiled_shaders \
            share/weasel/icons \
            share/weasel/otf \
            share/weasel/rml

.PHONY: all install uninstall run check

all:
	@echo "Targets:"
	@echo "  make install [PREFIX=/usr/local]  — install system-wide"
	@echo "  make uninstall [PREFIX=/usr/local] — remove installed files"
	@echo "  make run                          — run from this directory"

check:
	@test -f bin/weasel || { echo "Error: bin/weasel not found"; exit 1; }
	@test -f bin/weasel-cli || { echo "Error: bin/weasel-cli not found"; exit 1; }
	@test -f lib/libwsl.so || { echo "Error: lib/libwsl.so not found"; exit 1; }

install: check
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(SHAREDIR)
	install -Dm755 bin/weasel $(DESTDIR)$(BINDIR)/weasel
	install -Dm755 bin/weasel-cli $(DESTDIR)$(BINDIR)/weasel-cli
	install -Dm644 lib/libwsl.so $(DESTDIR)$(LIBDIR)/libwsl.so
	cp -r share/weasel/* $(DESTDIR)$(SHAREDIR)/
	@echo "Installed to $(PREFIX)"
	@echo "Run: $(BINDIR)/weasel"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/weasel
	rm -f $(DESTDIR)$(BINDIR)/weasel-cli
	rm -f $(DESTDIR)$(LIBDIR)/libwsl.so
	rm -rf $(DESTDIR)$(SHAREDIR)
	@echo "Uninstalled from $(PREFIX)"

run: check
	./bin/weasel
