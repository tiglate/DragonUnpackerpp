# Makefile -- a thin proxy to make.sh, which holds the real logic (build, run, test, install...).
# Every target is forwarded as the action; extra arguments go in ARGS, for example:
#   make build ARGS=driver
#   make run ARGS="-- --open=/tmp/pak0.pak"
#   make install ARGS="-p /opt/dragonunpacker"
#   make list ARGS=~/games/Quake/id1/pak0.pak
# Run "make help" (or ./make.sh --help) for the list of actions.

SH   := ./make.sh
ARGS ?=

.DEFAULT_GOAL := all

.PHONY: all help configure build rebuild run bench test test-driver test-ui e2e sweep list extract \
        clean purge declutter install package

all:
	@$(SH) build

help:
	@$(SH) --help

configure build rebuild run bench test test-driver test-ui e2e sweep list extract \
clean purge declutter install package:
	@$(SH) $@ $(ARGS)

# any other action name that make.sh learns is forwarded too
%:
	@$(SH) $@ $(ARGS)
