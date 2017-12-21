#pragma once

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding>

void onEvent(const char *event, struct json_object *object);
int loadConf();
int execConf();
