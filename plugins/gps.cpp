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
#include <math.h>

#include "signal-composer.hpp"

extern "C"
{
CTLP_CAPI_REGISTER("gps");

CTLP_CAPI (getHeading, source, argsJ, eventJ) {

	static bool coordUpdated = true;
	static double prvLat = 0, prvLon = 0, curLat = 0, curLon = 0, heading = 0;
	double r2d =  180.0 / M_PI;
	double d2r = M_PI / 180.0;
	int err = 0;

	struct signalCBT* ctx = (struct signalCBT*)source->context;
	json_object *val = NULL, *lastSignal = NULL, *id = NULL;

	lastSignal = json_object_array_get_idx(eventJ, json_object_array_length(eventJ) -1);

	if(json_object_object_get_ex(lastSignal, "uid", &id) &&
	   std::string(json_object_get_string(id)) == "latitude" &&
	   json_object_object_get_ex(lastSignal, "value", &val)) {
			prvLat = curLat;
			curLat = json_object_get_double(val);
			coordUpdated = !coordUpdated;
	}

	if(json_object_object_get_ex(lastSignal, "uid", &id) &&
	   std::string(json_object_get_string(id)) == "longitude" &&
	   json_object_object_get_ex(lastSignal, "value", &val)) {
		prvLon = curLon;
		curLon = json_object_get_double(val);
		coordUpdated = !coordUpdated;
	}

	if(coordUpdated) {
		heading = round(r2d * atan2((curLon - prvLon) * cos(d2r * curLat), curLat - prvLat));
		ctx->setSignalValue(ctx->aSignal, 0, json_object_new_double(heading));
	}

	AFB_API_NOTICE(source->api, "======== Heading: %f", heading);
	return err;
}
// extern "C" closure
}
