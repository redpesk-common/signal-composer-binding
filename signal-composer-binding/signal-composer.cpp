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

#include <uuid.h>
#include <string.h>
#include <fnmatch.h>

#include "clientApp.hpp"

// aSignal member value will be initialized in sourceAPI->addSignal()
static struct signalCBT pluginHandle = {
	.aSignal = nullptr,
	.pluginCtx = nullptr,
};

CtlSectionT Composer::ctlSections_[] = {
	[0]={.key="plugins" , .uid="plugins", .info=nullptr,
		.loadCB=PluginConfig,
		.handle=&pluginHandle,
		.actions=nullptr},
	[1]={.key="sources" , .uid="sources", .info=nullptr,
		 .loadCB=loadSourcesAPI,
		 .handle=nullptr,
		 .actions=nullptr},
	[2]={.key="signals" , .uid="signals", .info=nullptr,
		 .loadCB=loadSignals,
		 .handle=nullptr,
		 .actions=nullptr},
	[3]={.key=nullptr, .uid=nullptr, .info=nullptr,
		 .loadCB=nullptr,
		 .handle=nullptr,
		 .actions=nullptr}
};

///////////////////////////////////////////////////////////////////////////////
//                             PRIVATE METHODS                               //
///////////////////////////////////////////////////////////////////////////////

Composer::Composer()
{}

Composer::~Composer()
{
	// This will free onReceived_ action member from signal objects
	// Not the best to have it occurs here instead of in Signal destructor
	for(auto& j: ctlActionsJ_)
	{
		if(j) json_object_put(j);
	}
	if (ctlConfig_->configJ) json_object_put(ctlConfig_->configJ);
	if (ctlConfig_->requireJ)json_object_put(ctlConfig_->requireJ);
	free(ctlConfig_);
}

CtlActionT* Composer::convert2Action(afb_api_t apihandle, const std::string& name, json_object* actionJ)
{
	CtlActionT *ctlAction = new CtlActionT;

	json_object_object_add(actionJ, "uid", json_object_new_string(name.c_str()));

	if(! ActionLoadOne(apihandle, ctlAction, actionJ, 0))
		{return ctlAction;}
	delete(ctlAction);
	return nullptr;
}

int Composer::loadOneSourceAPI(afb_api_t apihandle, json_object* sourceJ)
{
	json_object *initJ = nullptr,
				*getSignalsJ = nullptr,
				*onReceivedJ = nullptr;
	CtlActionT  *initCtl = nullptr,
				*getSignalsCtl = nullptr,
				*onReceivedCtl = nullptr;
	const char *uid, *api, *info;
	int retention = 0;

	int err = wrap_json_unpack(sourceJ, "{ss,ss,s?o,s?o,s?o,s?o,s?i !}",
			"uid", &uid,
			"api", &api,
			"info", &info,
			"init", &initJ,
			"getSignals", &getSignalsJ,
			// Signals field to make signals conf by sources
			"onReceived", &onReceivedJ,
			"retention", &retention);
	if (err)
	{
		AFB_ERROR("Missing or incorrect field uid|api|[info]|[init]|[getSignals]|[onReceived]|[retention] in %s", json_object_get_string(sourceJ));
		return err;
	}

	// Checking duplicate entry and ignore if so
	for(auto& src: sourcesListV_)
	{
		if(*src == uid)
		{
			json_object_put(sourceJ);
			return 0;
		}
	}

	if(ctlConfig_ && ctlConfig_->requireJ)
	{
		const char* requireS = json_object_to_json_string(ctlConfig_->requireJ);
		if(!strcasestr(requireS, api) && !strcasestr(api, afbBindingRoot->apiname))
			{AFB_WARNING("Caution! You don't specify the API source as required in the metadata section. This API '%s' may not be initialized", api);}
	}

	initCtl = initJ ? convert2Action(apihandle, "init", initJ) : nullptr;

	// Define default action to take to subscibe souce's signals if none
	// defined.
	if(!getSignalsJ)
	{
		getSignalsJ = json_object_new_object();

		std::string uri = "api://" + std::string(api) + "#subscribe";
		json_object_object_add(getSignalsJ, "action", json_object_new_string(uri.c_str()));
	}
	getSignalsCtl = convert2Action(apihandle, "getSignals", getSignalsJ);

	onReceivedCtl = onReceivedJ ? convert2Action(apihandle, "onReceived", onReceivedJ) : nullptr;

	newSourcesListV_.push_back(std::make_shared<SourceAPI>(uid, api, info, initCtl, getSignalsCtl, onReceivedCtl, retention));
	return err;
}

