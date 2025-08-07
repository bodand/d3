.POSIX:
# d3 project
#
# Copyright <YEAR> András Bodor <bodand@proton.me>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice,
#  this list of conditions and the following disclaimer in the documentation
#  and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

.PHONY: clean all build distclean configure compile_db
.SUFFIXES:
.SUFFIXES: .cxx .o .adoc

PROGRAMS = d3-resolve d3-query d3-filter
CC_FLAGS = -std=c++26 ${CFG_LIBCURL_CFLAGS}
LINK_FLAGS = ${CFG_LIBCURL_LIBS}

MANPAGE_SRC = docs/d3-filter.1.adoc \
				  docs/d3-query.1.adoc \
				  docs/d3-resolve.1.adoc
MANPAGE_OUT = ${MANPAGE_SRC:.adoc=}

EFFECTIVE_LINK_FLAGS = ${LINK_FLAGS} ${LDFLAGS}
EFFECTIVE_CC_FLAGS = ${CC_FLAGS} ${CXXFLAGS} ${CFLAGS}

all: build build-docs

include config.mk
config.mk: configure.pl
	@./configure.pl

build: ${PROGRAMS}

build-docs: ${MANPAGE_OUT}

RESOLVE_SRC = d3-resolve.cxx resolver.cxx ipify-com-resolver.cxx
RESOLVE_OBJ = ${RESOLVE_SRC:.cxx=.o}
d3-resolve: ${RESOLVE_OBJ}
	${CXX} -o $@ ${RESOLVE_OBJ} ${EFFECTIVE_LINK_FLAGS}

QUERY_SRC = d3-query.cxx
QUERY_OBJ = ${QUERY_SRC:.cxx=.o}
d3-query: ${QUERY_OBJ}
	${CXX} -o $@ ${QUERY_OBJ} ${EFFECTIVE_LINK_FLAGS}

FILTER_SRC = d3-filter.cxx
FILTER_OBJ = ${FILTER_SRC:.cxx=.o}
d3-filter: ${FILTER_OBJ}
	${CXX} -o $@ ${FILTER_OBJ} ${EFFECTIVE_LINK_FLAGS}

clean:
	-rm *.o *.bak
	-rm ${PROGRAMS}

distclean: clean
	-rm config.mk
	-rm depend.mk

compile_db: compile_commands.json

compile_commands.json: Makefile depend.mk config.mk
	bear -- ${MAKE}

include depend.mk
depend.mk: Makefile ${RESOLVE_SRC} ${QUERY_SRC} ${FILTER_SRC}
	@touch $@
	gccmakedep -f $@ -- ${EFFECTIVE_CC_FLAGS} -- ${RESOLVE_SRC}

.cxx.o:
	${CXX} -c ${EFFECTIVE_CC_FLAGS} -o $@ $<

.adoc:
	${ASCIIDOCTOR_EXE} -b manpage -o $@ $<

