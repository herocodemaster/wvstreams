WVSTREAMS=.
WVSTREAMS_SRC= # Clear WVSTREAMS_SRC so wvrules.mk uses its WVSTREAMS_foo
include wvrules.mk
override enable_efence=no

XPATH=include

include vars.mk

ifeq ("$(build_xplc)", "yes")
  MYXPLC:=xplc
endif

all: config.mk $(MYXPLC) $(TARGETS)

ifeq ("$(build_xplc)", "yes")

.PHONY: xplc
xplc:
	$(MAKE) -C xplc

# Prevent complaints that Make can't find these two linker options.
-lxplc-cxx: ;

-lxplc: ;

endif

%.so: SONAME=$@.$(RELEASE)

.PHONY: clean depend dust kdoc doxygen install install-shared install-dev install-xplc uninstall tests dishes dist distclean realclean test

# FIXME: little trick to ensure that the wvautoconf.h.in file is there
.PHONY: dist-hack-clean
dist-hack-clean:
	rm -f stamp-h.in

dist-hook: dist-hack-clean configure
	rm -rf autom4te.cache
	if test -d .xplc; then \
	    $(MAKE) -C .xplc clean patch; \
	    cp -Lpr .xplc/build/xplc .; \
	fi

runconfigure: config.mk include/wvautoconf.h

config.mk: configure config.mk.in include/wvautoconf.h.in
	$(error Please run the "configure" script)

# FIXME: there is some confusion here
ifdef WE_ARE_DIST
configure: configure.ac config.mk.in include/wvautoconf.h.in
	$(warning "$@" is old, please run "autoconf")

include/wvautoconf.h.in: configure.ac
	$(warning "$@" is old, please run "autoheader")
else
configure: configure.ac
	autoheader
	autoconf

include/wvautoconf.h.in:
	autoheader
endif

define wild_clean
	@list=`echo $(wildcard $(1))`; \
		test -z "$${list}" || sh -cx "rm -rf $${list}"
endef

realclean: distclean
	$(call wild_clean,$(REALCLEAN))


distclean: clean
	$(call wild_clean,$(DISTCLEAN))
	@rm -f .xplc

clean: depend dust
	$(call wild_clean,$(TARGETS) uniconf/daemon/uniconfd \
		$(GARBAGE) $(TESTS) tmp.ini \
		$(shell find . -name '*.o' -o -name '*.moc'))

depend:
	$(call wild_clean,$(shell find . -name '.*.d'))

dust:
	$(call wild_clean,$(shell find . -name 'core' -o -name '*~' -o -name '.#*') $(wildcard *.d))

kdoc:
	kdoc -f html -d Docs/kdoc-html --name wvstreams --strip-h-path */*.h

doxygen:
	doxygen

install: install-shared install-dev install-xplc install-uniconfd

install-shared: $(TARGETS_SO)
	$(INSTALL) -d $(DESTDIR)$(libdir)
	for i in $(TARGETS_SO); do \
	    $(INSTALL_PROGRAM) $$i.$(RELEASE) $(DESTDIR)$(libdir)/ ; \
	done

install-dev: $(TARGETS_SO) $(TARGETS_A)
	$(INSTALL) -d $(DESTDIR)$(includedir)/wvstreams
	$(INSTALL_DATA) $(wildcard include/*.h) $(DESTDIR)$(includedir)/wvstreams
	$(INSTALL) -d $(DESTDIR)$(libdir)
	for i in $(TARGETS_A); do \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(libdir); \
	done
	for i in $(TARGETS_SO); do \
	    cd $(DESTDIR)$(libdir) && $(LN_S) $$i.$(RELEASE) $$i; \
	done

ifeq ("$(build_xplc)", "yes")

install-xplc: xplc
	$(MAKE) -C xplc install

else

install-xplc: ;

endif

uniconfd: uniconf/daemon/uniconfd uniconf/daemon/uniconfd.ini \
          uniconf/daemon/uniconfd.8

install-uniconfd: uniconfd
	$(INSTALL) -d $(DESTDIR)$(sbindir)
	$(INSTALL_PROGRAM) uniconf/daemon/uniconfd $(DESTDIR)$(sbindir)/
	$(INSTALL) -d $(DESTDIR)$(sysconfdir)
	$(INSTALL_DATA) uniconf/daemon/uniconf.conf $(DESTDIR)$(sysconfdir)/
	$(INSTALL) -d $(DESTDIR)$(localstatedir)/lib/uniconf
	touch $(DESTDIR)$(localstatedir)/lib/uniconf/uniconfd.ini
	$(INSTALL) -d $(DESTDIR)$(mandir)
	$(INSTALL_DATA) uniconf/daemon/uniconfd.8 $(DESTDIR)$(mandir)/

uninstall:
	$(tbd)

$(TESTS): $(LIBUNICONF)
$(addsuffix .o,$(TESTS)):
tests: $(TESTS)

include $(filter-out xplc%,$(wildcard */rules.mk */*/rules.mk)) /dev/null

-include $(shell find . -name '.*.d') /dev/null

test: runconfigure all tests wvtestmain
	$(WVTESTRUN) $(MAKE) runtests

runtests:
	$(VALGRIND) ./wvtestmain $(TESTNAME)
ifeq ("$(TESTNAME)", "unitest")
	cd uniconf/tests && DAEMON=0 ./unitest.sh
	cd uniconf/tests && DAEMON=1 ./unitest.sh
endif

wvtestmain: wvtestmain.o \
	$(call objects, $(shell find . -type d -name t)) \
	$(LIBUNICONF)

