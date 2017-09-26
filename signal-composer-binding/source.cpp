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

SourceAPI::SourceAPI(const std::string& api, const std::string& info, CtlActionT* init, CtlActionT* getSignals):
	api_(api), info_(info), init_(init), getSignals_(getSignals)
{}

int SourceAPI::init()
{
	if(init_)
		{return ActionExecOne(init_, nullptr);}
	else if(api_ == afbBindingV2.api)
		{api_ = Composer::instance().ctlConfig()->api;}

	return 0;
}

std::string SourceAPI::api() const
{
	return api_;
}

void SourceAPI::addSignal(const std::string& id, const std::string& event, std::vector<std::string>& depends, int retention, const std::string& unit, double frequency, CtlActionT* onReceived, json_object* getSignalsArgs)
{
	std::shared_ptr<Signal> sig = std::make_shared<Signal>(id, event, depends, unit, retention, frequency, onReceived, getSignalsArgs);

	signalsMap_[id] = sig;
}

std::vector<std::shared_ptr<Signal>> SourceAPI::getSignals() const
{
	std::vector<std::shared_ptr<Signal>> signals;
	for (auto& sig: signalsMap_)
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

	if(signalsMap_.count(name))
		{signals.emplace_back(signalsMap_[name]);}
	else
	{
		for (auto& sig: signalsMap_)
		{
			if(*sig.second == name)
				{signals.emplace_back(sig.second);}
		}
	}

	return signals;
}

int SourceAPI::makeSubscription()
{
	int err = 0;
	if(getSignals_)
	{
		for(auto& sig: signalsMap_)
		{
			json_object* signalJ = sig.second->toJSON();
			if(!signalJ)
			{
				AFB_ERROR("Error building JSON query object to subscribe to for signal %s", sig.second->id().c_str());
				err = -1;
				break;
			}
			err += ActionExecOne(getSignals_, signalJ);
			if(err)
				{AFB_WARNING("Fails to subscribe to signal '%s/%s'",
				api_.c_str(), sig.second->id().c_str());}
			else
				{sig.second->subscribed_ = true;}
		}
		err += ActionExecOne(getSignals_, nullptr);
	}

	return err;
}
