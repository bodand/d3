// Copyright 2025 András Bodor <bodand@proton.me>
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
// 	this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <cstring>
#include <print>
#include <format>
#include <utility>

#include <errno.h>


namespace d3 {
	inline const char*
	progname() {
#if defined(__APPLE__) || defined(__FreeBSD__)
		const char* const appname = getprogname();
#elif defined(_GNU_SOURCE)
		const char* const appname = program_invocation_name;
#else
		const char* const appname = "unknown";
#endif
		return appname;
	}

	template<class T, class... Args>
	decltype(auto)
	xerr(T&& ret, std::format_string<Args...> fmt, Args&&... args) {
		std::println(stderr,
				"{}: error: {}: {}",
				progname(),
				std::format(fmt, std::forward<Args>(args)...),
				strerror(errno));
		return std::forward<T>(ret);
	}

	template<class T, class... Args>
	decltype(auto)
	xerrx(T&& ret, std::format_string<Args...> fmt, Args&&... args) {
		std::println(stderr,
				"{}: error: {}",
				progname(),
				std::format(fmt, std::forward<Args>(args)...));
		return std::forward<T>(ret);
	}
}

