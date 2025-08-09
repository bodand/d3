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

#include <algorithm>
#include <format>
#include <array>
#include <iostream>

#include <updaters/cloudflare_updater.hxx>

#include <ext/json.hpp>
#include <curl/curl.h>

using namespace std::literals;

namespace {
	constexpr unsigned max_domain_sz = std::max(HOST_NAME_MAX, 255);
	constexpr auto max_record_type_sz = 16u;
	constexpr auto max_record_value_sz = 65536u;

	constexpr auto zone_listing_url_fmt = "https://api.cloudflare.com/client/v4/zones?name={0}"sv;
	constexpr auto zone_listing_url_buf_sz = zone_listing_url_fmt.size() + max_domain_sz;

	constexpr auto zone_id_sz = "00000000000000000000000000000000"sv.size(); // valid id copy-pasted and vi"r0
	constexpr auto record_id_sz = zone_id_sz;

	constexpr auto record_listing_url_fmt =
			"https://api.cloudflare.com/client/v4/zones/{0}/dns_records?type={1}&name={2}"sv;
	constexpr auto record_listing_url_buf_sz = record_listing_url_fmt.size()
															+ zone_id_sz + max_record_type_sz + max_domain_sz;

	constexpr auto record_patch_url_fmt = "https://api.cloudflare.com/client/v4/zones/{0}/dns_records/{1}"sv;
	constexpr auto record_patch_url_buf_sz = record_patch_url_fmt.size() + zone_id_sz + record_id_sz;

	constexpr auto payload_fmt = R"({{"name":"{0}","type":"{1}","content":"{2}"}})"sv;
	constexpr auto payload_buf_sz = payload_fmt.size()
												+ max_domain_sz + max_record_type_sz + max_record_value_sz;

	size_t
	write_curl_to_string(const char* data,
								const size_t size,
								const size_t count,
								void* buf) {
		const auto real_sz = size * count;
		const auto read_buf = static_cast<std::string*>(buf);
		read_buf->append(data, real_sz);
		return real_sz;
	}

	CURLcode
	curl_load(std::string& buf,
				CURL* curl,
				curl_slist* headers,
				const std::string_view url) {
		buf.clear();
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, nullptr);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, nullptr);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
		curl_easy_setopt(curl, CURLOPT_URL, url.data());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&buf));
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_curl_to_string);
		return curl_easy_perform(curl);
	}

	CURLcode
	curl_patch(std::string& buf,
					const std::string_view payload,
					CURL* curl,
					curl_slist* headers,
					const std::string_view url) {
		buf.clear();
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
		curl_easy_setopt(curl, CURLOPT_URL, url.data());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data());
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&buf));
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_curl_to_string);
		return curl_easy_perform(curl);
	}
}

