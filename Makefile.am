SUBDIRS = src
EXTRA_DIST = TODO COPYING.LESSER m4/NOTES
ACLOCAL_AMFLAGS = -I m4 --install

dist_sysconf_DATA = asm.ini

# Create a binary distribution archive
dist-bin: all
	@mkdir -p /dev/shm/asm-root
	@$(MAKE) install DESTDIR=/dev/shm/asm-root
	@mkdir -p /dev/shm/$(distdir)-bin
	@install -m 644 /dev/shm/asm-root/$(sysconfdir)/asm.ini /dev/shm/$(distdir)-bin/
	@install -m 755 /dev/shm/asm-root/$(bindir)/asm /dev/shm/$(distdir)-bin/
	@install -m 755 /dev/shm/asm-root/$(libdir)/$(PACKAGE)/asmdll.so /dev/shm/$(distdir)-bin/
	tar -C /dev/shm -cvzf $(distdir)-bin.tgz $(distdir)-bin
	@rm -rf /dev/shm/$(distdir)-bin
	@$(MAKE) uninstall DESTDIR=/dev/shm/asm-root
	@rm -rf /dev/shm/asm-root
