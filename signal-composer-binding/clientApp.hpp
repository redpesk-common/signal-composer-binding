/*
 * Copyright (C) 2015, 2016 "IoT.bzh"
 * Author "Romain Forlot" <romain.forlot@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#pragma once

#include "signal-composer.hpp"

class clientAppCtx: public Observer<Signal>
{
private:
	std::string uuid_;
	std::vector<std::shared_ptr<Signal>> subscribedSignals_;
	afb_event_t event_;
public:
	explicit clientAppCtx(const char* uuid);

	void update(Signal* sig);
	void appendSignals(std::shared_ptr<Signal> sig);
	void subtractSignals(std::shared_ptr<Signal> sig);
	int makeSubscription(afb_req_t  request);
	int makeUnsubscription(afb_req_t  request);
	std::string getUUID() { return std::string(uuid_); }
};
