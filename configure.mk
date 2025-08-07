.POSIX:
# Copyright 2025 András Bodor <bodand@proton.me>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
.SILENT:



# _check_library OUT NAME ENV PKGCONF?
_check_library:
	[ -n "${OUT}" ] || exit 1
	[ -n "${NAME}" ] || exit 1
	[ -n "${ENV}" ] || exit 1
	printf "finding CFLAGS for ${NAME}... "
	${MAKE} -f configure.mk _get_cflags "OUT=${OUT}" "NAME=${NAME}" "ENV=${ENV}" "PKGCONF=${PKGCONF}"
	:
	printf "finding LIBS for ${NAME}... "
	${MAKE} -f configure.mk _get_libs "OUT=${OUT}" "NAME=${NAME}" "ENV=${ENV}" "PKGCONF=${PKGCONF}"

# _get_cflags OUT NAME ENV PKGCONF?
.PHONY:
_get_cflags:
	if [ -n "$$${ENV}_CFLAGS" ]; \
	then \
		printf "CFG_%s_CFLAGS = %s\n" "${ENV}" "$$${ENV}_CFLAGS" >>${OUT}; \
		printf "ok(env)"; \
	elif [ -n "${PKGCONF}" ]; \
	then \
		printf "CFG_%s_CFLAGS = " "${ENV}" >>${OUT}; \
		if ${PKGCONF} --cflags ${NAME} 2>>pkgconf.log >>${OUT}; \
		then \
			printf "ok(pkgconf)\n"; \
		else \
			printf "\n" >>${OUT}; \
			printf "fail(pkgconf: $$?)\n"; \
		fi; \
	else \
		printf "CFG_%s_CFLAGS = \n" "${ENV}" >>${OUT}; \
		printf "fail\n"; \
	fi

# _get_libs OUT NAME ENV PKGCONF?
.PHONY:
_get_libs:
	if [ -n "$$${ENV}_LIBS" ]; \
	then \
		printf "CFG_%s_LIBS = %s\n" "${ENV}" "$$${ENV}_LIBS" >>${OUT}; \
		printf "ok(env)\n"; \
	elif [ -n "${PKGCONF}" ]; \
	then \
		printf "CFG_%s_LIBS = " "${ENV}" >>${OUT}; \
		if ${PKGCONF} --libs ${NAME} 2>>pkgconf.log >>${OUT}; \
		then \
			printf "ok(pkgconf)\n"; \
		else \
			printf "\n" >>${OUT}; \
			printf "fail(pkgconf: $$?)\n"; \
		fi; \
	else \
		printf "CFG_%s_LIBS = \n" "${ENV}" >>${OUT}; \
		printf "fail\n"; \
	fi


