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

#ifndef D3_RESOLVER_HXX
#define D3_RESOLVER_HXX

#include <cstddef>
#include <stdexcept>
#include <span>

namespace d3 {
	struct resolver {
		resolver(bool ipv4, bool ipv6)
			: _ipv4(ipv4)
			, _ipv6(ipv6) { }
		virtual ~resolver() noexcept = default;

		void
		resolve(std::span<char> sv) {
			if (!large_enough_for_resolve(sv.size())) 
				throw std::runtime_error("passed buffer is not long enough for requested resolve");
			resolve_into(sv);
		}
	protected:
		virtual void
		resolve_into(std::span<char> sv) = 0;

		bool 
		ipv4() const noexcept { return _ipv4; }

		bool 
		ipv6() const noexcept { return _ipv6; }

	private:
		bool
		large_enough_for_resolve(std::size_t buf_sz);

		bool _ipv4, _ipv6;
	};
}

#endif
