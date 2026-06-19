// secrets.cpp - SD-only variant
// This file is ignored by .gitignore. Do NOT commit it to source control.

#include "secrets.h"

// GLOBE Air Temperature metadata fields
const char* GLOBE_ORGANIZATION_ID = "";
const char* GLOBE_ORG_NAME = "";
const char* GLOBE_SITE_ID = "";
const char* GLOBE_SITE_NAME = "";
const char* GLOBE_ELEVATION_M = "";
const char* GLOBE_USER_ID = "";
const char* GLOBE_TEAMS = "";

// Deep sleep defaults
const bool ENABLE_DEEP_SLEEP = false;          // set true on battery devices
const unsigned long DEEP_SLEEP_SECONDS = 60;   // seconds to sleep between wakes