int Composer::loadSourcesAPI(afb_api_t apihandle, CtlSectionT* section, json_object *sourcesJ)
{
	int err = 0;
	Composer& composer = instance();

	if(sourcesJ)
	{
		std::size_t count = 1;
		json_object *sigCompJ = nullptr;

		// add the signal composer itself as source if not already done
		if(! composer.getSourceAPI("Signal-Composer-service"))
		{
			wrap_json_pack(&sigCompJ, "{ss,ss,ss}",
			"uid", "Signal-Composer-service",
			"api", afbBindingRoot->apiname,
			"info", "Api on behalf the virtual signals are sent");

			if(json_object_is_type(sourcesJ, json_type_array))
				json_object_array_add(sourcesJ, sigCompJ);
		}

		if (json_object_get_type(sourcesJ) == json_type_array)
		{
			count = json_object_array_length(sourcesJ);

			for (int idx = 0; idx < count; idx++)
			{
				json_object *sourceJ = json_object_array_get_idx(sourcesJ, idx);
				err = composer.loadOneSourceAPI(apihandle, sourceJ);
				if (err) return err;
			}
		}
		else
		{
			if ( (err = composer.loadOneSourceAPI(apihandle, sourcesJ)) )
				return err;
			if (sigCompJ)
			{
				if ( (err = composer.loadOneSourceAPI(apihandle, sigCompJ)) )
					return err;
			}
		}
		AFB_NOTICE("%ld new sources added to service", count);
	}
	else
		{Composer::instance().initSourcesAPI();}

	return err;
}

int Composer::loadOneSignal(afb_api_t apihandle, json_object* signalJ)
{
	json_object *onReceivedJ = nullptr,
		    *dependsJ = nullptr,
		    *metadataJ = nullptr,
		    *getSignalsArgsJ = nullptr;
	CtlActionT* onReceivedCtl;
	const char *id = nullptr,
			   *event = nullptr,
			   *unit = nullptr,
			   *permission = nullptr;
	int retention = 0;
	double frequency=0.0;
	std::vector<std::string> dependsV;
	ssize_t sep;
	std::shared_ptr<SourceAPI> src = nullptr;

	int err = wrap_json_unpack(signalJ, "{ss,s?s,s?o,s?s,s?o,s?i,s?s,s?o,s?F,s?o !}",
			"uid", &id,
			"event", &event,
			"depends", &dependsJ,
			"permission", &permission,
			"getSignalsArgs", &getSignalsArgsJ,
			"retention", &retention,
			"unit", &unit,
			"metadata", &metadataJ,
			"frequency", &frequency,
			"onReceived", &onReceivedJ);
	if (err)
	{
		AFB_ERROR("Missing something uid|[event|depends]|[getSignalsArgs]|[retention]|[unit]|[frequency]|[onReceived] in %s", json_object_get_string(signalJ));
		return err;
	}

	// event or depends field manadatory
	if( (!event && !dependsJ) || (event && dependsJ) )
	{
		AFB_ERROR("Missing something uid|[event|depends]|[getSignalsArgs]|[retention]|[unit]|[frequency]|[onReceived] in %s. Or you declare event AND depends, only one of them is needed.", json_object_get_string(signalJ));
		return -1;
	}

	// Declare a raw signal
	if(event)
	{
		std::string eventStr = std::string(event);
		if( (sep = eventStr.find("/")) == std::string::npos)
		{
			AFB_ERROR("Missing something in event declaration. Has to be like: <api>/<event>");
			return -1;
		}
		std::string api = eventStr.substr(0, sep);
		src = getSourceAPI(api);
		if(!src)
		{
			AFB_ERROR("Can't find the source API %s for the event %s. Check you configuration.", api.c_str(), eventStr.c_str());
			return -1;
		}
	}
	else
	{
		event = "";
	}
	// Process depends JSON object to declare virtual signal dependencies
	if (dependsJ)
	{
		if(json_object_get_type(dependsJ) == json_type_array)
		{
			std::size_t count = json_object_array_length(dependsJ);
			for(int i = 0; i < count; i++)
			{
				std::string sourceStr = json_object_get_string(json_object_array_get_idx(dependsJ, i));
				if( (sep = sourceStr.find("/")) != std::string::npos)
				{
					AFB_ERROR("Signal composition needs to use signal 'uid', don't use full low level signal name");
					return -1;
				}
				dependsV.push_back(sourceStr);
			}
		}
		else
		{
			std::string sourceStr = json_object_get_string(dependsJ);
			if( (sep = sourceStr.find("/")) != std::string::npos)
			{
				AFB_ERROR("Signal composition needs to use signal 'uid', don't use full low level signal name");
				return -1;
			}
			dependsV.push_back(sourceStr);
		}
		if(!src) {
			if ( ! sourcesListV_.empty() || ! newSourcesListV_.empty())
				src = getSourceAPI("signal-composer");
			else
			{
				AFB_WARNING("Signal '%s' not correctly attached to its source. Abort this one", json_object_to_json_string(signalJ));
				return -1;
			}
		}
	}

	// Set default retention if not specified
	if(!retention)
	{
		retention = src->signalsDefault().retention ?
			src->signalsDefault().retention :
			30;
	}
	unit = !unit ? "" : unit;

	// Set default onReceived action if not specified
	char uid[CONTROL_MAXPATH_LEN] = "onReceived_";
	strncat(uid, id, CONTROL_MAXPATH_LEN - strlen(uid) - 1);

	if(!onReceivedJ)
	{
		onReceivedCtl = src->signalsDefault().onReceived ?
			src->signalsDefault().onReceived :
			nullptr;
			// Overwrite uid to the signal one instead of the default
			if(onReceivedCtl)
				{onReceivedCtl->uid = uid;}
	}
	else {onReceivedCtl = convert2Action(apihandle, uid, onReceivedJ);}

	if(src != nullptr)
		{src->addSignal(id, event, dependsV, retention, unit, metadataJ, frequency, onReceivedCtl, getSignalsArgsJ, permission);}
	else
		{err = -1;}

	return err;
}

