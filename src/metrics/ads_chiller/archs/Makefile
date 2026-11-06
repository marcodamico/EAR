CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC

######## FILES

ads_chiller_OBJS = \
    modbus.o \
    dummy.o

######## RULES

all: $(ads_chiller_OBJS)

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
