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

#include <float.h>
#include <string.h>
#include <fnmatch.h>
#include <sstream>

#include "signal.hpp"
#include "signal-composer.hpp"
#include "clientApp.hpp"

#define USEC_TIMESTAMP_FLAG 1506514324881224

extern "C" void signal_verb(afb_req_t request)
{
	Composer& composer = Composer::instance();
	json_object *actionJ = afb_req_json(request),
		    *optionsJ = nullptr,
		    *ret = nullptr;
	std::string action;

	Signal* sigraw = (Signal*) afb_req_get_vcbdata(request);
	std::shared_ptr<Signal> sig = sigraw->get_shared_ptr();

	if(json_object_object_get_ex(actionJ, "get", &optionsJ))
	{
		ret = composer.getSignalValue(sig, optionsJ);
		afb_req_success(request, ret, "get");
	}
	else if(json_object_object_get_ex(actionJ, "change", &optionsJ))
	{
		if(sig->change(optionsJ))
			afb_req_fail_f(request, "Changing the configuration of signal '%s' failed.", sig->id().c_str());
		else
			afb_req_success(request, ret, "change");
	}
	else if(json_object_is_type(actionJ, json_type_string))
	{
		clientAppCtx *cContext = reinterpret_cast<clientAppCtx*>(afb_req_context(request, 0, Composer::createContext, Composer::destroyContext, nullptr));
		action = json_object_get_string(actionJ);
		if(action == "get")
		{
			ret = composer.getSignalValue(sig, nullptr);
			afb_req_success(request, ret, "get");
		}
		else if(action == "config")
		{
			afb_req_success(request, sig->config(), "config");
		}
		else if(action == "subscribe")
		{
			cContext->appendSignals(sig);
			if(cContext->makeSubscription(request))
				afb_req_fail_f(request, "Subscription of signal '%s' failed.", sig->id().c_str());
			else
				afb_req_success(request, nullptr, "subscribe");
		}
		else if(action == "unsubscribe")
		{
			cContext->subtractSignals(sig);
			if(cContext->makeUnsubscription(request))
				afb_req_fail_f(request, "Unsubscription of signal '%s' failed.", sig->id().c_str());
			else
				afb_req_success(request, nullptr, "unsubscribe");
		}
	}
	else
		afb_req_fail(request, "JSON argument is not correct", "choose between 'get', 'config', 'change', 'subscribe', 'unsubscribe'");
}

Signal::Signal(const std::string& id, const std::string& event, std::vector<std::string>& depends, const std::string& unit, json_object *metadata, int retention, double frequency, CtlActionT* onReceived, json_object* getSignalsArgs, const char* permission)
:id_(id),
 event_(event),
 dependsSigV_(depends),
 timestamp_(0.0),
 value_(),
 onReceived_(onReceived),
 getSignalsArgs_(getSignalsArgs),
 signalCtx_({nullptr, nullptr}),
 retention_(retention),
 frequency_(frequency),
 unit_(unit),
 metadata_(metadata),
 subscribed_(false)
{
	struct afb_auth* auth = (struct afb_auth*) calloc(1, sizeof(struct afb_auth));
	if(permission)
	{
		auth->type = afb_auth_Permission;
		auth->text = permission;
		auth->next = nullptr;
	}
	else
	{
		auth->type = afb_auth_Yes;
		auth->loa = 0;
		auth->next = nullptr;
	}

	if(afb_api_add_verb(afbBindingRoot, id.c_str(),
	   "Signal verb to interact with",
	   signal_verb,
	   (void*)this,
	   auth, 0, 0))
		AFB_ERROR("Wrongly added verb to the API, this signal could not be reached using its dedicated verb.");
}

Signal::~Signal()
{
	if(metadata_) json_object_put(metadata_);
	if(getSignalsArgs_) json_object_put(getSignalsArgs_);
	if(onReceived_) delete(onReceived_);
}

Signal::operator bool() const
{
	if(id_.empty())
		{return false;}
	return true;
}

bool Signal::operator ==(const Signal& other) const
{
	if(id_ == other.id_) {return true;}
	return false;
}

