MODE ?= release
ifeq ($(MODE),debug)
    LIB_SUFFIX = _dbg # Append _dbg to certain libraries if building in debug mode.
else
    LIB_SUFFIX = 
endif

RAYNET_HOME ?= $(abspath ../..)
OMNETPP_ROOT ?= $(HOME)/omnetpp
OMNETPP_SAMPLES_ROOT ?= $(OMNETPP_ROOT)/samples
INET_ROOT ?= $(lastword $(sort $(wildcard $(OMNETPP_SAMPLES_ROOT)/inet*)))

all: checkmakefiles
	cd src && $(MAKE)

clean: checkmakefiles
	cd src && $(MAKE) clean

cleanall: checkmakefiles
	cd src && $(MAKE) MODE=release clean
	cd src && $(MAKE) MODE=debug clean
	rm -f src/Makefile

makefiles:
	cd src && opp_makemake -o RLComponents --make-so -f --deep --mode ${MODE} -I${INET_ROOT}/src -L${INET_ROOT}/src -lINET${LIB_SUFFIX}


checkmakefiles:
	@if [ ! -f src/Makefile ]; then \
	echo; \
	echo '======================================================================='; \
	echo 'src/Makefile does not exist. Please use "make makefiles" to generate it!'; \
	echo '======================================================================='; \
	echo; \
	exit 1; \
	fi
