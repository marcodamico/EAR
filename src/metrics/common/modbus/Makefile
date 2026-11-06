CFLAGS = $(CC_FLAGS) -fPIC -I$(SRCDIR) $(CONSTANTS)

######## FILES

modbus_OBJS = \
    emodbus.o \
    ads_chiller_modbus.o \
    two_phase_modbus.o

######## RULES

all: $(modbus_OBJS)

# libmodbus is dlopen'ed at runtime (see emodbus.c), only its headers are needed here.
%.o: %.c %.h
	$(CC) $(CFLAGS) $(MODBUS_CFLAGS) -c $<

######## OPTIONS

install: ;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
