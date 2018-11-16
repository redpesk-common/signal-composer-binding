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

#include <vector>
#include <string>
#include "source.hpp"

class Composer
{
private:
	CtlConfigT* ctlConfig_;

	static CtlSectionT ctlSections_[]; ///< Config Section definition (note: controls section index should match handle retrieval in)
	std::vector<json_object*> ctlActionsJ_; ///< Vector of action json object to be kept if we want to freed them correctly avoiding leak mem.
	std::vector<std::shared_ptr<SourceAPI>> newSourcesListV_;
	std::vector<std::shared_ptr<SourceAPI>> sourcesListV_;

	explicit Composer(const std::string& filepath);
	Composer();
	~Composer();

	CtlActionT* convert2Action(AFB_ApiT apihandle, const std::string& name, json_object* action);

	int loadOneSourceAPI(AFB_ApiT apihandle, json_object* sourcesJ);
	static int loadSourcesAPI(AFB_ApiT apihandle, CtlSectionT* section, json_object *signalsJ);

	int loadOneSignal(AFB_ApiT apihandle, json_object* signalsJ);
	static int loadSignals(AFB_ApiT apihandle, CtlSectionT* section, json_object *signalsJ);

	int execSignalsSubscription(AFB_ReqT request = nullptr);
	std::shared_ptr<SourceAPI> getSourceAPI(const std::string& api);
	void processOptions(const std::map<std::string, int>& opts, std::shared_ptr<Signal> sig, json_object* response) const;
public:
	static Composer& instance();
	static void* createContext(void* ctx);
	static void destroyContext(void* ctx);
	int loadConfig(AFB_ApiT api, std::string& filepath);
	int loadSources(json_object* sourcesJ);
	int loadSignals(json_object* signalsJ);
	int initSignals(AFB_ReqT request = nullptr);
	void initSourcesAPI();

	CtlConfigT* ctlConfig();
	std::vector<std::shared_ptr<Signal>> getAllSignals();
	std::vector<std::shared_ptr<Signal>> searchSignals(const std::string& aName);
	json_object* getsignalValue(const std::string& sig, json_object* options);
};