bool Signal::operator !=(const Signal& other) const
{
	if(id_ != other.id_) {return true;}
	return false;
}

bool Signal::operator ==(const std::string& aName) const
{
	if(! ::fnmatch(aName.c_str(), id_.c_str(), FNM_CASEFOLD) ||
	   ! ::fnmatch(aName.c_str(), event_.c_str(), FNM_CASEFOLD))
		{return true;}

	return false;
}

bool Signal::operator !=(const std::string& aName) const
{
	if(::fnmatch(aName.c_str(), id_.c_str(), FNM_CASEFOLD) &&
	   ::fnmatch(aName.c_str(), event_.c_str(), FNM_CASEFOLD))
		{return true;}

	return false;
}

std::shared_ptr<Signal> Signal::get_shared_ptr()
{
	return shared_from_this();
}

const std::string Signal::id() const
{
	return id_;
}

const std::string Signal::eventName() const
{
	if(event_.find("/"))
		if(event_.length() > event_.find("/"))
			return event_.substr(event_.find("/")+1);
	return event_;
}

/// @brief Build a JSON object with data members of Signal object
///
/// @return the built JSON object representing the Signal
json_object* Signal::toJSON() const
{
	json_object* sigJ = json_object_new_object();
	json_object *nameArrayJ = json_object_new_array();
	std::vector<std::string> dependsSignalName;

	for (const std::string& src: dependsSigV_ )
	{
		ssize_t sep = src.find_first_of("/");
		if(sep != std::string::npos)
			dependsSignalName.push_back(src.substr(sep+1));
	}

	for (const std::string& lowSig: dependsSignalName)
		json_object_array_add(nameArrayJ, json_object_new_string(lowSig.c_str()));

	json_object_object_add(sigJ, "uid", json_object_new_string(id_.c_str()));

	json_object_object_add(sigJ, "getSignalsArgs", json_object_get(getSignalsArgs_));

	if (!event_.empty())
		json_object_object_add(sigJ, "event", json_object_new_string(event_.c_str()));

	if (json_object_array_length(nameArrayJ))
		json_object_object_add(sigJ, "depends", nameArrayJ);
	else
		json_object_put(nameArrayJ);

	if (!unit_.empty())
		json_object_object_add(sigJ, "unit", json_object_new_string(unit_.c_str()));

	if (!metadata_)
		json_object_object_add(sigJ, "metadata", json_object_get(metadata_));

	if (frequency_)
		json_object_object_add(sigJ, "frequency", json_object_new_double(frequency_));

	if(timestamp_)
		json_object_object_add(sigJ, "timestamp", json_object_new_int64(timestamp_));

	if(value_)
		json_object_object_add(sigJ, "value", value_);

	return sigJ;
}

/// @brief Initialize signal context if not already done and return it.
///  Signal context is a handle to be use by plugins then they can call
///  some signal object method setting signal values.
///  Also if plugin set a context it retrieve it and initiaze the pluginCtx
///  member then plugin can find a persistent memory area where to hold its
///  value.
///
/// @return a pointer to the signalCtx_ member initialized.
struct signalCBT* Signal::get_context()
{
	if(!signalCtx_.aSignal)
	{
		signalCtx_.aSignal = (void*)this;
		signalCtx_.pluginCtx = onReceived_ && onReceived_->type == CTL_TYPE_CB
		&& onReceived_->exec.cb.plugin ?
			onReceived_->exec.cb.plugin->context:
			nullptr;
	}

	return &signalCtx_;
}

/// @brief Accessor to getSignalsArgs_ json_object member
///
/// @return a pointer the json_object getSignalsArgs_ private member
json_object *Signal::getSignalsArgs()
{
	return getSignalsArgs_;
}

/// @brief Accessor to retrieve only the modifiable configuration attributes.
/// metadata, retention, unit and frequency.
///
/// @return a pointer the json_object with the configuration attributes
json_object* Signal::config() const
{
	json_object* sigJ = json_object_new_object();

	if (!unit_.empty())
		json_object_object_add(sigJ, "unit", json_object_new_string(unit_.c_str()));
	if (!metadata_)
		json_object_object_add(sigJ, "metadata", json_object_get(metadata_));
	if (frequency_)
		json_object_object_add(sigJ, "frequency", json_object_new_double(frequency_));
	if (retention_)
		json_object_object_add(sigJ, "retention", json_object_new_int(retention_));

	return sigJ;
}

