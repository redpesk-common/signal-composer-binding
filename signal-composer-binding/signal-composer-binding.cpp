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

#include "signal-composer-binding-apidef.h"
#include "clientApp.hpp"

/// @brief verb that loads JSON configuration to add sources and/or signals
/// to the running signal composer binding.
///
/// @param[in] request - AFB request pointer
void addObjects(afb_req_t request)
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
void list(afb_req_t request)
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

int loadConf(afb_api_t api)
{
	int err = 0;
	std::string bindingDirPath = GetBindingDirPath(api);
	std::string rootdir = bindingDirPath + "/etc";

	err = Composer::instance().loadConfig(api, rootdir);

	return err;
}

int execConf(afb_api_t api)
{
	Composer& composer = Composer::instance();
	int err = 0;
	CtlConfigExec(api, composer.ctlConfig());

	AFB_DEBUG("Signal Composer Control configuration Done.");

	return err;
}
