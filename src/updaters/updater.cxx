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

#include <updaters/updater.hxx>

std::string_view
d3::updater::zone_of(std::string_view domain) {
	// Get the last max. two sections of the domain:
	//		my.example.com -> example.com
	//		my.example.com. -> example.com
	//		my.other.example.com -> example.com
	//		my.com -> my.com
	//		tld.	-> tld (this will likely fail later, unlikely that tld
	//						  maintainers are going to use d3 in any capacity)

	if (domain.back() == '.') domain = domain.substr(0, domain.size() - 1);

	std::string_view searcher = domain;

	int found = 0;
	for (; found < 2; ++found) {
		const auto next_dot = searcher.rfind('.');
		if (next_dot == std::string_view::npos) break;

		searcher = searcher.substr(0, next_dot);
	}

	// tld or my.tld
	if (found == 0 || found == 1) return domain;
	// some.other.things.my.tld
	return domain.substr(searcher.size() + 1);
}
