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

#include <cstddef>
#include <stdexcept>

#include <unistd.h>

#include <curl/curl.h>

#include <resolvers/ipify-com-resolver.hxx>

d3::ipify_com_resolver::ipify_com_resolver(bool ipv4, bool ipv6)
	: resolver(ipv4, ipv6)
	, _impl_ctx(curl_easy_init()) {
	if (!_impl_ctx) throw std::runtime_error("cannot initialize curl backend for ipify resolver");
}

namespace {
	constexpr std::string_view ipv4_url = "https://api4.ipify.org";
	constexpr std::string_view ipv4_marker = "A\t";
	constexpr std::string_view ipv6_url = "https://api6.ipify.org";
	constexpr std::string_view ipv6_marker = "AAAA\t";

	struct read_buffer {
		const std::string_view *marker;
		int output;

		ssize_t
		write(const std::string_view value) const noexcept {
			::write(output, marker->data(), marker->size());
			const auto written = ::write(output, value.data(), value.size());
			::write(output, "\n", 1);
			return written;
		}
	};

	size_t
	write_curl_buf(const char *data,
						const size_t size,
						const size_t count,
						void *buf) {
		const auto real_sz = size * count;
		const auto read_buf = static_cast<read_buffer *>(buf);
		return read_buf->write(std::string_view(data, real_sz));
	}

	bool
	load_ip_at(CURL *curl, const char *url, read_buffer *buf) {
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(buf));
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_curl_buf);
		return curl_easy_perform(curl) == CURLE_OK;
	}
}

void
d3::ipify_com_resolver::resolve_into(const int output) {
	const auto curl = _impl_ctx;
	read_buffer buf{
		.marker = nullptr,
		.output = output
	};

	if (ipv4()) {
		buf.marker = &ipv4_marker;
		load_ip_at(curl, ipv4_url.data(), &buf);
	}
	if (ipv6()) {
		buf.marker = &ipv6_marker;
		load_ip_at(curl, ipv6_url.data(), &buf);
	}
}

d3::ipify_com_resolver::~ipify_com_resolver() noexcept {
	curl_easy_cleanup(_impl_ctx);
}
