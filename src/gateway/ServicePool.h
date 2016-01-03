/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/asio.hpp>

namespace ember {

class ServicePool {


public:
	ServicePool(unsigned int size);

	boost::asio::io_service* get_service();
	void run();
};

} // ember