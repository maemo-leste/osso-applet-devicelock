HILDON_CONTROL_PANEL_LIB_DIR=$(shell pkg-config hildon-control-panel --variable=pluginlibdir)
HILDON_CONTROL_PANEL_DATA_DIR=$(shell pkg-config hildon-control-panel --variable=plugindesktopentrydir)

SUBDIRS= src
BUILDDIR=build
CP_LIB=libcpdevicelock.so
DATA_FILE_CP=cpdevicelock.desktop

all: create_builddir subdirs

install: all
	install -d $(DESTDIR)/$(HILDON_CONTROL_PANEL_DATA_DIR)
	install -m 644 data/$(DATA_FILE_CP) $(DESTDIR)/$(HILDON_CONTROL_PANEL_DATA_DIR)
	install -d $(DESTDIR)/$(HILDON_CONTROL_PANEL_LIB_DIR)
	install -m 644 $(BUILDDIR)/$(CP_LIB) $(DESTDIR)/$(HILDON_CONTROL_PANEL_LIB_DIR)
	install -d $(DESTDIR)/$(CONTROL_PANEL_IMAGE_PATH)
subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

create_builddir:
	mkdir -p build

.PHONY: all clean install $(SUBDIRS)

clean:
	rm -rf build
	for d in $(SUBDIRS); do (cd $$d; $(MAKE) clean); done
