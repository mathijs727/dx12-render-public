#pragma once

namespace Core {

template <size_t N, typename T>
struct Bounds;
using Bounds2u = Bounds<2, unsigned>;
using Bounds2f = Bounds<2, float>;
using Bounds3f = Bounds<3, float>;
struct BoundingSphere;

class Keyboard;
class Mouse;
class Window;

struct ProfileTask;
template <typename T>
class Singleton;
class Stopwatch;
struct Transform;
struct FPSCameraControls;

}
