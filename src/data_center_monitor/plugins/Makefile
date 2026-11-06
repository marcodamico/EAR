CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -shared
# Cooling monitor build mode. SINGLE_NODE=1 (default) reads the IT power associated with the
# cooling devices from the local node (metrics API). Build with `make SINGLE_NODE=0` to take it
# from EARGM instead (rack/cluster deployments).
SINGLE_NODE ?= 1
ifeq ($(SINGLE_NODE),1)
CFLAGS += -DSINGLE_NODE
endif

######## RULES

plug_PATH = $(DESTDIR)/lib/plugins/monitoring
plug_PERM = 0775
plug_BINS = \
    conf.so \
    eardcon.so \
    eard_serialization.so \
    management.so \
    management_viewer.so \
    metrics.so \
    metrics_viewer.so \
    periodic_metrics.so \
    test_gpu.so \
    nodesensors.so \
    nodesensors_report.so \
    nodesensors_alerts.so \
    cooling_monitor.so \
    cooling_report.so

ifdef CUDA_BASE 
ifdef KERNELS
plug_BINS += kernel_cuda.so
endif
endif

ifdef OPENCL_BASE
plug_BINS += kernel_cl.so
endif

all: $(plug_BINS)

%.so: %.c
	$(CC) $(CFLAGS)  -o $@ $^

conf.so:conf.c
	$(CC) $(CFLAGS) -o $@ $^ $(call whole,$(SRCDIR)/common/libcommon.a)

eardcon.so: eardcon.c $(SRCDIR)/daemon/local_api/libeard.a
	$(CC) $(CFLAGS)  -o $@ $^

management.so: management.c
	$(CC)  $(CFLAGS)  -o $@ $^ $(call whole,$(SRCDIR)/management/libmgt.a)

metrics.so: metrics.c
	$(CC) $(CFLAGS)  -o $@ $^ $(call whole,$(SRCDIR)/metrics/libmetrics.a) $(SRCDIR)/daemon/local_api/libeard.a -lm

periodic_metrics.so: periodic_metrics.c
	$(CC) $(CFLAGS)  -o $@ $^ $(SRCDIR)/report/report.o

nodesensors.so: nodesensors.c
	$(CC) $(CFLAGS) -o $@ $^ $(SRCDIR)/common/libcommon.a $(SRCDIR)/metrics/libmetrics.a -lm

nodesensors_report.so: nodesensors_report.c
	$(CC) $(CFLAGS) -o $@ $^ $(SRCDIR)/common/libcommon.a $(SRCDIR)/report/report.o

nodesensors_alerts.so: nodesensors_alerts.c
	$(CC) $(CFLAGS) -o $@ $^ $(SRCDIR)/common/libcommon.a

# cooling_data_t (cooling_monitor.h) is shared by pointer between cooling_monitor.so (owner),
# cooling_report.so and report/cooling_csv.so, so they must be rebuilt when the header changes.
cooling_monitor.so: cooling_monitor.c cooling_monitor.h conf.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(call whole,$(SRCDIR)/metrics/libmetrics.a) $(SRCDIR)/daemon/local_api/libeard.a $(SRCDIR)/global_manager/eargm_rapi.o $(SRCDIR)/common/libcommon.a -lm

cooling_report.so: cooling_report.c cooling_monitor.h conf.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(SRCDIR)/common/libcommon.a $(SRCDIR)/report/report.o

######## OPTIONS

install: plug.ginstall;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