/// @brief Accessor to change the modifiable attributes of the signal
/// metadata, retention, unit and frequency.
///
/// @return 0 on success and other on failure.
int Signal::change(json_object* config)
{
	json_object *optionsJ = nullptr;

	if(json_object_object_get_ex(config, "retention", &optionsJ) &&
	   json_object_is_type(optionsJ, json_type_int))
		retention_ = json_object_get_int(optionsJ);
	else
		return -1;

	if(json_object_object_get_ex(config, "frequency", &optionsJ) &&
	   json_object_is_type(optionsJ, json_type_double))
		frequency_ = json_object_get_int(optionsJ);
	else
		return -1;

	if(json_object_object_get_ex(config, "unit", &optionsJ) &&
	   json_object_is_type(optionsJ, json_type_string))
		unit_ = json_object_get_string(optionsJ);
	else
		return -1;

	if(json_object_object_get_ex(config, "metadata", &optionsJ) &&
	   json_object_is_type(optionsJ, json_type_object))
	{
		json_object_put(metadata_);
		metadata_ = json_object_get(optionsJ);
	}
	else
		{return -1;}

	return 0;
}

/// @brief Set Signal timestamp and value property when an incoming
/// signal arrived. Called by a plugin because treatment can't be
/// standard as signals sources format could changes. See low-can plugin
/// example.
///
/// @param[in] timestamp - timestamp of occured signal
/// @param[in] value - value of change
void Signal::set(uint64_t timestamp, json_object* value)
{
	uint64_t diff = retention_+1;
	value_ = value;
	timestamp_ = timestamp;
	history_[timestamp_] = value_;

	while(diff > retention_)
	{
		uint64_t first = history_.begin()->first;
		diff = (timestamp_ - first)/NANO;
		if(diff > retention_) {
			json_object_put(history_.cbegin()->second);
			history_.erase(history_.cbegin());
		}
	}

	notify();
}

/// @brief Observer method called when an Observable Signal has changed.
///  this will call the onReceived callback with a JSONinifed object of observed
///  signals.
///
///  The signal which triggered the update action will be provided as the last
///  element.
///
/// @param[in] Observable - object from which update come from
void Signal::update(Signal* sig)
{
	json_object *depSigJ = json_object_new_array();
	CtlSourceT source;
	::memset(&source, 0, sizeof(CtlSourceT));
	source.uid = sig->id().c_str();
	source.api = afbBindingRoot;
	source.context = (void*)get_context();
	source.status = CTL_STATUS_EVT ;

	Composer& composer = Composer::instance();

	for(const std::string& depSignal : dependsSigV_)
	{
		if(*sig != depSignal) {
			std::vector<std::shared_ptr<Signal>> vs = composer.searchSignals(depSignal);
			for(const auto& s : vs)
				json_object_array_add(depSigJ, s->toJSON());
		}
	}
	json_object_array_add(depSigJ, sig->toJSON());

	ActionExecOne(&source, onReceived_, depSigJ);
}

/// @brief
///
/// @param[in] eventJ - json_object containing event data to process
///
/// @return 0 if ok, -1 or others if not
void Signal::defaultReceivedCB(Signal *signal, json_object *eventJ)
{
	uint64_t ts = 0;
	json_object* sv = nullptr;
	json_object_iterator iter;
	json_object_iterator iterEnd;

	if (json_object_is_type(eventJ, json_type_array)) {
		sv = eventJ;
		if(!sv)
		{
			AFB_ERROR("No data found to set signal %s with key \"value\" or \"%s\" or \"%s\" in %s", signal->id().c_str(), signal->eventName().c_str(), signal->id().c_str(), json_object_to_json_string(eventJ));
			return;
		}
		else if(ts == 0)
		{
			struct timespec t;

			if(!::clock_gettime(CLOCK_REALTIME, &t))
				ts = (uint64_t)(t.tv_sec) * (uint64_t)NANO + (uint64_t)(t.tv_nsec);
		}

		signal->set(ts, sv);
	}

	iter = json_object_iter_begin(eventJ);
	iterEnd = json_object_iter_end(eventJ);

	while(!json_object_iter_equal(&iter, &iterEnd))
	{
		std::string key = json_object_iter_peek_name(&iter);
		json_object *value = json_object_iter_peek_value(&iter);
		if (key.find("value") != std::string::npos ||
			key.find(signal->eventName()) != std::string::npos ||
			key.find(signal->id()) != std::string::npos)
		{
			sv = json_object_get(value);
		}
		else if (key.find("timestamp") != std::string::npos)
		{
			ts = json_object_is_type(value, json_type_int) ? json_object_get_int64(value):ts;
		}
		json_object_iter_next(&iter);
	}

}

