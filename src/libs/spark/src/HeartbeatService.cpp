/*
 * Copyright (c) 2015, 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Core_generated.h"
#include <spark/HeartbeatService.h>
#include <spark/Service.h>
#include <shared/FilterTypes.h>
#include <boost/uuid/uuid_io.hpp>
#include <functional>

namespace ember { namespace spark {

namespace sc = std::chrono;

HeartbeatService::HeartbeatService(boost::asio::io_service& io_service, const Service* service,
                                   log::Logger* logger) : timer_(io_service),
                                   service_(service), logger_(logger) {
	set_timer();
}

void HeartbeatService::on_message(const Link& link, const ResponseToken& token, const void* message) {
	switch(message->data_type()) {
		case messaging::Data::Ping:
			handle_ping(link, message);
			break;
		case messaging::Data::Pong:
			handle_pong(link, message);
			break;
		default:
			LOG_WARN_FILTER(logger_, LF_SPARK)
				<< "[spark] Unhandled message received by core from "
				<< boost::uuids::to_string(link.uuid) << LOG_ASYNC;
	}
}

void HeartbeatService::on_link_up(const spark::Link& link) {
	std::lock_guard<std::mutex> guard(lock_);
	peers_.emplace_front(link);
}

void HeartbeatService::on_link_down(const spark::Link& link) {
	std::lock_guard<std::mutex> guard(lock_);
	peers_.remove(link);
}

void HeartbeatService::handle_ping(const Link& link, const messaging::MessageRoot* message) {
	auto ping = static_cast<const messaging::Ping*>(message->data());
	send_pong(link, ping->timestamp());
}

void HeartbeatService::handle_pong(const Link& link, const messaging::MessageRoot* message) {
	auto pong = static_cast<const messaging::Pong*>(message->data());
	auto time = sc::duration_cast<sc::milliseconds>(sc::steady_clock::now().time_since_epoch()).count();

	if(pong->timestamp()) {
		auto latency = std::chrono::milliseconds(time - pong->timestamp());

		if(latency > LATENCY_WARN_THRESHOLD) {
			LOG_WARN_FILTER(logger_, LF_SPARK)
				<< "[spark] Detected high latency to " << link.description
				<< ":" << boost::uuids::to_string(link.uuid) << LOG_ASYNC;
		}
	}
}

void HeartbeatService::send_ping(const Link& link, std::uint64_t time) {
	auto fbb = std::make_shared<flatbuffers::FlatBufferBuilder>();
	auto msg = messaging::CreateMessageRoot(*fbb, messaging::Service::Core, 0, 0,
		messaging::Data::Ping, messaging::CreatePing(*fbb, time).Union());
	fbb->Finish(msg);
	service_->send(link, fbb);
}

void HeartbeatService::send_pong(const Link& link, std::uint64_t time) {
	auto fbb = std::make_shared<flatbuffers::FlatBufferBuilder>();
	auto msg = messaging::CreateMessageRoot(*fbb, messaging::Service::Core, 0, 0,
		messaging::Data::Pong, messaging::CreatePong(*fbb, time).Union());
	fbb->Finish(msg);
	service_->send(link, fbb);
}

void HeartbeatService::trigger_pings(const boost::system::error_code& ec) {
	if(ec) { // if ec is set, the timer was aborted
		return;
	}

	// generate the time once for all pings
	// not quite as accurate as per-ping but slightly more efficient
	auto time = sc::duration_cast<sc::milliseconds>(
		sc::steady_clock::now().time_since_epoch()).count();

	std::lock_guard<std::mutex> guard(lock_);

	for(auto& link : peers_) {
		send_ping(link, time);
	}

	set_timer();
}

void HeartbeatService::set_timer() {
	timer_.expires_from_now(PING_FREQUENCY);
	timer_.async_wait(std::bind(&HeartbeatService::trigger_pings, this, std::placeholders::_1));
}

void HeartbeatService::shutdown() {
	timer_.cancel();
}

}} // spark, ember