CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC

######## FILES

two_phase_OBJS = \
    modbus.o \
    dummy.o

######## RULES

all: $(two_phase_OBJS)

modbus.o: modbus.c modbus.h
	$(CC) $(CFLAGS) $(MODBUS_CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

######## OPTIONS

install: ;

clean: rclean;

######## EXPORTS

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