/// @brief Notify observers that there is a change and execute callback defined
/// when signal is received
///
/// @param[in] eventJ - JSON query object to transmit to callback function
///
void Signal::onReceivedCB(json_object *eventJ)
{
	if(onReceived_ && onReceived_->type == CTL_TYPE_LUA)
	{
		json_object_iterator iter = json_object_iter_begin(eventJ);
		json_object_iterator iterEnd = json_object_iter_end(eventJ);
		while(!json_object_iter_equal(&iter, &iterEnd))
		{
			const char *name = ::strdup(json_object_iter_peek_name(&iter));
			json_object *value = json_object_iter_peek_value(&iter);
			if(json_object_is_type(value, json_type_int))
			{
				int64_t newVal = json_object_get_int64(value);
				newVal = newVal > USEC_TIMESTAMP_FLAG ? newVal/NANO:newVal;
				json_object_object_del(eventJ, name);
				json_object* luaVal = json_object_new_int64(newVal);
				json_object_object_add(eventJ, name, luaVal);
			}
			json_object_iter_next(&iter);
		}
	}

	CtlSourceT source;
	::memset(&source, 0, sizeof(CtlSourceT));
	source.uid = id_.c_str();
	source.api = afbBindingRoot;
	source.context = (void*)get_context();
	// Always call the default CB that will set the value in the signal's value
	// if the signal is a raw event instead.
	if (onReceived_)
		ActionExecOne(&source, onReceived_, json_object_get(eventJ));
	if(!event_.empty())
		defaultReceivedCB(this, eventJ);
}

/// @brief Make a Signal observer observes Signals observables
/// set in its observable vector.
///
/// @param[in] composer - bindinApp instance
void Signal::attachToSourceSignals(Composer& composer)
{
	for (const std::string& srcSig: dependsSigV_)
	{
		if(srcSig.find("/") == std::string::npos)
		{
			std::vector<std::shared_ptr<Signal>> observables = composer.searchSignals(srcSig);
			if(observables[0])
			{
				AFB_NOTICE("Attaching %s to %s", id_.c_str(), srcSig.c_str());
				observables[0]->addObserver(this);
				continue;
			}
			AFB_WARNING("Can't attach. Is %s exists ?", srcSig.c_str());
		}
	}
}

/// @brief Make an average over the last X 'seconds'
///
/// @param[in] seconds - period to calculate the average
///
/// @return A json_object with the Average value or an error message
json_object * Signal::average(int seconds) const
{
	uint64_t begin = history_.begin()->first;
	uint64_t end = seconds ?
		begin+(seconds*NANO) :
		history_.rbegin()->first;
	double total = 0.0;
	int nbElt = 0;
	std::stringstream errorMsg;

	if(history_.empty() && seconds < 0)
	{
		errorMsg << "There are no historized values or you requested a negative time interval for that signal: " << id_;
		return json_object_new_string(errorMsg.str().c_str());
	}

	for(const auto& val: history_)
	{
		if(val.first >= end) {
			break;
		}
		if(val.second)
		{
			switch(json_object_get_type(val.second)) {
				case json_type_int:
					total += static_cast<double>(json_object_get_int64(val.second));
					break;
				case json_type_double:
					total += json_object_get_double(val.second);
					break;
				default:
					errorMsg << "The stored value '" << json_object_get_string(val.second) << "' for the signal '" << id_ << "'' isn't numeric, it is not possible to compute an average value.";
					return json_object_new_string(errorMsg.str().c_str());
			}
			nbElt++;
		}
	}

	return json_object_new_double(total / nbElt);
}

