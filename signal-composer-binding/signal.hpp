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

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <ctl-config.h>

#include "observer-pattern.hpp"

#define NANO 1000000000

class Composer;

extern "C" void signal_verb(afb_req_t request);

/// @brief Holds composer callbacks and obj to manipulate
struct signalCBT
{
	void* aSignal;
	void* pluginCtx;
};


/// @brief Holds a signal (raw or virtual) definition. Value could be of
///  different types, so an intermediate structure is use to store them.
///  A signal could also be a subject or an observer at the same time, this
///  is the behavior retained to compose virtual signals based on raw signals.
///  And this is the way that is used to update and reacts to an underlaying
///  signal change.
class Signal: public std::enable_shared_from_this<Signal>, public Observable<Signal>, public Observer<Signal>
{
private:
	std::string id_;
	std::string event_;
	std::vector<std::string> dependsSigV_;
	uint64_t timestamp_;
	json_object* value_;
	std::map<uint64_t, json_object*> history_; ///< history_ - Hold signal value history in map with <timestamp, value>
	int retention_;
	double frequency_;
	std::string unit_;
	json_object* metadata_;
	CtlActionT* onReceived_;
	json_object* getSignalsArgs_;
	struct signalCBT signalCtx_;

public:
	bool subscribed_; ///< subscribed_ - boolean value telling if yes or no the signal has been subcribed to the low level binding.
	Signal(const std::string& id,
		const std::string& event,
		std::vector<std::string>& depends,
		const std::string& unit,
		json_object *metadata,
		int retention,
		double frequency,
		CtlActionT* onReceived,
		json_object* getSignalsArgs,
		const char* permission);
	~Signal();

	explicit operator bool() const;
	bool operator==(const Signal& other) const;
	bool operator!=(const Signal& other) const;
	bool operator==(const std::string& aName) const;
	bool operator!=(const std::string& aName) const;
	std::shared_ptr<Signal> get_shared_ptr();

	const std::string id() const;
	json_object* toJSON() const;
	struct signalCBT* get_context();
	json_object *getSignalsArgs();

	json_object* config() const;
	int change(json_object* config);
	void set(uint64_t timestamp, json_object* value);
	void update(Signal* sig);
	static void defaultReceivedCB(Signal *signal, json_object *eventJ);
	void onReceivedCB(json_object *eventJ);
	void attachToSourceSignals(Composer& composer);

	json_object* average(int seconds = 0) const;
	json_object* minimum(int seconds = 0) const;
	json_object* maximum(int seconds = 0) const;
	json_object* last_value() const;
	uint64_t last_timestamp() const;

	int initialRecursionCheck();
	int recursionCheck();
};
