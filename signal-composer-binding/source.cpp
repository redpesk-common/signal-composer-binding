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
#include <cstring>

#include "source.hpp"
#include "signal-composer.hpp"

SourceAPI::SourceAPI()
{}

SourceAPI::SourceAPI(const std::string& uid, const std::string& api, const std::string& info, CtlActionT* init, CtlActionT* getSignals, CtlActionT* onReceived, int retention):
 uid_{uid},
 api_{api},
 info_{info},
 init_{init},
 getSignals_{getSignals},
 signalsDefault_({onReceived, retention})
{}

bool SourceAPI::operator ==(const SourceAPI& other) const
{
	if(uid_ == other.uid_) {return true;}
	return false;
}

bool SourceAPI::operator ==(const std::string& other) const
{
	if(uid_ == other) {return true;}
	return false;
}

void SourceAPI::init()
{
	if(init_)
	{
		CtlSourceT source;
		::memset(&source, 0, sizeof(CtlSourceT));
		source.uid = init_->uid;
		source.api = afbBindingRoot;
		ActionExecOne(&source, init_, json_object_new_object());
		std::string sourceAPI_events = api_ + "/*";
		afb_api_event_handler_add(afbBindingRoot, sourceAPI_events.c_str(), SourceAPI::onSourceEvents, NULL);
		return;
	}

	if(api_ == afbBindingRoot->apiname)
		{api_ = Composer::instance().ctlConfig()->api;}
}

std::string SourceAPI::api() const
{
	return api_;
}

/// @brief callback for receiving message from low bindings. This will call back
/// an action defined in the configuration files depending on the events
/// received from an API.
///
/// @param[in] object - an opaq pointer holding userdata
/// @param[in] event  - event name
/// @param[in] object - eventual data that comes with the event
/// @param[in] object - the api that subscribed the event
///
void SourceAPI::onSourceEvents(void *closure, const char *event_name, json_object *event_obj, afb_api_t api)
{
	std::vector<std::shared_ptr<Signal>> signals { Composer::instance().searchSignals(event_name) };

	if(signals.empty())
	{
		AFB_NOTICE("This event '%s' isn't registered within the signal composer configuration. Continue.", event_name);
		return;
	}
	// If there is more than 1 element then maybe we can find a more
	// detailled event name in JSON object as 1 event may carry several
	// signals. Try to find that one.
	if(signals.size() > 1)
	{
		bool found = false;
		json_object_iterator iter = json_object_iter_begin(event_obj);
		json_object_iterator iterEnd = json_object_iter_end(event_obj);
		while(!json_object_iter_equal(&iter, &iterEnd))
		{
			json_object *value = json_object_iter_peek_value(&iter);
			if(json_object_is_type(value, json_type_string))
			{
				std::string name = json_object_get_string(value);
				for(auto& sig: signals)
				{
					if(*sig == name)
					{
						found = true;
						sig->onReceivedCB(event_obj);
					}
				}
			}
			json_object_iter_next(&iter);
		}
		// If nothing found in JSON data then apply onReceived callback
		// for all signals found
		if(! found)
		{
			for(auto& sig: signals)
				{sig->onReceivedCB(event_obj);}
		}
	}
	else
	{
		signals[0]->onReceivedCB(event_obj);
	}
}

/// @brief callback for receiving message from low bindings. This will call back
/// an action defined in the configuration files depending on the events
/// received from an API.
///
/// @param[in] object - an opaq pointer holding userdata
/// @param[in] event  - event name
/// @param[in] object - eventual data that comes with the event
/// @param[in] object - the api that subscribed the event
///
void SourceAPI::onSignalEvents(void *closure, const char *event_name, json_object *event_obj, afb_api_t api)
{
	Signal *sig = (Signal*) closure;
	sig->onReceivedCB(event_obj);
}

const struct signalsDefault& SourceAPI::signalsDefault() const
{
	return signalsDefault_;
}