int Composer::loadSignals(afb_api_t apihandle, CtlSectionT* section, json_object *signalsJ)
{
	int err = 0;
	Composer& composer = instance();

	if(signalsJ)
	{
		std::size_t count = 1;
		if (json_object_get_type(signalsJ) == json_type_array)
		{
			count = json_object_array_length(signalsJ);
			for (int idx = 0; idx < count; idx++)
			{
				json_object *signalJ = json_object_array_get_idx(signalsJ, idx);
				err += composer.loadOneSignal(apihandle, signalJ);
			}
		}
		else
			{err = composer.loadOneSignal(apihandle, signalsJ);}
		AFB_NOTICE("%ld new signals added to service", count);
	}
	else
		{Composer::instance().initSignals();}

	return err;
}

void Composer::processOptions(const std::map<std::string, int>& opts, std::shared_ptr<Signal> sig, json_object* response) const
{
	json_object *value = nullptr;
	if (opts.at("average"))
	{
		value = sig->average(opts.at("average"));
		json_object_is_type(value, json_type_double) ?
			json_object_object_add(response, "average", value) :
			json_object_object_add(response, "error", value);
	}
	if (opts.at("minimum"))
	{
		value = sig->minimum(opts.at("minimum"));
		json_object_is_type(value, json_type_double) ?
			json_object_object_add(response, "minimum", value) :
			json_object_object_add(response, "error", value);
	}
	if (opts.at("maximum"))
	{
		value = sig->maximum(opts.at("maximum"));
		json_object_is_type(value, json_type_double) ?
			json_object_object_add(response, "maximum", value) :
			json_object_object_add(response, "error", value);
	}
	if (opts.at("last"))
	{
		value = sig->last_value();
		json_object_is_type(value, json_type_null) ?
			json_object_object_add(response, "error", value) :
			json_object_object_add(response, "last", value);
	}
}

///////////////////////////////////////////////////////////////////////////////
//                             PUBLIC METHODS                                //
///////////////////////////////////////////////////////////////////////////////

