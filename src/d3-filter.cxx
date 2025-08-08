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

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iterator>
#include <print>

#include <netinet/in.h>
#include <stdexcept>
#include <string_view>
#include <unistd.h>

#include <util/xerr.hxx>

using namespace std::literals;

struct config_bundle {
	[[nodiscard]] bool
	help() const noexcept { return _help; }

	[[nodiscard]] bool
	version() const noexcept { return _version; }

	void
	help(bool h) noexcept { _help = h; }

	void
	version(bool v) noexcept { _version = v; }
private:
	bool _help{false};
	bool _version{false};
};

config_bundle
read_config(int& argc, char**& argv) {
	config_bundle cfg;
	
	for (int opt = getopt(argc, argv, "hv"); 
			opt != -1;
			opt = getopt(argc, argv, "hv")) {
		switch (opt) {
		case 'v': cfg.version(true); break;
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
	int 
	print_version() {
		std::println("{}", "v0.1.0"); // XXX configure generate this
		return 2;
	}

	int
	print_usage(char* progname) {
		std::println("d3 utility: filter FQDN-IP address mappings\n");
		std::println("usage:");
		std::println("\t{} [-hv] exec...", progname);
		return 2;
	}

	class filterer {
		struct line_split {
			line_split(std::string_view line) {
				const auto separator0 = line.find('\t');
				if (separator0 == std::string_view::npos)
					throw std::runtime_error("invalid line: does not contain separator tab betwen domain and type");

				const auto separator1 = line.find('\t', separator0 + 1);
				if (separator1 == std::string_view::npos)
					throw std::runtime_error("invalid line: does not contain separator tab between type and IP");

				domain = line.substr(0, separator0);
				type = line.substr(separator0 + 1, separator1 - separator0 - 1);
				ip = line.substr(separator1 + 1);

				if (!(is_ipv4() || is_ipv6())) 
					throw std::runtime_error("invalid line: type must be either A or AAAA");
			}

			std::string_view domain{};
			std::string_view type{};
			std::string_view ip{};

			[[nodiscard]] bool
			is_ipv4() const noexcept { return type == "A"sv; }

			[[nodiscard]] bool
			is_ipv6() const noexcept { return type == "AAAA"sv; }
		};
	public:
		filterer(int out_fd) 
				: _out_fd(out_fd) { }

		void
		process_line(char* line_raw, size_t line_sz) {
			const auto line = load_raw_line(line_raw, line_sz);
			if (line.empty()) return;

			if (line.starts_with("A\t"))
				return reset_ipv4_filter(line.substr(1 + 1));
			if (line.starts_with("AAAA\t"))
				return reset_ipv6_filter(line.substr(4 + 2));

			line_split input(line);

			if (input.is_ipv6() && _current_ipv6.empty()) 
				throw std::runtime_error("invalid line: missing filter for AAAA rows before encountering AAAA entry");
			if (input.is_ipv4() && _current_ipv4.empty()) 
				throw std::runtime_error("invalid line: missing filter for A rows before encountering A entry");

			if (input.domain == _current_domain) 
				return current_domain_update(input);

			// new domain
			flush_current_domain();
			reset_domain(input.domain);
			current_domain_update(input);
		}

		void 
		flush() {
			flush_current_domain();
		}

	private:
		void
		reset_ipv4_filter(std::string_view ipv4) {
			if (ipv4.size() > std::size(_ipv4_filter))
				throw std::runtime_error(std::format(
							"invalid IPv4 format in filter, must be in dotted decimal: {}", 
							ipv4));

			flush_current_domain();

			std::memcpy(_ipv4_filter, ipv4.data(), ipv4.size());
			_current_ipv4 = std::string_view{_ipv4_filter, ipv4.size()};
		}

		void
		reset_ipv6_filter(std::string_view ipv6) {
			if (ipv6.size() > std::size(_ipv6_filter))
				throw std::runtime_error(std::format(
							"invalid IPv6 format in filter: {}", 
							ipv6));

			flush_current_domain();

			std::memcpy(_ipv6_filter, ipv6.data(), ipv6.size());
			_current_ipv6 = std::string_view{_ipv6_filter, ipv6.size()};
		}

		void
		reset_domain(std::string_view domain) {
			if (domain.size() > std::size(_domain))
				throw std::runtime_error(std::format("too large domain: this system can only handle domains up to {} bytes", std::size(_domain)));

			std::memcpy(_domain, domain.data(), domain.size());
			_current_domain = std::string_view(_domain, domain.size());
			_change_ipv4 = true;
			_change_ipv6 = true;
		}

		void
		flush_current_domain() {
			if (_current_domain.empty()) return;

			bool need_ln = false;
			if (_change_ipv4 && !_current_ipv4.empty()) {
				need_ln = true;
				write(_out_fd, _current_domain.data(), _current_domain.size());
				write(_out_fd, "\tA\t", 1 + 1 + 1);
				write(_out_fd, _current_ipv4.data(), _current_ipv4.size());
			}

			if (_change_ipv6 && !_current_ipv6.empty()) {
				need_ln = true;
				write(_out_fd, _current_domain.data(), _current_domain.size());
				write(_out_fd, "\tAAAA\t", 1 + 4 + 1);
				write(_out_fd, _current_ipv6.data(), _current_ipv6.size());
			}

			if (need_ln) write(_out_fd, "\n", 1);
		}

		void
		current_domain_update(const line_split& input) {
			if (input.is_ipv6()) {
				if (!_change_ipv6) return;
				_change_ipv6 = _current_ipv6 != input.ip;
			}
			if (input.is_ipv4()) {
				if (!_change_ipv4) return;
				_change_ipv4 = _current_ipv4 != input.ip;
			}
		}

		std::string_view
		load_raw_line(char* line_raw, std::size_t line_sz) {
			std::string_view line{line_raw, line_sz};

			// skip empty lines
			const auto non_ws_pos = line.find_first_not_of(" \t\f\v");
			if (non_ws_pos == std::string_view::npos) return {};
			line = line.substr(non_ws_pos);

			// skip comments
			if (line.starts_with("#")) return {};

			// normal line
			return line;
		}

		std::string_view _current_domain{};
		std::string_view _current_ipv4{};
		std::string_view _current_ipv6{};

		int _out_fd;
		char _domain[HOST_NAME_MAX]{};
		bool _change_ipv4{true};
		bool _change_ipv6{true};
		char _ipv4_filter[INET_ADDRSTRLEN]{};
		char _ipv6_filter[INET6_ADDRSTRLEN]{};
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

	int pipe_read = STDIN_FILENO;
	int pipe_write = STDOUT_FILENO;
	if (argc > 0) {
		int pfd[2];
		if (pipe(pfd) < 0) 
			return xerr(1, "pipe");
		pipe_read = pfd[0];
		pipe_write = pfd[1];
	}
	
	filterer filter(pipe_write);

	try {
		for (;;) {
			char line_buf[HOST_NAME_MAX + 1 + 4 + 1 + INET6_ADDRSTRLEN + 1]{};
			//            ^~~~~~~~~~~~~   ^   ^   ^   ^~~~~~~~~~~~~~~~   ^
			//            |               |   |   |   |                  NUL
			//            |               |   |   |   IPv6 is larger than
			//            |               |   |   |   IPv4 so if it fits we good
			//            |               |   |   tab
			//            |               |   AAAA, or just A, same logic, if
			//            |               |   IPv6 marker fits, so does IPv4
			//            |               tab
			//            Longest host name possible on this system. POSIX 
			//            specifies 255 but Linux is broken with 64.
			// All together this is the largest valid input line's length,
			// if input does not fit this, we abort further processing 
			// under the assumption of a malicious input.
			const auto res = std::fgets(line_buf, std::size(line_buf), stdin);
			if (!res) {
				// nullptr from fgets means either eof or error
				if (std::feof(stdin)) break; // feof means we are done
				return xerr(1, "fgets");
			}
			
			// the two edge cases of fgets for checking LN characters
			// to figure out the end:
			// 	1) input contains NUL: this is in general bad
			// 	2) last line does not contain LN

			// first we check strlen and search for LN
			const auto nul_pos = ::strnlen(line_buf, std::size(line_buf));
			const auto ln_ptr = std::memchr(line_buf, '\n', std::size(line_buf));

			if (ln_ptr) {
				const auto ln_pos = static_cast<char*>(ln_ptr) - line_buf;
				assert(std::cmp_not_equal(ln_pos, nul_pos) &&
						"NUL and LN have the same position");

				// in case of 1) NUL happened before LN, this should not happen
				// in any meaningful input so instead of extra trickery, we just
				// drop everything
				if (std::cmp_less(nul_pos, ln_pos)) // pos(NUL) < pos(LN)
					return xerr(2, "NUL byte in input");

				// at this point LN was found before NUL, so a full line 
				// was successfully extracted
				assert(std::cmp_equal(nul_pos - 1, ln_pos)
						&& "LN not directly before NUL (fgets)");

				filter.process_line(line_buf, ln_pos);
				continue;
			}

			// no LN in input could mean:
			// 	1) last line did not contain LN. This is alright, we need
			// 	   to process this too. In this case fgetc() will return EOF
			// 	   as reading more thing fill not be viable
			// 	2) line was too long, this means that fgetc() will still be
			// 	   able to read more input. This is bad, as we do not consider
			// 	   too large inputs as potentially viable, could be malicious.
			// 	   Therefore we just exit.
		
			int next_ch = std::fgetc(stdin);
			if (next_ch != EOF)
				return xerrx(2, "input line too large: a line can at most be {} characters long on this system:\ninvalid line was: {}\nnext char is: {} ({})", std::size(line_buf), line_buf, static_cast<char>(next_ch), next_ch);
			
			// last line, no more work :)
			filter.process_line(line_buf, nul_pos);
			break;
		}
	}
	catch (const std::exception& ex) {
		return xerrx(1, "{}", ex.what());
	}
	filter.flush(); // flush potiential last line change in IPs

	// no exec... passed, pipe_write was STDOUT, we are done
	if (argc == 0) return 2;

	// manipulate STDIN into our output
	if (dup2(pipe_read, STDIN_FILENO) < 0)
		return xerr(1, "dup2");
	if (close(pipe_write) < 0) 
		return xerr(1, "close");

	char* exe = argv[0];
	execvp(exe, argv);
	return xerr(1, "execvp");
}

