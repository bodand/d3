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

#include <util/config_bundle.hxx>

#include <print>

#include <unistd.h>

#include <util/xerr.hxx>

void
d3::config_bundle::read_config(int &argc, char **&argv) {
	for (int opt = getopt(argc, argv, _getopts_arg.data());
			opt != -1;
			opt = getopt(argc, argv, _getopts_arg.data())) {
		switch (opt) {
			case 'v': version(true);
				break;
			default:
				if (handle_option(static_cast<char>(opt), optarg)) break;
				[[fallthrough]];
			case 'h':
				help(true);
				break;
		}
	}

	argc -= optind;
	argv += optind;
	_argc = argc;
	_argv = argv;

	if (!shorts()) setup_piping_fd();
}

int
d3::config_bundle::do_shortcircuit() const {
	if (help()) return print_usage();
	if (version()) return print_version();

	return 0;
}

std::optional<int>
d3::config_bundle::finalize() const {
	if (const auto custom = do_finalize()) return custom;

	// no exec... passed, _output was STDOUT, we are done
	if (_argc == 0) return 2;

	// manipulate STDIN into our output
	if (dup2(_child_stdin, STDIN_FILENO) != 0) return xerr(1, "dup2");
	if (close(_output) != 0) return xerr(1, "close");

	return std::nullopt; // all good, proceed to exec
}

bool
d3::config_bundle::handle_option(char, char *) {
	return false;
}

int
d3::config_bundle::print_version() const {
	std::println("{}", "v0.1.0"); // XXX configure generate this
	return 2;
}

int
d3::config_bundle::print_usage() const {
	std::println(
		"d3 utility: {}\n"
		"\n"
		"usage:\n"
		"\t{} [-{}] exec...",
		_usage_msg,
		_progname,
		_getopts_arg
	);
	return 2;
}

void
d3::config_bundle::setup_piping_fd() {
	if (_argc == 0) return;

	int pfd[2];
	if (pipe(pfd) != 0) {
		xerr(1, "pipe");
		throw std::runtime_error("fatal: failed opening pipelining for future exec(2)ee");
	}
	_child_stdin = pfd[0];
	_output = pfd[1];
}