void SourceAPI::addSignal(const std::string& id, const std::string& event, std::vector<std::string>& depends, int retention, const std::string& unit, json_object *metadata, double frequency, CtlActionT* onReceived, json_object* getSignalsArgs)
{
	std::shared_ptr<Signal> sig = std::make_shared<Signal>(id, event, depends, unit, metadata, retention, frequency, onReceived, getSignalsArgs);

	if(!event.empty())
		afb_api_event_handler_add(afbBindingRoot, event.c_str(), SourceAPI::onSignalEvents, (void*)sig.get());

	newSignalsM_[id] = sig;
}

void SourceAPI::initSignals()
{
	int err = 0;
	Composer& composer = Composer::instance();

	for(auto& i: newSignalsM_)
		{i.second->attachToSourceSignals(composer);}

	for(auto i = newSignalsM_.begin(); i != newSignalsM_.end();)
	{
		if (err += i->second->initialRecursionCheck())
		{
			AFB_ERROR("There is an infinite recursion loop in your signals definition. Root coming from signal: %s. Ignoring it.", i->second->id().c_str());
			++i;
			continue;
		}
		signalsM_[i->first] = i->second;
		i = newSignalsM_.erase(i);
	}
}

std::vector<std::shared_ptr<Signal>> SourceAPI::getSignals() const
{
	std::vector<std::shared_ptr<Signal>> signals;
	for (auto& sig: signalsM_)
	{
		signals.push_back(sig.second);
	}
	for (auto& sig: newSignalsM_)
	{
		signals.push_back(sig.second);
	}

	return signals;
}

/// @brief Search a signal in a source instance. If an exact signal name is find
///  then it will be returned else it will be search against each signals
///  contained in the map and signal will be deeper evaluated.
///
/// @param[in] name - A signal name to be searched
///
/// @return Returns a vector of found signals.
std::vector<std::shared_ptr<Signal>> SourceAPI::searchSignals(const std::string& name)
{
	std::vector<std::shared_ptr<Signal>> signals;

	if(signalsM_.count(name))
		{signals.emplace_back(signalsM_[name]);}
	else if(newSignalsM_.count(name))
		{signals.emplace_back(signalsM_[name]);}
	else
	{
		for (auto& sig: signalsM_)
		{
			if(*sig.second == name)
				{signals.emplace_back(sig.second);}
		}
		for (auto& sig: newSignalsM_)
		{
			if(*sig.second == name)
				{signals.emplace_back(sig.second);}
		}
	}

	return signals;
}

int SourceAPI::cleanNotSubscribedSignals() {
	int erased = 0;
	for(auto sig = signalsM_.begin(); sig != signalsM_.end();)
	{
		if(!sig->second->subscribed_)
		{
			sig = signalsM_.erase(sig);
			erased++;
		}
		else
			{sig++;}
	}

	return erased;
}

int SourceAPI::makeSubscription(afb_req_t request)
{
	int err = 0;
	if(getSignals_)
	{
		CtlSourceT source;
		::memset(&source, 0, sizeof(CtlSourceT));
		source.uid = api_.c_str();
		source.api = afbBindingRoot;
		source.request = request;
		json_object *argsSaveJ = getSignals_->argsJ;

		for(auto& sig: signalsM_)
		{
			if(!sig.second->subscribed_)
			{
				json_object* signalJ = sig.second->toJSON();
				if(!signalJ)
				{
					AFB_ERROR("Error building JSON query object to subscribe to for signal %s", sig.second->id().c_str());
					break;
				}
				source.uid = sig.first.c_str();
				source.context = getSignals_->type == CTL_TYPE_CB ?
					getSignals_->exec.cb.plugin->context :
					nullptr;
				getSignals_->argsJ = sig.second->getSignalsArgs() ?
					sig.second->getSignalsArgs() :
					argsSaveJ;

				sig.second->subscribed_ = ActionExecOne(&source, getSignals_, signalJ) ? false : true;
			}
		}
		err += cleanNotSubscribedSignals();
		getSignals_->argsJ = argsSaveJ;
		source.uid = uid_.c_str();
		ActionExecOne(&source, getSignals_, nullptr);
	}

	if(err)
		{AFB_ERROR("%d signal(s) removed because invalid. Please review your signal definition.", err);}

	return err;
}
