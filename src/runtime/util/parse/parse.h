#pragma once

#include <runtime/util/chrono/chrono.h>
#include <runtime/util/endpoint/endpoint.h>

#include <sstream>

std::istringstream& operator>>(std::istringstream& in, mtf::rt::Endpoint& endpoint);
std::istringstream& operator>>(std::istringstream& in, mtf::rt::Duration& duration);
std::istringstream& operator>>(std::istringstream& in, mtf::rt::Interval& interval);
