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

#ifndef D3_POLYMORPH_HXX
#define D3_POLYMORPH_HXX

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <cstddef>
#include <new>

namespace d3 {
	template<class>
	struct type_tag {};

	template<class TBase, std::size_t Size = 1024>
	struct polymorph {
		TBase *
		get() noexcept {
			return std::launder(std::bit_cast<TBase *>(data.data()));
		}

		const TBase *
		get() const noexcept {
			return std::launder(std::bit_cast<const TBase *>(data.data()));
		}

		TBase &
		operator*() noexcept {
			return *std::launder(std::bit_cast<TBase *>(data.data()));
		}

		const TBase &
		operator*() const noexcept {
			return *std::launder(std::bit_cast<const TBase *>(data.data()));
		}

		TBase *
		operator->() noexcept {
			return std::launder(std::bit_cast<TBase *>(data.data()));
		}

		const TBase *
		operator->() const noexcept {
			return std::launder(std::bit_cast<const TBase *>(data.data()));
		}

		~polymorph() noexcept(std::is_nothrow_destructible_v<TBase>) {
			std::launder(std::bit_cast<TBase *>(data.data()))->~TBase();
		}

	private:
		template<class T1, class T2, std::size_t N, class... Args>
		friend polymorph<T1, N>
		make_polymorph(Args &&...);

		template<class T, class... Args>
		explicit
		polymorph(type_tag<T>, Args &&... args) {
			new(data.data()) T(std::forward<Args>(args)...);
		}

		alignas(std::max_align_t) std::array<std::byte, Size> data;
	};

	template<class TBase, class T, std::size_t N = 1024, class... Args>
	polymorph<TBase, N>
	make_polymorph(Args &&... args) {
		static_assert(sizeof(T) <= N, "polymorph size too small");
		static_assert(std::is_constructible_v<T, std::remove_cvref_t<Args>...>, "cannot construct T with given Args...");
		static_assert(std::derived_from<T, TBase>, "T is not dervied from TBase stored in polymorph");

		return polymorph<TBase, N>(type_tag<T>{},
											std::forward<Args>(args)...);
	}
}

#endif
