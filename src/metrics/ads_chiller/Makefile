CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall

######## FILES

ads_chiller_BINS = \
    ads_chiller.o

######## RULES

all: $(ads_chiller_BINS) all.components

%.o: %.c %.h
	$(CC) $(CFLAGS) $(ads_chiller_DEFS) -c $<

######## SUBFOLDERS

%.components:
	@ $(MAKE) -C 'archs' $* || exit
	#@ $(MAKE) -C 'test'  $* || exit

######## OPTIONS

post:
	#@ $(MAKE) -C 'test' $@ || exit

install: install.components

%.install: ;

clean: rclean clean.components

depend: depend.components

depend-clean: depend-clean.components

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
