TARGET = bin/streem

ifeq (Windows_NT,$(OS))
TARGET:=$(TARGET).exe
endif

CTAGS = /usr/local/bin/ctags
RM = rm -f

TESTS=$(wildcard examples/*.strm)

.PHONY : all format clean
all format clean:
	$(MAKE) -C src $@

.PHONY : test
test : all
	$(TARGET) -c $(TESTS)

.PHONY : ctags
ctags:
	$(RM) ./tags; \
	$(CTAGS) -R .; \
	$(CTAGS) -a -R /usr/include

.PHONY : global
global:
	gtags --gtagslabel=pygments

.PHONY : stool
stool: ctags global
