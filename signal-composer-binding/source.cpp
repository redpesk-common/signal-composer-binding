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
		source.uid = init_->uid;
		ActionExecOne(&source, init_, json_object_new_object());
		return;
	}
	else if(api_ == afbBindingV3root->apiname)
		{api_ = Composer::instance().ctlConfig()->api;}
}

std::string SourceAPI::api() const
{
	return api_;
}

const struct signalsDefault& SourceAPI::signalsDefault() const
{
	return signalsDefault_;
}

void SourceAPI::addSignal(const std::string& id, const std::string& event, std::vector<std::string>& depends, int retention, const std::string& unit, json_object *metadata, double frequency, CtlActionT* onReceived, json_object* getSignalsArgs)
{
	std::shared_ptr<Signal> sig = std::make_shared<Signal>(id, event, depends, unit, metadata, retention, frequency, onReceived, getSignalsArgs);

	newSignalsM_[id] = sig;
}

void SourceAPI::initSignals()
{
	Composer& composer = Composer::instance();
	int err = 0;
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

int SourceAPI::makeSubscription(AFB_ReqT request)
{
	int err = 0;
	if(getSignals_)
	{
		CtlSourceT source;
		source.uid = api_.c_str();
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