/// @brief Find minimum in the recorded values
///
/// @param[in] seconds - period to find the minimum
///
/// @return A json_object with  the Minimum value contained in the history or the error string
json_object *Signal::minimum(int seconds) const
{
	uint64_t begin = history_.begin()->first;
	uint64_t end = seconds ?
	begin+(seconds*NANO) :
	history_.rbegin()->first;
	double current = 0.0;

	double min = DBL_MAX;
	std::stringstream errorMsg;

	if(history_.empty() && seconds < 0)
	{
		errorMsg << "There is no historized values or you requested a negative time interval for that signal: " << id_;
		return json_object_new_string(errorMsg.str().c_str());
	}

	for(const auto& val : history_)
	{
		if(val.first >= end) {
			break;
		}
		else if(val.second) {
			switch(json_object_get_type(val.second)) {
				case json_type_int:
					current = static_cast<double>(json_object_get_int64(val.second));
					min = current < min ? current : min;
					break;
				case json_type_double:
					current = json_object_get_double(val.second);
					min = current < min ? current : min;
					break;
				default:
					errorMsg << "The stored value '" << json_object_get_string(val.second) << "' for the signal '" << id_ << "'' isn't numeric, it is not possible to find a minimum value.";
					return json_object_new_string(errorMsg.str().c_str());
			}
		}
	}

	return json_object_new_double(min);
}

/// @brief Find maximum in the recorded values
///
/// @param[in] seconds - period to find the maximum
///
/// @return A json_object with Maximum value contained in the history or an error string
json_object * Signal::maximum(int seconds) const
{
	uint64_t begin = history_.begin()->first;
	uint64_t end = seconds ?
	begin+(seconds*NANO) :
	history_.rbegin()->first;
	double current = 0.0;

	double max = 0.0;
	std::stringstream errorMsg;

	if(history_.empty() && seconds < 0) {
		errorMsg << "There is no historized values or you requested a negative time interval for that signal: " << id_;
		return json_object_new_string(errorMsg.str().c_str());
	}

	for(const auto& val : history_)
	{
		if(val.first >= end) {
			break;
		}
		else if(val.second) {
			switch(json_object_get_type(val.second)) {
				case json_type_int:
					current = static_cast<double>(json_object_get_int64(val.second));
					max = current > max ? current : max;
					break;
				case json_type_double:
					current = json_object_get_double(val.second);
					max = current > max ? current : max;
					break;
				default:
					errorMsg << "The stored value '" << json_object_get_string(val.second) << "' for the signal '" << id_ << "'' isn't numeric, it is not possible to find a maximum value.";
					return json_object_new_string(errorMsg.str().c_str());
			}
		}
	}

	return json_object_new_double(max);
}

/// @brief Return last value recorded
///
/// @return Last value
json_object* Signal::last_value() const
{
	return json_object_get(value_);
}

uint64_t Signal::last_timestamp() const
{
	return timestamp_;
}

/// @brief Recursion check to ensure that there is no infinite loop
/// in the Observers/Observables structure.
/// This will check that observer signal is not the same than itself
/// then trigger the check against the following eventuals observers
///
/// @return 0 if no infinite loop detected, -1 if not.
int Signal::initialRecursionCheck()
{
	for (auto& obs: observerList_)
	{
		if(obs == this)
			{return -1;}
		if(static_cast<Signal*>(obs)->recursionCheck())
			{return -1;}
	}
	return 0;
}

/// @brief Inner recursion check. Argument is the Signal id coming
/// from the original Signal that made a recursion check.
///
/// @param[in] origId - name of the origine of the recursion check
///
/// @return 0 if no infinite loop detected, -1 if not.
int Signal::recursionCheck()
{
	for (const auto& obs: observerList_)
	{
		Signal* obsSig = static_cast<Signal*>(obs);
		if(this->id() == obsSig->id())
			{return -1;}
 		if(obsSig->recursionCheck())
			{return -1;}
	}
	return 0;
}
