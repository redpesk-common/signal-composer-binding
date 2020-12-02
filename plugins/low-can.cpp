/*
 * Copyright (C) 2016 "IoT.bzh"
 * Author Romain Forlot <romain.forlot@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#define CTL_PLUGIN_MAGIC 1286576532
#include <afb/afb-binding.h>
#include <systemd/sd-event.h>
#include <json-c/json_object.h>
#include <stdbool.h>
#include <string.h>

#include "signal-composer.hpp"
#include "wrap-json.h"

extern "C"
{
CTLP_CAPI_REGISTER("low-can");

typedef struct {
	json_object* door;
	json_object* window;
} doorT;

typedef struct {
	doorT front_left;
	doorT front_right;
	doorT rear_left;
	doorT rear_right;
} allDoorsCtxT;

struct pluginCtxT {
	json_object *subscriptionBatch;
	allDoorsCtxT allDoorsCtx;
};

void setDoor(doorT* aDoor, const char* eventName, json_object* eventStatus)
{
	if(json_object_is_type(eventStatus, json_type_boolean)) {
		if(strcasestr(eventName, "door")) aDoor->door = eventStatus;
		else if(strcasestr(eventName, "window")) aDoor->window = eventStatus;
	}
}

// Call at initialisation time
CTLP_ONLOAD(plugin, composerHandle)
{
	struct pluginCtxT* pluginCtx = new struct pluginCtxT;
	pluginCtx->subscriptionBatch = json_object_new_array();
	memset(&pluginCtx->allDoorsCtx, 0, sizeof(allDoorsCtxT));

	struct signalCBT* handle = (struct signalCBT*)composerHandle;
	handle->pluginCtx = (void*)pluginCtx;

	plugin->context = (void*)handle;

	return 0;
}

CTLP_CAPI (subscribeToLow, source, argsJ, eventJ) {
	json_object* metadataJ = nullptr, *dependsArrayJ = nullptr, *subscribeArgsJ = nullptr, *subscribeFilterJ = nullptr, *responseJ = nullptr, *filterJ = nullptr;
	const char *id = nullptr, *event = nullptr, *unit = nullptr;
	double frequency = 0;
	int err = 0;

	struct signalCBT* context = reinterpret_cast<struct signalCBT*>(source->context);
	struct pluginCtxT* pluginCtx = reinterpret_cast<struct pluginCtxT*>(context->pluginCtx);

	if(eventJ)
	{
		err = wrap_json_unpack(eventJ, "{ss,s?s,s?o,s?s,s?o,s?F,s?o !}",
			"uid", &id,
			"event", &event,
			"depends", &dependsArrayJ,
			"unit", &unit,
			"metadata", &metadataJ,
			"frequency", &frequency,
			"getSignalsArgs", &filterJ);
		if(err)
		{
			AFB_API_ERROR(source->api, "Problem to unpack JSON object eventJ: %s",
			json_object_to_json_string(eventJ));
			return err;
		}

		if(frequency > 0 && !filterJ)
			{wrap_json_pack(&subscribeFilterJ, "{sf}", "frequency", frequency);}
		else
		{
			json_object_get(filterJ);
			subscribeFilterJ = filterJ;
		}

		std::string eventStr = std::string(event);
		std::string lowEvent = eventStr.substr(eventStr.find("/")+1);
		err = wrap_json_pack(&subscribeArgsJ, "{ss, so*}",
		"event", lowEvent.c_str(),
		"filter", subscribeFilterJ);
		if(err)
		{
			AFB_API_ERROR(source->api, "Error building subscription query object");
			return err;
		}
		json_object_array_add(pluginCtx->subscriptionBatch, subscribeArgsJ);
	}
	else
	{
		json_object_get(pluginCtx->subscriptionBatch);
		AFB_API_DEBUG(source->api, "Calling subscribe with %s", json_object_to_json_string(pluginCtx->subscriptionBatch));
		err = afb_api_call_sync_legacy(source->api, "low-can", "subscribe", pluginCtx->subscriptionBatch, &responseJ);
		if(err)
			{AFB_API_ERROR(source->api, "Subscribe to '%s' responseJ:%s", json_object_to_json_string(pluginCtx->subscriptionBatch), json_object_to_json_string(responseJ));}

		// Renew subscription json object for the next time we need it
		json_object_put(pluginCtx->subscriptionBatch);
		pluginCtx->subscriptionBatch = json_object_new_array();
	}

	return err;
}

CTLP_CAPI (isOpen, source, argsJ, eventJ) {
	const char *eventName = nullptr;
	json_object *eventStatus;
	uint64_t timestamp;
	struct signalCBT* context = reinterpret_cast<struct signalCBT*>(source->context);
	struct pluginCtxT* pluginCtx = reinterpret_cast<struct pluginCtxT*>(context->pluginCtx);

	int err = wrap_json_unpack(eventJ, "{ss,so,s?I}",
		"name", &eventName,
		"value", &eventStatus,
		"timestamp", &timestamp);
	if(err)
	{
		AFB_API_ERROR(source->api, "Error parsing event %s", json_object_to_json_string(eventJ));
		return -1;
	}

	if(strcasestr(eventName, "front_left"))
	{
		setDoor(&pluginCtx->allDoorsCtx.front_left, eventName, eventStatus);
	}
	else if(strcasestr(eventName, "front_right"))
	{
		setDoor(&pluginCtx->allDoorsCtx.front_right, eventName, eventStatus);
	}
	else if(strcasestr(eventName, "rear_left"))
	{
		setDoor(&pluginCtx->allDoorsCtx.rear_left, eventName, eventStatus);
	}
	else if(strcasestr(eventName, "rear_right"))
	{
		setDoor(&pluginCtx->allDoorsCtx.rear_right, eventName, eventStatus);
	}
	else
	{
		AFB_API_WARNING(source->api, "Unexpected behavior, this '%s' is it really a door ! ", json_object_to_json_string(eventJ));
		return -1;
	}

	AFB_API_DEBUG(source->api, "This is the situation: source:%s, args:%s, event:%s,\n fld: %s, flw: %s, frd: %s, frw: %s, rld: %s, rlw: %s, rrd: %s, rrw: %s",
	source->uid,
	argsJ ? json_object_to_json_string(argsJ):"",
	eventJ ? json_object_to_json_string(eventJ):"",
	json_object_get_string(pluginCtx->allDoorsCtx.front_left.door),
	json_object_get_string(pluginCtx->allDoorsCtx.front_left.window),
	json_object_get_string(pluginCtx->allDoorsCtx.front_right.door),
	json_object_get_string(pluginCtx->allDoorsCtx.front_right.window),
	json_object_get_string(pluginCtx->allDoorsCtx.rear_left.door),
	json_object_get_string(pluginCtx->allDoorsCtx.rear_left.window),
	json_object_get_string(pluginCtx->allDoorsCtx.rear_right.door),
	json_object_get_string(pluginCtx->allDoorsCtx.rear_right.window)
);

	return 0;
}

}
