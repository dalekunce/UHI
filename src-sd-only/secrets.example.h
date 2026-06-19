// secrets.example.h - SD-only variant
// Copy this to src-sd-only/secrets.h and fill in your values.
// Do NOT commit your real secrets.

#ifndef _SECRETS_EXAMPLE_H_
#define _SECRETS_EXAMPLE_H_

// GLOBE Air Temperature metadata fields
extern const char* GLOBE_ORGANIZATION_ID;
extern const char* GLOBE_ORG_NAME;
extern const char* GLOBE_SITE_ID;
extern const char* GLOBE_SITE_NAME;
extern const char* GLOBE_ELEVATION_M;
extern const char* GLOBE_USER_ID;
extern const char* GLOBE_TEAMS;

// Deep sleep settings
extern const bool ENABLE_DEEP_SLEEP;          // true to enable sleeping between records
extern const unsigned long DEEP_SLEEP_SECONDS; // sleep duration in seconds

#endif // _SECRETS_EXAMPLE_H_
