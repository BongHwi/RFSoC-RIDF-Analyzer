#pragma once

#ifdef __GNUC__
#include_next <nlohmann/json.hpp>
#else
#include "/usr/include/nlohmann/json.hpp"
#endif
