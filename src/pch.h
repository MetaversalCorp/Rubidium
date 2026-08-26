// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_PCH_H
#define RUBIDIUM_PCH_H

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <sstream>

#include <nlohmann/json.hpp>

#include <SDL3/SDL.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/ElementUtilities.h>

#include <Sneeze.h>

#include "logger/Logger.h"
#include "shell/App.h"
#ifndef RUBIDIUM_PLATFORM_WINDOWS
#include "shell/App_SDL.h"
#endif
#include "canvas/Canvas.h"
#include "Native.h"
#include "Utils.h"

#ifdef _WIN32
#undef GetNextSibling    // Needed for RmlUi/Core.h
#undef GetFirstChild     // Needed for RmlUi/Core.h
#endif

#endif // RUBIDIUM_PCH_H
