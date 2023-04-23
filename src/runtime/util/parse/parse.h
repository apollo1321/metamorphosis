#pragma once

#include <runtime/util/chrono/chrono.h>
#include <runtime/util/endpoint/endpoint.h>

#include <sstream>

std::istringstream& operator>>(std::istringstream& in, ceq::rt::Endpoint& endpoint);
std::istringstream& operator>>(std::istringstream& in, ceq::rt::Duration& duration);
std::istringstream& operator>>(std::istringstream& in, ceq::rt::Interval& interval);
