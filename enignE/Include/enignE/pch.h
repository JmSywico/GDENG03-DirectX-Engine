#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <array>
#include <chrono>
#include <limits>
#include <cstring>
#include <typeinfo>
#include <typeindex>
#include <algorithm>
#include <cmath>
#include <windowsx.h>
#include <sal.h>

#include "Events/EventManager.h"
#include "Events/Event.h"
#include "Logging/Logging.h"
#include "Layer/Layer.h"
#include "Layer/DebugLayer.h"
#include "Input/Input.h"