Composer& Composer::instance()
{
	static Composer* composer;
	if(!composer) composer = new Composer();
	return *composer;
}

void* Composer::createContext(void* ctx)
{
	uuid_t x;
	char cuid[38];
	uuid_generate(x);
	uuid_unparse(x, cuid);
	clientAppCtx* ret = new clientAppCtx(cuid);

	return (void*)ret;
}

void Composer::destroyContext(void* ctx)
{
	delete(reinterpret_cast<clientAppCtx*>(ctx));
}

int Composer::loadConfig(afb_api_t api, std::string& filepath)
{
	const char *dirList= getenv("CONTROL_CONFIG_PATH");
	if (!dirList) dirList=CONTROL_CONFIG_PATH;
	filepath.append(":");
	filepath.append(dirList);
	const char *configPath = CtlConfigSearch(api, filepath.c_str(), "control");

	if (!configPath) {
		AFB_ERROR("CtlPreInit: No control-* config found invalid JSON %s ", filepath.c_str());
		return -1;
	}

	// create one API per file
	ctlConfig_ = CtlLoadMetaData(api, configPath);
	if (!ctlConfig_) {
		AFB_ERROR("CtrlPreInit No valid control config file in:\n-- %s", configPath);
		return -1;
	}

	// Save the config in the api userdata field
	afb_api_set_userdata(api, ctlConfig_);

	int err= CtlLoadSections(api, ctlConfig_, ctlSections_);
	return err;
}

int Composer::loadSources(json_object* sourcesJ)
{
	int err = loadSourcesAPI(nullptr, nullptr, sourcesJ);
	if(err)
	{
		AFB_ERROR("Loading sources failed. JSON: %s", json_object_to_json_string(sourcesJ));
		return err;
	}
	initSourcesAPI();
	return err;
}

int Composer::loadSignals(json_object* signalsJ)
{
	return loadSignals(nullptr, nullptr, signalsJ);
}

CtlConfigT* Composer::ctlConfig()
{
	return ctlConfig_;
}

void Composer::initSourcesAPI()
{
	size_t toInitialize = newSourcesListV_.size();
	for(int i=0; i < toInitialize; i++)
	{
		std::shared_ptr<SourceAPI> src = newSourcesListV_.back();
		newSourcesListV_.pop_back();
		src->init();
		sourcesListV_.push_back(src);
	}
}

int Composer::initSignals(afb_req_t request)
{
	for(int i=0; i < sourcesListV_.size(); i++)
	{
		std::shared_ptr<SourceAPI> src = sourcesListV_[i];
		src->initSignals();
	}
	return execSignalsSubscription(request);
}

std::shared_ptr<SourceAPI> Composer::getSourceAPI(const std::string& api)
{
	for(auto& source: sourcesListV_)
	{
		if (source->api() == api)
			{return source;}
	}
	for(auto& source: newSourcesListV_)
	{
		if (source->api() == api)
			{return source;}
	}
	return nullptr;
}

std::vector<std::shared_ptr<Signal>> Composer::getAllSignals()
{
	std::vector<std::shared_ptr<Signal>> allSignals;
	for( auto& source : sourcesListV_)
	{
		std::vector<std::shared_ptr<Signal>> srcSignals = source->getSignals();
		allSignals.insert(allSignals.end(), srcSignals.begin(), srcSignals.end());
	}

	return allSignals;
}

std::vector<std::shared_ptr<Signal>> Composer::searchSignals(const std::string& aName)
{
	std::string api;
	std::vector<std::shared_ptr<Signal>> signals;
	size_t sep = aName.find_first_of("/");
	if(sep != std::string::npos)
	{
		api = aName.substr(0, sep);
		std::string signal_id = aName.substr(sep + 1, std::string::npos);
		std::shared_ptr<SourceAPI> source = getSourceAPI(api);
		return source->searchSignals(signal_id);
	}
	else
	{
		std::vector<std::shared_ptr<Signal>> allSignals = getAllSignals();
		for (std::shared_ptr<Signal>& sig : allSignals)
		{
			if(*sig == aName)
				{signals.emplace_back(sig);}
		}
	}
	return signals;
}

