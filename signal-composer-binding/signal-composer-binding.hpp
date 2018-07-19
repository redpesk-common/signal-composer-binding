#pragma once

#include <afb/afb-binding>

void onEvent(const char *event, struct json_object *object);
int loadConf();
int execConf();
