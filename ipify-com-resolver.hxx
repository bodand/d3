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

#ifndef D3_IPIFY_COM_RESOLVER_HXX
#define D3_IPIFY_COM_RESOLVER_HXX

#include <bit>
#include <cassert>
#include <span>

#include "resolver.hxx"

namespace d3 {
	struct ipify_com_resolver final : resolver {
		static void 
		maker(std::span<std::byte> buf, bool ipv4, bool ipv6) {
			assert(buf.size() >= sizeof(ipify_com_resolver));
			::new (std::bit_cast<ipify_com_resolver*>(buf.data())) ipify_com_resolver(ipv4, ipv6);
		}

		static void
		deleter(std::span<std::byte> buf) noexcept {
			std::bit_cast<ipify_com_resolver*>(buf.data())->~ipify_com_resolver();
		}

		void
		resolve_into(std::span<char> resolved) override;

		~ipify_com_resolver() noexcept;

	private:
		ipify_com_resolver(bool ipv4, bool ipv6);

		void* _impl_ctx;
   };
}

#endif