json_object* Composer::getSignalValue(std::shared_ptr<Signal> sig, json_object* options)
{
	std::map<std::string, int> opts;
	json_object *response = nullptr, *finalResponse = json_object_new_array();

	if(options && wrap_json_unpack(options, "{s?i, s?i, s?i, s?i !}",
		"average", &opts["average"],
		"minimum", &opts["minimum"],
		"maximum", &opts["maximum"],
		"last", &opts["last"]))
	{
		response = json_object_new_object();
		json_object_object_add(response, "error", json_object_new_string("Invalid options specified. Use the following: 'options': {['average': nb_seconds,] ['minimum': nb_seconds,] ['maximum': nb_seconds] }"));
		json_object_array_add(finalResponse, response);
		return finalResponse;
	}

	wrap_json_pack(&response, "{ss}",
			"signal", sig->id().c_str());
	if (opts.empty())
	{
		json_object* value = sig->last_value();
		json_object_object_add(response, "value", value);
	}
	else
		{processOptions(opts, sig, response);}

	json_object_array_add(finalResponse, response);

	return finalResponse;
}

json_object *getVerbSample()
{
	int err;
	json_object *sample;
	json_object *tmp;
	json_object *arg;

	sample = json_object_new_array();
	err = wrap_json_pack(&arg, "{s:i}",
		"average", 1);
	if(err)
		goto FAILURE;
	err = wrap_json_pack(&tmp, "{s:o}",
		"get", arg);
	if(err)
		goto FAILURE;
	json_object_array_add(sample, tmp);
	json_object_array_add(sample, json_object_new_string("config"));
	err = wrap_json_pack(&arg, "{s:i}",
		"retention", 1);
	if(err)
		goto FAILURE;
	err = wrap_json_pack(&tmp, "{s:o}",
		"change", arg);
	if(err)
		goto FAILURE;
	json_object_array_add(sample, tmp);
	json_object_array_add(sample, json_object_new_string("subscribe"));
	json_object_array_add(sample, json_object_new_string("unsubscribe"));
	return sample;
	FAILURE:
	return NULL;
}

void Composer::setInfo(json_object *verbInfo)
{
	int err;
	json_object *metadata;
	json_object *groups;
	json_object *verbs;
	json_object *tmp;

	err = wrap_json_pack(&metadata, "{s:s s:s* s:s*}",
		"uid", ctlConfig_->uid,
		"info", ctlConfig_->info,
		"version", ctlConfig_->version);
	if(err)
		goto FAILURE;
	groups = json_object_new_array();
	for(std::shared_ptr<SourceAPI> src: sourcesListV_)
	{
		verbs = json_object_new_array();
		if(src->api() != std::string(ctlConfig_->api))
		{
			for(std::shared_ptr<Signal> sig: src->getSignals())
			{
				err = wrap_json_pack(&tmp, "{s:s s:s s:o*}",
				"uid", sig->id().c_str(),
				"verb", sig->id().c_str(),
				"sample", getVerbSample());
				if(err)
					goto FAILURE;
				json_object_array_add(verbs, tmp);
			}
			err = wrap_json_pack(&tmp, "{s:s s:s* s:s* s:o*}",
			"uid", src->uid().c_str(),
			"api", src->api().c_str(),
			"info", src->info().c_str(),
			"verbs", verbs);
			if(err)		
				goto FAILURE;		
		}
		else
		{
			err = wrap_json_pack(&tmp, "{s:s s:s* s:s* s:o*}",
			"uid", src->uid().c_str(),
			"api", src->api().c_str(),
			"info", src->info().c_str(),
			"verbs", verbInfo);
			if(err)		
				goto FAILURE;			
		}
		json_object_array_add(groups, tmp);	
	}
	err = wrap_json_pack(&info_, "{s:o s:o}",
			"metadata", metadata,
			"groups", groups);
	return;
	FAILURE:
	info_ = NULL;
}

json_object* Composer::getInfo()
{
	return json_object_get(info_);
}

int Composer::execSignalsSubscription(afb_req_t request)
{
	int err = 0;;
	for(std::shared_ptr<SourceAPI> srcAPI: sourcesListV_)
	{
		if (srcAPI->api() != std::string(ctlConfig_->api))
		{
			err += srcAPI->makeSubscription(request);
		}
	}

	return err;
}
