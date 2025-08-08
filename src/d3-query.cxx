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
#include <cerrno>
#include <climits>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <print>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <util/xerr.hxx>

struct config_bundle {
	bool
	ipv4() const noexcept {
		if (_ip_default) return true;
		return _ipv4;
	}

	bool
	ipv6() const noexcept {
		if (_ip_default) return false;
		return _ipv6;
	}

	void
	ipv4(bool v4) noexcept {
		_ip_default = false;
		_ipv4 = v4;
	}

	void
	ipv6(bool v6) noexcept {
		_ip_default = false;
		_ipv6 = v6;
	}

	bool
	help() const noexcept { return _help; }

	bool
	version() const noexcept { return _version; }

	void
	help(bool h) noexcept { _help = h; }

	void
	version(bool v) noexcept { _version = v; }

	std::span<const std::string_view>
	domains() const noexcept {
		if (!_domains.empty()) return std::span(_domains);
		if (gethostname(_hostname_buf, HOST_NAME_MAX + 1) < 0)
			d3::xerr(1, "gethostname");
		_hostname = std::string_view(_hostname_buf);
		return std::span<const std::string_view>(&_hostname, 1);
	}

	void
	add_domain(char* domain) noexcept {
		_domains.emplace_back(domain);
	}
private:
	mutable char _hostname_buf[HOST_NAME_MAX + 1];
	mutable std::string_view _hostname;
	std::vector<std::string_view> _domains;

	bool _help{false};
	bool _version{false};
	bool _ipv4{false}, _ipv6{false};
	bool _ip_default{true};
};

config_bundle
read_config(int& argc, char**& argv) {
	config_bundle cfg;

	for (int opt = getopt(argc, argv, "46d:hv");
			opt != -1;
			opt = getopt(argc, argv, "46d:hv")) {
		switch (opt) {
		case '4': cfg.ipv4(true); break;
		case '6': cfg.ipv6(true); break;
		case 'v': cfg.version(true); break;
		case 'd': cfg.add_domain(optarg); break;
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

namespace {
	struct addrinfo_deleter {
		void
		operator()(struct addrinfo* addr) const noexcept {
			freeaddrinfo(addr);
		}
	};

	struct addr {
		std::string_view type;
		std::string domain;
		std::optional<std::string> ip;
	};

	std::string_view
	ip_af_to_type(int af) {
		switch (af) {
		case AF_INET6:
			return "AAAA";
		default:
			return "A";
		}
	}

	std::vector<addr>
	load_address(std::string_view domain, bool ipv4, bool ipv6) {
		constexpr auto ai_flags = AI_IDN | AI_CANONIDN | AI_CANONNAME;
		const auto ip_version = ipv6 && ipv4
			? AF_UNSPEC
			: (ipv4 * AF_INET) | (ipv6 * AF_INET6);

		const auto addr_filter = (struct addrinfo){
			.ai_flags = ai_flags,
			.ai_family = ip_version,
			.ai_socktype = SOCK_STREAM,
			.ai_protocol = IPPROTO_TCP,
			.ai_addrlen = 0,
			.ai_addr = nullptr,
			.ai_canonname = nullptr,
			.ai_next = nullptr,
		};

		std::unique_ptr<struct addrinfo, addrinfo_deleter> ainfo;
		struct addrinfo* raw;

		std::vector<addr> ret;

		if (int errc = getaddrinfo(domain.data(), nullptr, &addr_filter, &raw);
				errc != 0) {
			ret.emplace_back(ip_af_to_type(ip_version), std::string(domain), std::nullopt);
			return d3::xerrx(std::move(ret), "getaddrinfo: {}", gai_strerror(errc));
		}
		ainfo.reset(raw);

		const auto canon = std::string(ainfo->ai_canonname);

		for (const auto* ptr = ainfo.get(); ptr; ptr = ptr->ai_next) {
			const auto* sock = ptr->ai_addr;
			char ip[sizeof("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF")];
			ip[0] = '?';
			ip[1] = '\0';
			switch (sock->sa_family) {
			case AF_INET: {
				const auto addr = std::bit_cast<struct sockaddr_in*>(sock)->sin_addr;
				inet_ntop(sock->sa_family, &addr, ip, std::size(ip));
				break;
			}
			case AF_INET6: {
				const auto addr = std::bit_cast<struct sockaddr_in6*>(sock)->sin6_addr;
				inet_ntop(sock->sa_family, &addr, ip, std::size(ip));
				break;
			}
			default:
				break;
			}
			ret.emplace_back(ip_af_to_type(sock->sa_family), canon, ip);
		}

		return ret;
	}

	int
	print_version() {
		std::cout << "v0.1.0"; // XXX configure generate this
		return 2;
	}

	int
	print_usage(char* progname) {
		std::cout << "d3 utility: resolve current public IP address\n\n";
		std::cout << "usage:\n";
		std::cout << "\t" << progname << " [-46dhv] exec...\n";
		return 2;
	}
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

	int pipe_read = STDIN_FILENO;
	int pipe_write = STDOUT_FILENO;
	if (argc > 0) {
		int pfd[2];
		if (pipe(pfd) < 0)
			return xerr(1, "pipe");
		pipe_read = pfd[0];
		pipe_write = pfd[1];
	}

	int old = fcntl(STDIN_FILENO, F_GETFL, nullptr);
	if (old < 0) return xerr(1, "fcnt(stdin, F_GETFL)");
	if (fcntl(STDIN_FILENO, F_SETFL, old | O_NONBLOCK) < 0)
		return xerr(1, "fcntl(stdin, F_SETFL, +NONBLOCK)");

	// shovel everything from stdin to pipe_write
	char buf[8192];
	for (;;) {
		int read_cnt = read(STDIN_FILENO, buf, std::size(buf));
		if (read_cnt == 0) break; // EOF
		if (read_cnt == -1) {
			if (errno == EAGAIN) break; // no more input
			return xerr(1, "read");
		}

		write(pipe_write, buf, read_cnt);
	}
	static_assert(std::size(buf) >= HOST_NAME_MAX + 1 + INET6_ADDRSTRLEN + 1,
			"buf of 8192 bytes is not enought for hostname + tab + ipv6 + newline"
			" on this system");

	for (const auto& dom : cfg.domains()) {
		const auto addresses = load_address(dom, cfg.ipv4(), cfg.ipv6());
		for (const auto& address : addresses) {
			const auto res =
				std::format_to_n(buf, std::size(buf),
					"{}\t{}\t{}\n",
					address.domain,
					address.type,
					address.ip
						? *address.ip
						: "?");
			const auto formatted_sz = static_cast<std::size_t>(res.out - buf);
			write(pipe_write, buf, formatted_sz);
		}
	}

	// no exec... passed, pipe_write was STDOUT, we are done
	if (argc == 0) return 1;

	// manipulate STDIN into our output
	if (dup2(pipe_read, STDIN_FILENO) < 0)
		return xerr(1, "dup2");
	if (close(pipe_write) < 0)
		return xerr(1, "close");

	char* exe = argv[0];
	execvp(exe, argv);
	return xerr(1, "execvp");
}

