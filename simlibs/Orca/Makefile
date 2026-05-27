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
TCPPACED_ROOT ?= $(OMNETPP_SAMPLES_ROOT)/tcpPaced
CUBIC_ROOT ?= $(OMNETPP_SAMPLES_ROOT)/cubic

all: checkmakefiles
	cd src && $(MAKE)

clean: checkmakefiles
	cd src && $(MAKE) clean

cleanall: checkmakefiles
	cd src && $(MAKE) MODE=release clean
	cd src && $(MAKE) MODE=debug clean
	rm -f src/Makefile

makefiles:
	cd src && opp_makemake -o Orca --make-so -f --deep --mode ${MODE} -I${RAYNET_HOME}/simlibs/RLComponents/src -L${RAYNET_HOME}/simlibs/RLComponents/src -lRLComponents${LIB_SUFFIX} -I${INET_ROOT}/src -L${INET_ROOT}/src -lINET${LIB_SUFFIX} -I${TCPPACED_ROOT}/src -L${TCPPACED_ROOT}/src -ltcpPaced${LIB_SUFFIX}  -I${CUBIC_ROOT}/src -L${CUBIC_ROOT}/src -lcubic${LIB_SUFFIX}  

checkmakefiles:
	@if [ ! -f src/Makefile ]; then \
	echo; \
	echo '======================================================================='; \
	echo 'src/Makefile does not exist. Please use "make makefiles" to generate it!'; \
	echo '======================================================================='; \
	echo; \
	exit 1; \
	fi
