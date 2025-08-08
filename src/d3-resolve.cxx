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

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <exception>
#include <print>
#include <iostream>
#include <ostream>

#include <unistd.h>

#include <resolvers/ipify-com-resolver.hxx>
#include <util/xerr.hxx>

struct config_bundle {
	[[nodiscard]] bool
	ipv4() const noexcept { 
		if (_ip_default) return true;
		return _ipv4;
	}

	[[nodiscard]] bool
	ipv6() const noexcept { 
		if (_ip_default) return false;
		return _ipv6;
	}

	void
	ipv4(const bool v4) noexcept {
		_ip_default = false;
		_ipv4 = v4;
	}

	void
	ipv6(const bool v6) noexcept {
		_ip_default = false;
		_ipv6 = v6;
	}

	[[nodiscard]] bool
	help() const noexcept { return _help; }

	[[nodiscard]] bool
	version() const noexcept { return _version; }

	void
	help(const bool h) noexcept { _help = h; }

	void
	version(const bool v) noexcept { _version = v; }

	[[nodiscard]] std::string_view
	backend() const noexcept { return _backend; }

	void
	backend(const char* be) noexcept { _backend = be; }
private:
	std::string_view _backend{"ipify"};
	bool _help{false};
	bool _version{false};
	bool _ipv4{false}, _ipv6{false};
	bool _ip_default{true};
};

config_bundle
read_config(int& argc, char**& argv) {
	config_bundle cfg;
	
	for (int opt = getopt(argc, argv, "46b:hv"); 
			opt != -1;
			opt = getopt(argc, argv, "46b:hv")) {
		switch (opt) {
		case '4': cfg.ipv4(true); break;
		case '6': cfg.ipv6(true); break;
		case 'v': cfg.version(true); break;
		case 'b': cfg.backend(optarg); break;
		case 'h':
		default:	
			cfg.help(true);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	return cfg;
}

int 
print_version() {
	std::cout << "v0.1.0"; // XXX configure generate this
	return 2;
}

int
print_usage(const char* const progname) {
	std::cout << "d3 utility: resolve current public IP address\n\n";
	std::cout << "usage:\n";
	std::cout << "\t" << progname << " [-46bhv] exec...\n";
	return 2;
}

namespace {
	struct resolver_map {
		std::string_view name;
		void (*maker)(std::span<std::byte> buf, bool ipv4, bool ipv6);
	};

	constexpr const auto resolver_mapping = std::array{
		resolver_map{"ipify", d3::ipify_com_resolver::maker}
	};
}

using d3::xerr;
using d3::xerrx;

int
main(int argc, char** argv) {
	// workaround for glibc getopt not being posix
	char posix_buf[] = "POSIXLY_CORRECT=1";
	putenv(posix_buf);

	const auto progname = argv[0];
	const config_bundle cfg = read_config(argc, argv);

	if (cfg.help()) return print_usage(progname);
	if (cfg.version()) return print_version();

	const auto resolver_it = std::find_if(resolver_mapping.cbegin(),
		resolver_mapping.cend(),
		[&cfg](const auto& mapping) {
			return cfg.backend() == mapping.name;
		});
	if (resolver_it == resolver_mapping.cend())
		return xerrx(2, "invalid resolver backend: {}", cfg.backend());

	char resolv_buffer[256]{0};

	// slightly hacky zero allocation use of polymorphic classes
	// buf provides storage for polymorphic resolver
	std::byte buf[1024];
	d3::resolver* resolver = nullptr;
	try {	
		resolver_it->maker(std::span(buf), cfg.ipv4(), cfg.ipv6());
		resolver = std::launder(std::bit_cast<d3::resolver*>(&(buf[0])));

		resolver->resolve(resolv_buffer);
		resolver->~resolver();
	}
	catch (const std::exception& ex) {
		if (resolver) resolver->~resolver();
		std::cerr << "error: " << ex.what() << "\n";
		return 1;
	}

	if (argc == 0) {
		std::cout << resolv_buffer;
		return 1;
	}

	int pfd[2];
	if (pipe(pfd) < 0) 
		return xerr(1, "pipe");
	const int pipe_read = pfd[0];
	const int pipe_write = pfd[1];

	if (dup2(pipe_read, STDIN_FILENO) < 0)
		return xerr(1, "dup2");

	if (write(pipe_write, resolv_buffer, std::strlen(resolv_buffer)) < 0)
		return xerr(1, "write");
	if (close(pipe_write) < 0) 
		return xerr(1, "close");

	const char* const exe = argv[0];
	execvp(exe, argv);
	return xerr(1, "execvp");
}
