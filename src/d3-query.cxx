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

#include "util/config_bundle.hxx"

namespace d3::query {
	struct config_bundle final : d3::config_bundle {
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

		std::span<const std::string_view>
		domains() const noexcept {
			if (!_domains.empty()) return _domains;
			if (gethostname(_hostname_buf, HOST_NAME_MAX + 1) < 0) d3::xerr(1, "gethostname");
			_hostname = std::string_view(_hostname_buf);
			return {&_hostname, 1};
		}

	protected:
		config_bundle(const std::string_view &progname,
							const std::string_view &usage_msg)
			: d3::config_bundle("46d:hv", progname, usage_msg) {}

		[[nodiscard]] bool
		handle_option(char opt, char *optarg) override {
			switch (opt) {
				case '4': ipv4(true);
					break;
				case '6': ipv6(true);
					break;
				case 'd': add_domain(optarg);
					break;
				default: return false;
			}
			return true;
		}

	private:
		friend d3::config_bundle;

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

		void
		add_domain(char *domain) noexcept {
			_domains.emplace_back(domain);
		}

		mutable char _hostname_buf[HOST_NAME_MAX + 1]{};
		mutable std::string_view _hostname;
		std::vector<std::string_view> _domains;
		bool _ipv4{false}, _ipv6{false};
		bool _ip_default{true};
	};
}

using d3::xerr;
using d3::xerrx;

namespace {
	struct addrinfo_deleter {
		void
		operator()(struct addrinfo *addr) const noexcept {
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
		struct addrinfo *raw;

		std::vector<addr> ret;

		if (int errc = getaddrinfo(domain.data(), nullptr, &addr_filter, &raw);
			errc != 0) {
			ret.emplace_back(ip_af_to_type(ip_version), std::string(domain), std::nullopt);
			return d3::xerrx(std::move(ret), "getaddrinfo: {}", gai_strerror(errc));
		}
		ainfo.reset(raw);

		const auto canon = std::string(ainfo->ai_canonname);

		for (const auto *ptr = ainfo.get(); ptr; ptr = ptr->ai_next) {
			const auto *sock = ptr->ai_addr;
			char ip[sizeof("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF")];
			ip[0] = '?';
			ip[1] = '\0';
			switch (sock->sa_family) {
				case AF_INET: {
					const auto addr = std::bit_cast<struct sockaddr_in *>(sock)->sin_addr;
					inet_ntop(sock->sa_family, &addr, ip, std::size(ip));
					break;
				}
				case AF_INET6: {
					const auto addr = std::bit_cast<struct sockaddr_in6 *>(sock)->sin6_addr;
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
	make_nonblocking(const int fd) {
		const int old = fcntl(fd, F_GETFL, nullptr);
		if (old < 0) return xerr(1, "fcnt(, F_GETFL)");
		if (fcntl(fd, F_SETFL, old | O_NONBLOCK) < 0) //
			return xerr(1, "fcntl(, F_SETFL, +NONBLOCK)");
		return 0;
	}

	int
	pipeline_input(const int from, const int to) {
		char buf[8192];
		for (;;) {
			const auto read_cnt = ::read(from, buf, std::size(buf));
			if (read_cnt == 0) break; // EOF
			if (read_cnt == -1) {
				if (errno == EAGAIN) break; // no more input
				return xerr(1, "read");
			}

			if (write(to, buf, static_cast<size_t>(read_cnt)) != read_cnt) //
				return xerr(1, "write");
		}
		return 0;
	}
}

std::optional<int>
d3_main(int &argc, char **&argv) noexcept try {
	const auto cfg = d3::config_bundle::build<d3::query::config_bundle>(
		argc, argv, "resolve current public IP address");
	if (const int early = cfg.do_shortcircuit()) return early;

	const int output = cfg.output();

	if (const auto nonblock = make_nonblocking(STDIN_FILENO)) return nonblock;
	if (const auto copying = pipeline_input(STDIN_FILENO, output)) return copying;

	char buf[HOST_NAME_MAX + 1 + INET6_ADDRSTRLEN + 1];
	for (const auto &dom: cfg.domains()) {
		for (const auto addresses = load_address(dom, cfg.ipv4(), cfg.ipv6());
				const auto &[type, domain, ip]: addresses) {
			const auto [out, size] =
					std::format_to_n(buf, std::size(buf),
											"{}\t{}\t{}\n",
											domain,
											type,
											ip
												? *ip
												: "?");
			if (const auto formatted_sz = static_cast<std::size_t>(out - buf);
				std::cmp_not_equal(write(output, buf, formatted_sz), formatted_sz)) //
				return xerr(1, "write");
		}
	}

	return cfg.finalize();
}
catch (const std::exception &ex) {
	return xerrx(1, "{}", ex.what());
}
catch (...) {
	return xerrx(1, "unknown error: someone threw some garbage...");
}
