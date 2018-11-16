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

#include <string>
#include <string.h>
// afb-utilities
#include <wrap-json.h>
#include <filescan-utils.h>
#include <afb/afb-binding>

#include "signal-composer-apidef.h"
#include "clientApp.hpp"

/// @brief callback for receiving message from low bindings. This will callback
/// an action defined in the configuration files depending on the event received
///
/// @param[in] event - event name
/// @param[in] object - eventual data that comes with the event
void onEvent(const char *event, json_object *object)
{
	std::vector<std::shared_ptr<Signal>> signals { Composer::instance().searchSignals(event) };

	if(!signals.empty())
	{
		// If there is more than 1 element then maybe we can find a more
		// detailled event name in JSON object as 1 event may carry several
		// signals. Try to find that one.
		if(signals.size() > 1)
		{
			bool found = false;
			json_object_iterator iter = json_object_iter_begin(object);
			json_object_iterator iterEnd = json_object_iter_end(object);
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
							sig->onReceivedCB(object);
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
					{sig->onReceivedCB(object);}
			}
		}
		else
			{signals[0]->onReceivedCB(object);}
	}
}

static int one_subscribe_unsubscribe(AFB_ReqT request,
	bool subscribe,
	const std::string& event,
	json_object* args,
	clientAppCtx* cContext)
{
	int err = 0;
	std::vector<std::shared_ptr<Signal>> signals = Composer::instance().searchSignals(event);

	if(signals.size() == 0 && subscribe)
	{
		err--;
	}
	else
	{
		if(subscribe)
		{
			cContext->appendSignals(signals);
			err = cContext->makeSubscription(request);
		}
		else
		{
			cContext->subtractSignals(signals);
			err = cContext->makeUnsubscription(request);
		}
	}

	return err;
}

static int subscribe_unsubscribe(AFB_ReqT request,
	bool subscribe,
	json_object* args,
	clientAppCtx* cContext)
{
	int rc = 0;
	json_object *eventJ = nullptr;
	if (args == NULL || !json_object_object_get_ex(args, "signal", &eventJ))
	{
		rc = one_subscribe_unsubscribe(request, subscribe, "*", args, cContext);
	}
	else if (json_object_get_type(eventJ) == json_type_string)
	{
		rc = one_subscribe_unsubscribe(request, subscribe, json_object_get_string(eventJ), args, cContext);
	}
	else if (json_object_get_type(eventJ) == json_type_array)
	{
		for (int i = 0 ; i < json_object_array_length(eventJ) ; i++)
		{
			json_object *x = json_object_array_get_idx(eventJ, i);
			rc += one_subscribe_unsubscribe(request, subscribe, json_object_get_string(x), args, cContext);
		}
	}
	else {rc = -1;}

	return rc;
}

/// @brief entry point for client subscription request.
static void do_subscribe_unsubscribe(AFB_ReqT request, bool subscribe, clientAppCtx* cContext)
{
	int rc = 0;
	json_object *oneArg = nullptr, *args = afb_req_json(request);
	if (json_object_get_type(args) == json_type_array)
	{
		for (int i = 0 ; i < json_object_array_length(args); i++)
		{
			oneArg = json_object_array_get_idx(args, i);
			rc += subscribe_unsubscribe(request, subscribe, oneArg, cContext);
		}
	}
	else
	{
		rc = subscribe_unsubscribe(request, subscribe, args, cContext);
	}

	if(rc >= 0)
		afb_req_success(request, NULL, NULL);
	else
		afb_req_fail(request, "error", NULL);
}

/// @brief entry point for client un-subscription request.
void subscribe(AFB_ReqT request)
{
	clientAppCtx *cContext = reinterpret_cast<clientAppCtx*>(afb_req_context(request, 0, Composer::createContext, Composer::destroyContext, nullptr));

	do_subscribe_unsubscribe(request, true, cContext);
}

/// @brief entry point for client un-subscription request.
void unsubscribe(AFB_ReqT request)
{
	clientAppCtx *cContext = reinterpret_cast<clientAppCtx*>(afb_req_context(request, 0, Composer::createContext, Composer::destroyContext, nullptr));

	do_subscribe_unsubscribe(request, false, cContext);
}

/// @brief verb that loads JSON configuration (old SigComp.json file now)

