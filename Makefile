ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc#build

include $(RTE_SDK)/mk/rte.vars.mk

LDLIBS = -lpcap -lpthread -lm 
#LDFLAGS = -Wl,--rpath -Wl,/usr/local/lib

# binary name
APP = dpdk

# all source are stored in SRCS-y
SRCS-y := DPDK.c
#SRCS-y := src/main_copy.c

CFLAGS += -static-libgcc -fPIC #-static-libstdc++ 
CFLAGS += $(WERROR_FLAGS)

include $(RTE_SDK)/mk/rte.extapp.mk

gcc -shared -static -o libDPDK.so DPDK.o -lintel_dpdk