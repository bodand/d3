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

#ifndef D3_CONFIG_BUNDLE_HXX
#define D3_CONFIG_BUNDLE_HXX

#include <string_view>

namespace d3 {
	struct config_bundle {
		explicit
		config_bundle(const std::string_view &getopts_arg,
							const std::string_view progname,
							const std::string_view usage_msg)
			: _getopts_arg(getopts_arg)
			, _progname(progname)
			, _usage_msg(usage_msg) {}

		virtual ~config_bundle() noexcept = default;

		[[nodiscard]] bool
		help() const noexcept { return _help; }

		[[nodiscard]] bool
		version() const noexcept { return _version; }

		void
		read_config(int &argc, char **&argv);

		[[nodiscard]] int
		do_shortcircuit() const;

		[[nodiscard]] int
		output() const { return _output; }

		[[nodiscard]] int
		finalize() const;

	protected:
		[[nodiscard]] bool
		shorts() const noexcept {
			return help() || version();
		}

		[[nodiscard]] virtual int
		do_finalize() const { return 0; }

		[[nodiscard]] virtual bool
		handle_option(char opt, char *optarg) = 0;

		void
		help(const bool h) noexcept { _help = h; }

		void
		version(const bool v) noexcept { _version = v; }

		[[nodiscard]] int
		print_version() const;

		[[nodiscard]] int
		print_usage() const;

		void
		setup_piping_fd();

	private:
		int _child_stdin;
		int _output;

		std::string_view _getopts_arg;
		std::string_view _progname;
		std::string_view _usage_msg;
		bool _help{false};
		bool _version{false};
		int _argc;
		char **_argv;
	};
}

#endif //D3_CONFIG_BUNDLE_HXX
