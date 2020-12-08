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
/*#################
## EXAMPLE INCLUDES
#################*/
#include <wrap-json.h>

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>
#include <ctl-plugin.h>
#include <ctl-config.h>
#include <signal-composer.hpp>

/*#################
## PLUGIN VARIABLES
#################*/
#define API_NAME "example"

extern "C"
{
    CTLP_CAPI_REGISTER(API_NAME);
    CTLP_CAPI(example, source, argsJ, eventJ)
    {
        struct signalCBT* ctx;
        Signal* sig;
        ctx = (struct signalCBT*)source->context;
        sig = (Signal*) ctx->aSignal;
        sig->set(0, json_object_new_int(42))
        return 0;
    }
}