d3::cloudflare_updater::cloudflare_updater(const std::optional<std::string_view>& zone)
	: updater(zone)
	, _curl(curl_easy_init()) {
	if (!_curl) throw std::runtime_error("failed to initialize curl");
	_curl_read_buffer.reserve(2048);

	if (!((_json_headers =
			curl_slist_append(static_cast<curl_slist*>(_json_headers), "Content-Type: application/json")))) {
		cleanup();
		throw std::bad_alloc();
	}
	if (!((_empty_headers = curl_slist_append(static_cast<curl_slist*>(_empty_headers), "Content-Type:")))) {
		cleanup();
		throw std::bad_alloc();
	}

	const auto* token = getenv("D3_CF_TOKEN");
	if (!token) throw std::runtime_error("failed to get D3_CF_TOKEN environment variable: is it set?");

	if (const auto res = curl_easy_setopt(_curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		res != CURLE_OK)
		throw std::runtime_error(std::format("failed to set curl to use Bearer token authentication: {}",
														curl_easy_strerror(res)));
	if (const auto res = curl_easy_setopt(_curl, CURLOPT_XOAUTH2_BEARER, token);
		res != CURLE_OK)
		throw std::runtime_error(std::format("failed to set Bearer token: {}", curl_easy_strerror(res)));
}

d3::cloudflare_updater::~cloudflare_updater() { cleanup(); }

int
d3::cloudflare_updater::do_update_dns(const std::string_view zone, const record& rec) {
	const auto zone_id = load_zone_id(zone);

	std::array<char, zone_id_sz> record_id_buf;
	const auto record_id = load_record_id(record_id_buf, zone_id, rec);

	if (patch_record(zone_id, record_id, rec)) return 0;
	return 1;
}

void
d3::cloudflare_updater::cleanup() {
	curl_slist_free_all(static_cast<curl_slist*>(_json_headers));
	curl_slist_free_all(static_cast<curl_slist*>(_empty_headers));
	curl_easy_cleanup(_curl);
}

std::string_view
d3::cloudflare_updater::load_zone_id(std::string_view zone) {
	if (zone == _zone_id_buffer_for) return std::string_view(_zone_id_buf, std::size(_zone_id_buf));

	std::array<char, zone_listing_url_buf_sz> url_buf;
	const auto [out, _written] = std::format_to_n(url_buf.data(), std::size(url_buf),
																zone_listing_url_fmt,
																zone);
	const auto url_sz = static_cast<std::size_t>(out - url_buf.data());
	const auto url = std::string_view(url_buf.data(), url_sz);
	url_buf.at(url_sz) = 0; // ensure url is zero terminated because C sucks

	if (const auto res = curl_load(_curl_read_buffer, _curl, static_cast<curl_slist*>(_empty_headers), url);
		res != CURLE_OK)
		throw std::runtime_error(std::format("cloudflare_updater: could not get zone-id from Cloudflare API: {}",
														curl_easy_strerror(res)));

	const auto response = nlohmann::json::parse(_curl_read_buffer);
	const auto& result = response["result"];
	assert(result.is_array() && "response result is not an array");
	if (result.size() < 1)
		throw std::runtime_error(std::format("cloudflare_updater: invalid zone query: no zone matches {}",
														zone));
	if (result.size() > 1) //
		throw std::runtime_error("cloudflare_updater: invalid zone query: zone name not unique for API token");

	const auto& zone_obj = result.front();
	assert(zone_obj.is_object() && "zone object is not an object");
	assert(zone_obj["name"] == zone);

	const auto id = zone_obj["id"].get<std::string>();

	std::ranges::copy(zone, _zone_id_buffer_for_buf);
	_zone_id_buffer_for = std::string_view(_zone_id_buffer_for_buf, std::size(zone));
	std::ranges::copy_n(id.cbegin(), id.size(), _zone_id_buf);
	return {_zone_id_buf, std::size(_zone_id_buf)};
}

std::string_view
d3::cloudflare_updater::load_record_id(std::span<char> id, std::string_view zone_id, const record& rec) {
	std::array<char, record_listing_url_buf_sz> url_buf;
	const auto [out, _written] = std::format_to_n(url_buf.data(), std::size(url_buf),
																record_listing_url_fmt,
																zone_id,
																rec.type,
																rec.name);
	const auto url_sz = static_cast<std::size_t>(out - url_buf.data());
	const auto url = std::string_view(url_buf.data(), url_sz);
	url_buf.at(url_sz) = 0; // ensure url is zero terminated because C sucks

	if (const auto res = curl_load(_curl_read_buffer, _curl, static_cast<curl_slist*>(_empty_headers), url);
		res != CURLE_OK)
		throw std::runtime_error(std::format(
			"cloudflare_updater: could not get record-id from Cloudflare API: {} (zoneid {}, type {}, name {})",
			curl_easy_strerror(res),
			zone_id,
			rec.type,
			rec.name));

	const auto response = nlohmann::json::parse(_curl_read_buffer);
	const auto& result = response["result"];
	assert(result.is_array() && "response result is not an array");
	if (result.size() < 1)
		throw std::runtime_error(std::format(
			"cloudflare_updater: invalid record query: no record matches {1} (zone {0}, type {2}, see CAVEATS in d3-update(1))",
			zone_id,
			rec.type,
			rec.name));
	if (result.size() > 1) //
		throw std::runtime_error(std::format(
			"cloudflare_updater: invalid record query: record {1} not unique in zone (zone {0}, type {2})",
			zone_id,
			rec.type,
			rec.name));

	const auto& zone_obj = result.front();
	assert(zone_obj.is_object() && "record object is not an object");
	assert(zone_obj["name"] == rec.name);

	const auto ptr = zone_obj["id"].get_ptr<const std::string*>();
	std::ranges::copy(*ptr, id.begin());
	return std::string_view{id.data(), ptr->size()};
}

bool
d3::cloudflare_updater::patch_record(std::string_view zone_id, std::string_view record_id, const record& rec) {
	std::array<char, record_patch_url_buf_sz> url_buf;
	const auto [out, _written] = std::format_to_n(url_buf.data(), std::size(url_buf),
																record_patch_url_fmt,
																zone_id,
																record_id);
	const auto url_sz = static_cast<std::size_t>(out - url_buf.data());
	const auto url = std::string_view(url_buf.data(), url_sz);
	url_buf.at(url_sz) = 0; // ensure url is zero terminated because C sucks

	std::array<char, payload_buf_sz> payload_buf;
	const auto [payload_out, _py_w] = std::format_to_n(payload_buf.data(), std::size(payload_buf),
														payload_fmt,
														rec.name,
														rec.type,
														rec.value);
	const auto payload_sz = static_cast<std::size_t>(payload_out - payload_buf.data());
	const auto payload = std::string_view(payload_buf.data(), payload_sz);
	payload_buf.at(payload_sz) = 0; // ensure payload is zero terminated because C sucks

	if (const auto res = curl_patch(_curl_read_buffer, payload, _curl, static_cast<curl_slist*>(_json_headers), url);
		res != CURLE_OK)
		throw std::runtime_error(std::format(
			"cloudflare_updater: could not update on Cloudflare: {} (zone-id {}, record-id {}, type {}, name {})",
			curl_easy_strerror(res),
			zone_id,
			record_id,
			rec.type,
			rec.name));

	const auto response = nlohmann::json::parse(_curl_read_buffer);
	const auto success = response["success"].get<bool>();
	const auto& errors = response["errors"];
	assert(errors.is_array() && "errors list not array");

	for (const auto& error : errors) {
		assert(error.is_object() && "error object is not an object");
		const auto code = error["code"].get<int>();
		const auto& msg = error["message"];

		const auto msg_string = msg.get_ptr<const std::string*>();

		std::println(std::cerr, "cloudflare_updater: error returned from Cloudflare: {}: {}",
			code,
			*msg_string);
	}

	return success;
}
