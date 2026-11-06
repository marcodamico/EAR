CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall

######## FILES

two_phase_BINS = \
    two_phase.o

######## RULES

all: $(two_phase_BINS) all.components

%.o: %.c %.h
	$(CC) $(CFLAGS) $(two_phase_DEFS) -c $<

######## SUBFOLDERS

%.components:
	@ $(MAKE) -C 'archs' $* || exit
	#@ $(MAKE) -C 'test'  $* || exit

######## OPTIONS

post:
	#@ $(MAKE) -C 'test' $@ || exit
	# $(CC) $(CFLAGS) -DTEST_EXAMPLE=1 -o gpuprof_test gpuprof.c ../libmetrics.a ../../common/libcommon.a -lpthread -lm -ldl

install: install.components

%.install: ;

clean: rclean clean.components

depend: depend.components

depend-clean: depend-clean.components

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
