
#pragma once

#include "platform/windows.h"

// For some reason spdlog depends on format.h but doesn't include it itself
#include <fmt/format.h>
#include "spdlog/logger.h"

// Global TemplePlus logger
extern std::shared_ptr<spdlog::logger> logger;

void InitLogging(const std::wstring &logFile);
