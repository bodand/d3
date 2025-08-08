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

#include <optional>
#include <string_view>
#include <bits/local_lim.h>

#include <util/config_bundle.hxx>
#include <util/xerr.hxx>

using namespace std::literals;

using d3::xerr;
using d3::xerrx;

namespace d3::resolve {
	struct config_bundle final : ::d3::config_bundle {
		[[nodiscard]] std::string_view
		backend() const noexcept { return _backend; }

		[[nodiscard]] std::optional<std::string_view>
		zone() const noexcept { return _zone; }

	protected:
		config_bundle(const std::string_view progname,
							const std::string_view usage_msg)
			: d3::config_bundle("b:hvz:"sv, progname, usage_msg) {}

		[[nodiscard]] bool
		handle_option(char opt, char *optarg) override {
			switch (opt) {
				case 'b': backend(optarg);
					break;
				case 'z': zone(optarg);
					break;
				default: return false;
			}
			return true;
		}

		[[nodiscard]] std::optional<int>
		do_finalize() const override { return 0; }

	private:
		friend d3::config_bundle;

		void
		backend(const char *be) noexcept { _backend = be; }

		void
		zone(const char *z) noexcept { _zone = z; }

		std::string_view _backend = "dummy"sv;
		std::optional<std::string_view> _zone{};
		bool _ip_default{true};
	};
}

namespace {
	struct update_record {
	private:
		std::string_view _name;
	};
}

std::optional<int>
d3_main(int &argc, char **&argv) noexcept try {
	const auto cfg = d3::config_bundle::build<d3::resolve::config_bundle>(
		argc, argv, "resolve current public IP address");
	if (const int early = cfg.do_shortcircuit()) return early;
	if (argc != 0) return xerrx(2, "{} does not take arguments: {} passed", cfg.progname(), argc);

	constexpr auto domain_max = std::max(HOST_NAME_MAX, 255);
	char input_fmt[256];
	std::format_to_n(input_fmt, std::size(input_fmt),
							"%{}s%n %n%16s%n %n%65536s%n",
							domain_max);

	for (;;) {
		char hostname_buf[domain_max + 1]{}; // DNS record name
		char record_type_buf[16 + 1]{}; // DNS record type
		char data_buf[65536 + 1]{}; // value

		int ws_sz1{}, ws_sz2{};
		int hostname_sz{}, record_type_sz{}, data_sz{};
		if (const auto res = std::scanf(input_fmt,
														hostname_buf, &hostname_sz,
														&ws_sz1, record_type_buf, &record_type_sz,
														&ws_sz2, data_buf, &data_sz);
			res != 3) {
			if (res == EOF) break;
			return xerrx(1, "invalid line: domain line could not be parsed");
		}
		if (const auto next = getchar(); !(next == '\n' || next == EOF))
			return xerrx(1, "invalid line: line or segment too long");

		const std::string_view hostname(hostname_buf, hostname_sz);
		const std::string_view record_type(record_type_buf, record_type_sz - ws_sz1);
		const std::string_view data(data_buf, data_sz - ws_sz2);

		// todo dns.update(hostname, record_type, data);
	}

	return cfg.finalize();
}
catch (const std::exception &ex) {
	return xerrx(1, "{}", ex.what());
}
catch (...) {
	return xerrx(1, "unknown error: someone threw some garbage...");
}
