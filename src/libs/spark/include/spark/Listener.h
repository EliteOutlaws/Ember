/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Logging.h>
#include <boost/asio.hpp>

namespace ember { namespace spark {

class SessionManager;

class Listener {
	boost::asio::io_service& service_;
	boost::asio::ip::tcp::acceptor acceptor_;
	boost::asio::ip::tcp::socket socket_;

	SessionManager& sessions_;
	log::Logger* logger_;
	log::Filter filter_;

	void accept_connection();
	void start_session(boost::asio::ip::tcp::socket socket);

public:
	Listener(boost::asio::io_service& service, std::string interface, std::uint16_t port,
	         SessionManager& sessions, log::Logger* logger, log::Filter filter);

	void shutdown();
};

}} // spark, ember