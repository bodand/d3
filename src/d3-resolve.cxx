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
#include <exception>

#include <resolvers/ipify-com-resolver.hxx>
#include <util/xerr.hxx>
#include <util/config_bundle.hxx>
#include <util/polymorph.hxx>

using namespace std::literals;

namespace d3::resolve {
	struct config_bundle final : ::d3::config_bundle {
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

		[[nodiscard]] std::string_view
		backend() const noexcept { return _backend; }

	protected:
		config_bundle(const std::string_view progname,
							const std::string_view usage_msg)
			: d3::config_bundle("46b:hv"sv, progname, usage_msg) {}

		[[nodiscard]] bool
		handle_option(char opt, char *optarg) override {
			switch (opt) {
				case '4': ipv4(true);
					break;
				case '6': ipv6(true);
					break;
				case 'b': backend(optarg);
					break;
				default: return false;
			}
			return true;
		}

	private:
		friend d3::config_bundle;

		void
		backend(const char *be) noexcept { _backend = be; }

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

		std::string_view _backend = "ipify"sv;
		bool _ipv4{false}, _ipv6{false};
		bool _ip_default{true};
	};
}

namespace {
	struct resolver_map {
		std::string_view name;

		d3::polymorph<d3::resolver>
		(*builder)(bool ipv4, bool ipv6);
	};

	constexpr auto resolver_mapping = std::array{
		resolver_map{"ipify"sv, d3::ipify_com_resolver::build}
	};
}

using d3::xerr;
using d3::xerrx;

std::optional<int>
d3_main(int &argc, char **&argv) noexcept try {
	const auto cfg = d3::config_bundle::build<d3::resolve::config_bundle>(
		argc, argv, "resolve current public IP address");
	if (const int early = cfg.do_shortcircuit()) return early;

	const auto resolver_it = std::ranges::find_if(resolver_mapping,
																[&cfg](const auto &mapping) {
																	return cfg.backend() == mapping.name;
																});
	if (resolver_it == resolver_mapping.cend()) return xerrx(2, "invalid resolver backend: {}", cfg.backend());

	d3::polymorph<d3::resolver> resolver = resolver_it->builder(cfg.ipv4(), cfg.ipv6());
	resolver->resolve(cfg.output());

	return cfg.finalize();
}
catch (const std::exception &ex) {
	return xerrx(1, "{}", ex.what());
}
catch (...) {
	return xerrx(1, "unknown error: someone threw some garbage...");
}
