#pragma once
#include <string>
#include <stddef.h>
struct CoreResult { int code = 0; std::string json, logs; };
CoreResult RunCore(int argc, const wchar_t* const* argv, const void* memory = nullptr, size_t length = 0);
