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

#ifndef D3_CLOUDFLARE_UPDATER_HXX
#define D3_CLOUDFLARE_UPDATER_HXX

#include <algorithm>
#include <climits>

#include <updaters/updater.hxx>
#include <util/polymorph.hxx>

namespace d3 {
	struct cloudflare_updater final : updater {
		static polymorph<updater>
		build(const std::optional<std::string_view>& zone) {
			return d3::make_polymorph<updater, cloudflare_updater>(zone);
		}

		explicit
		cloudflare_updater(const std::optional<std::string_view>& zone);

		~cloudflare_updater() override;

	protected:
		int
		do_update_dns(std::string_view zone, const record& rec) override;

	private:
		void
		cleanup();

		std::string_view
		load_zone_id(std::string_view zone);

		std::string_view
		load_record_id(std::span<char> id_buf, std::string_view zone_id, const record& rec);

		bool
		patch_record(std::string_view zone_id, std::string_view record_id, const record& rec);

		// shared dynamic buffer for curl responses, reused between multiple
		// calls to ease allocation pressure
		std::string _curl_read_buffer{ };
		// type erased curl handle
		void* _curl;
		void* _json_headers{ };
		void* _empty_headers{ };

		char _zone_id_buffer_for_buf[std::max(HOST_NAME_MAX, 255) + 1]{ };
		std::string_view _zone_id_buffer_for{ };
		char _zone_id_buf[sizeof("00000000000000000000000000000000") - 1]{ };
	};
}

#endif