void addObjects(AFB_ReqT request)
{
	Composer& composer = Composer::instance();
	json_object *sourcesJ = nullptr,
				*signalsJ = nullptr,
				*objectsJ = nullptr;
	const char* filepath = afb_req_value(request, "file");

	if(filepath)
	{
		objectsJ = json_object_from_file(filepath);
		if(! objectsJ)
		{
			const char *has_slash = strrchr(filepath, '/');
			char *filename = has_slash ? strdupa(has_slash + 1) : strdupa(filepath);
			char *filename_end = strrchr(filename, '.');
			if (filename_end)
				{*filename_end = '\0';}

			json_object* responseJ = ScanForConfig(CONTROL_CONFIG_PATH, CTL_SCAN_RECURSIVE, filename, ".json");

			if(responseJ)
			{
				filepath = ConfigSearch(nullptr, responseJ);
				if(filepath)
					{objectsJ = json_object_from_file(filepath);}
			}
			else
			{
				afb_req_fail_f(request, "Fail to find file: %s", filepath);
				return;
			}
		}
	}
	else
		{objectsJ = afb_req_json(request);}

	if(objectsJ)
	{
		json_object_object_get_ex(objectsJ, "sources", &sourcesJ);
		json_object_object_get_ex(objectsJ, "signals", &signalsJ);

		if(sourcesJ && composer.loadSources(sourcesJ))
		{
			afb_req_fail_f(request, "Loading 'sources' configuration or subscription error", "Error code: -4");
			return;
		}
		if(signalsJ)
		{
			if(!composer.loadSignals(signalsJ))
			{
				if(composer.initSignals())
				{
					afb_req_fail_f(request, "Loading 'signals' configuration or subscription error", "Error code: -3");
					return;
				}
			}
			else
			{
				afb_req_fail_f(request, "Loading 'signals' configuration or subscription error", "Error code: -2");
				return;
			}
		}
		afb_req_success(request, NULL, NULL);
	}
	else
		{afb_req_fail_f(request, "No 'file' key found in request argument", "Error code: -1");}
}

/// @brief entry point to list available signals
void list(AFB_ReqT request)
{
	struct json_object *allSignalsJ = json_object_new_array();

	std::vector<std::shared_ptr<Signal>> allSignals = Composer::instance().getAllSignals();
	for(auto& sig: allSignals)
		{json_object_array_add(allSignalsJ, sig->toJSON());}

	if(json_object_array_length(allSignalsJ))
		{afb_req_success(request, allSignalsJ, NULL);}
	else
		{afb_req_fail(request, "error", "No Signals recorded so far");}
}

/// @brief entry point for get requests.
void get(AFB_ReqT request)
{
	int err = 0, i = 0;
	size_t l = 0;
	struct json_object* args = afb_req_json(request), *ans = nullptr,
	*options = nullptr, *error = nullptr, *object = nullptr;
	const char* sig;

	// Process about Raw CAN message on CAN bus directly
	err = wrap_json_unpack(args, "{ss,s?o!}", "signal", &sig,
			"options", &options);
	if(err)
	{
		afb_req_fail(request, "error", "Valid arguments are:{'signal': 'wantedsignal'[, 'options': {['average': nb_seconds,] ['minimum': nb_seconds,] ['maximum': nb_seconds] }]");
		return;
	}

	ans = Composer::instance().getsignalValue(sig, options);

	l = json_object_array_length(ans);
	if(l) {
		while(i < l) {
			object = json_object_array_get_idx(ans, i++);
			if(json_object_object_get_ex(object, "error", &error))
				break;
		}
		if(error)
			afb_req_fail(request, "error", json_object_get_string(error));
		else
			afb_req_success(request, ans, NULL);
	}
	else
		afb_req_fail(request, "error", "No signals found.");

}

int loadConf(AFB_ApiT api)
{
	int err = 0;
	std::string bindingDirPath = GetBindingDirPath();
	std::string rootdir = bindingDirPath + "/etc";

	err = Composer::instance().loadConfig(api, rootdir);

	return err;
}

int execConf(AFB_ApiT api)
{
	Composer& composer = Composer::instance();
	int err = 0;
	CtlConfigExec(api, composer.ctlConfig());

	AFB_DEBUG("Signal Composer Control configuration Done.");

	return err;
}
