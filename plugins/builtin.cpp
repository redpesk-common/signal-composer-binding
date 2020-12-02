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
CTLP_LUA_REGISTER("builtin");

CTLP_LUA2C (setSignalValueWrap, source, argsJ, responseJ)
{
	const char* name = nullptr;
	uint64_t timestamp;
	json_object* resultNumJ;
	struct signalCBT* ctx = (struct signalCBT*)source->context;
	Signal *sig = (Signal*)ctx->aSignal;

	if(! wrap_json_unpack(argsJ, "{ss, so, sI? !}",
		"name", &name,
		"value", &resultNumJ,
		"timestamp", &timestamp))
	{
		return -1;
	}

	if(ctx->aSignal)
		 sig->set(timestamp*NANO, resultNumJ);
	else
		return -1;

	return 0;
}

// extern "C" closure
}

