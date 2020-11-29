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

#include "clientApp.hpp"

clientAppCtx::clientAppCtx(const char* uuid)
: uuid_{uuid},
  event_{nullptr}
{}

void clientAppCtx::update(Signal* sig)
{
	json_object* sigJ = sig->toJSON();

	if(afb_event_is_valid(event_)) {
		if(!afb_event_push(event_, sigJ))
			{sig->delObserver(this);}
	}
	else
		{json_object_put(sigJ);}
}

void clientAppCtx::appendSignals(std::shared_ptr<Signal> sig)
{
	bool set = false;
	// Clean up already subscribed signals to avoid duplicata
	for (auto& ctxSig: subscribedSignals_)
		if(sig == ctxSig)
			set = true;
	if (set)
	{
		AFB_INFO("Already subscribed, ignoring...");
		return;
	}

	sig->addObserver(this);
	subscribedSignals_.push_back(sig);
}

void clientAppCtx::subtractSignals(std::shared_ptr<Signal> sig)
{
	// Clean up already subscribed signals to avoid duplicata
	for (auto ctxSig = subscribedSignals_.cbegin(); ctxSig != subscribedSignals_.cend();ctxSig++)
	{
		if(sig == *ctxSig)
		{
			subscribedSignals_.erase(ctxSig);
			break;
		}
	}
	sig->delObserver(this);
	AFB_INFO("Signal %s delete from subscription", sig->id().c_str());
}

int clientAppCtx::makeSubscription(afb_req_t request)
{
	event_ = afb_event_is_valid(event_) ?
		event_ : afb_daemon_make_event(uuid_.c_str());
	return afb_req_subscribe(request, event_);
}

int clientAppCtx::makeUnsubscription(afb_req_t request)
{
	if(subscribedSignals_.empty())
	{
		AFB_NOTICE("No more signals subscribed, releasing.");
		return afb_event_is_valid(event_) ?
			afb_req_unsubscribe(request, event_) : -1;
	}
	return 0;
}
