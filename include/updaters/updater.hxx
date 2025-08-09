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

#ifndef D3_UPDATER_HXX
#define D3_UPDATER_HXX

#include <optional>
#include <string_view>

namespace d3 {
	struct record {
		std::string_view name;
		std::string_view type;
		std::string_view value;
	};

	struct updater {
		explicit
		updater(const std::optional<std::string_view>& zone)
			: _zone(zone) { }

		virtual ~updater() = default;

		int
		update(const record& rec) {
			const auto zone = _zone
										? *_zone
										: zone_of(rec.name);
			return do_update_dns(zone, rec);
		}

	protected:
		[[nodiscard]] std::optional<std::string_view>
		zone() const { return _zone; }

		virtual int
		do_update_dns(std::string_view zone, const record& rec) = 0;

	private:
		static std::string_view
		zone_of(std::string_view domain);

		std::optional<std::string_view> _zone;
	};
}

#endif
