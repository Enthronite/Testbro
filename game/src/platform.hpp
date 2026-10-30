#pragma once

#if !defined(_WIN32)
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

struct Vector3 {
  float x;
  float y;
  float z;
};

struct Color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

struct Rectangle {
  float x;
  float y;
  float width;
  float height;
};

struct Camera3D {
  Vector3 position;
  Vector3 target;
  Vector3 up;
  float fovy;
  int projection;
};

struct Texture2D {
  SDL_Texture* texture;
  int width;
  int height;
};

struct Image {
  SDL_Surface* surface;
};

constexpr Color WHITE{255, 255, 255, 255};
constexpr Color RAYWHITE{245, 245, 245, 255};
constexpr Color BLANK{0, 0, 0, 0};
constexpr int FLAG_WINDOW_RESIZABLE = 1;
constexpr int FLAG_VSYNC_HINT = 2;
constexpr int CAMERA_PERSPECTIVE = 0;
constexpr int TEXTURE_FILTER_POINT = 0;

constexpr int KEY_A = SDL_SCANCODE_A;
constexpr int KEY_D = SDL_SCANCODE_D;
constexpr int KEY_E = SDL_SCANCODE_E;
constexpr int KEY_ENTER = SDL_SCANCODE_RETURN;
constexpr int KEY_ESCAPE = SDL_SCANCODE_ESCAPE;
constexpr int KEY_F11 = SDL_SCANCODE_F11;
constexpr int KEY_J = SDL_SCANCODE_J;
constexpr int KEY_K = SDL_SCANCODE_K;
constexpr int KEY_L = SDL_SCANCODE_L;
constexpr int KEY_LEFT = SDL_SCANCODE_LEFT;
constexpr int KEY_RIGHT = SDL_SCANCODE_RIGHT;
constexpr int KEY_S = SDL_SCANCODE_S;
constexpr int KEY_SPACE = SDL_SCANCODE_SPACE;
constexpr int KEY_U = SDL_SCANCODE_U;
constexpr int KEY_UP = SDL_SCANCODE_UP;
constexpr int KEY_W = SDL_SCANCODE_W;
constexpr int KEY_DOWN = SDL_SCANCODE_DOWN;
constexpr int KEY_NULL = 0;

namespace project_dino_platform {

struct Point3 {
  float x;
  float y;
  float z;
  float depth;
};

enum class PrimitiveKind {
  Cube,
  WireCube,
  Billboard,
  Ring
};

struct Primitive {
  PrimitiveKind kind;
  Vector3 position;
  Vector3 size;
  float radius;
  Color tint;
  Texture2D texture;
  Rectangle source;
  float depth;
};

inline SDL_Window* window = nullptr;
inline SDL_Renderer* renderer = nullptr;
inline Camera3D camera{};
inline bool in3d = false;
inline bool closeRequested = false;
inline bool fullscreen = false;
inline int targetFps = 60;
inline float frameTime = 1.0f / 60.0f;
inline std::chrono::steady_clock::time_point appStart;
inline std::chrono::steady_clock::time_point lastFrame;
inline std::vector<Primitive> primitives;
inline std::vector<bool> pressedKeys(SDL_NUM_SCANCODES, false);

inline Vector3 subtract(Vector3 a, Vector3 b) {
  return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline float dot(Vector3 a, Vector3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 cross(Vector3 a, Vector3 b) {
  return Vector3{a.y * b.z - a.z * b.y,
                 a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x};
}

inline Vector3 normalize(Vector3 value) {
  const float length = std::sqrt(dot(value, value));
  if (length < 0.0001f) {
    return Vector3{0.0f, 0.0f, 0.0f};
  }
  return Vector3{value.x / length, value.y / length, value.z / length};
}

inline Vector3 scale(Vector3 value, float amount) {
  return Vector3{value.x * amount, value.y * amount, value.z * amount};
}

inline Vector3 plus(Vector3 a, Vector3 b) {
  return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3 cameraForward(const Camera3D& source) {
  return normalize(subtract(source.target, source.position));
}

inline Vector3 cameraRight(const Camera3D& source) {
  return normalize(cross(cameraForward(source), source.up));
}

inline Vector3 cameraUp(const Camera3D& source) {
  return normalize(cross(cameraRight(source), cameraForward(source)));
}

inline Point3 project(Vector3 world) {
  const Vector3 forward = cameraForward(camera);
  const Vector3 right = cameraRight(camera);
  const Vector3 up = cameraUp(camera);
  const Vector3 relative = subtract(world, camera.position);
  const float depth = dot(relative, forward);
  if (depth <= 0.12f) {
    return Point3{0.0f, 0.0f, depth, depth};
  }
  int width = 0;
  int height = 0;
  SDL_GetRendererOutputSize(renderer, &width, &height);
  const float focal = (static_cast<float>(height) * 0.5f) /
                      std::tan(camera.fovy * 0.008726646259971648f);
  return Point3{
      static_cast<float>(width) * 0.5f + dot(relative, right) * focal / depth,
      static_cast<float>(height) * 0.5f - dot(relative, up) * focal / depth,
      depth,
      depth,
  };
}

inline float primitiveDepth(Vector3 position) {
  return dot(subtract(position, camera.position), cameraForward(camera));
}

inline Color shade(Color source, float multiplier) {
  return Color{
      static_cast<unsigned char>(std::clamp(source.r * multiplier, 0.0f, 255.0f)),
      static_cast<unsigned char>(std::clamp(source.g * multiplier, 0.0f, 255.0f)),
      static_cast<unsigned char>(std::clamp(source.b * multiplier, 0.0f, 255.0f)),
      source.a,
  };
}

inline SDL_Vertex vertex(Point3 point, Color tint, SDL_FPoint uv = SDL_FPoint{0, 0}) {
  return SDL_Vertex{SDL_FPoint{point.x, point.y},
                    SDL_Color{tint.r, tint.g, tint.b, tint.a}, uv};
}

inline void drawTriangleQuad(const std::array<Point3, 4>& points, Color tint) {
  SDL_Vertex vertices[4] = {
      vertex(points[0], tint), vertex(points[1], tint),
      vertex(points[2], tint), vertex(points[3], tint)};
  const int indices[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6);
}

inline void drawCubePrimitive(const Primitive& primitive) {
  const float hx = primitive.size.x * 0.5f;
  const float hy = primitive.size.y * 0.5f;
  const float hz = primitive.size.z * 0.5f;
  const std::array<Vector3, 8> world = {{
      {primitive.position.x - hx, primitive.position.y - hy, primitive.position.z - hz},
      {primitive.position.x + hx, primitive.position.y - hy, primitive.position.z - hz},
      {primitive.position.x + hx, primitive.position.y + hy, primitive.position.z - hz},
      {primitive.position.x - hx, primitive.position.y + hy, primitive.position.z - hz},
      {primitive.position.x - hx, primitive.position.y - hy, primitive.position.z + hz},
      {primitive.position.x + hx, primitive.position.y - hy, primitive.position.z + hz},
      {primitive.position.x + hx, primitive.position.y + hy, primitive.position.z + hz},
      {primitive.position.x - hx, primitive.position.y + hy, primitive.position.z + hz},
  }};
  std::array<Point3, 8> projected{};
  for (size_t i = 0; i < world.size(); ++i) {
    projected[i] = project(world[i]);
  }

  constexpr std::array<std::array<int, 4>, 6> faces = {{
      {{0, 1, 2, 3}}, {{4, 7, 6, 5}}, {{0, 3, 7, 4}},
      {{1, 5, 6, 2}}, {{3, 2, 6, 7}}, {{0, 4, 5, 1}},
  }};
  constexpr std::array<float, 6> brightness = {0.95f, 0.7f, 0.82f,
                                                0.76f, 1.14f, 0.57f};

  if (primitive.kind == PrimitiveKind::WireCube) {
    SDL_SetRenderDrawColor(renderer, primitive.tint.r, primitive.tint.g,
                           primitive.tint.b, primitive.tint.a);
    constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) {
      if (projected[edge[0]].depth > 0.12f && projected[edge[1]].depth > 0.12f) {
        SDL_RenderDrawLineF(renderer, projected[edge[0]].x, projected[edge[0]].y,
                            projected[edge[1]].x, projected[edge[1]].y);
      }
    }
    return;
  }

  struct FaceOrder {
    int index;
    float depth;
  };
  std::array<FaceOrder, 6> order{};
  for (int i = 0; i < 6; ++i) {
    float depth = 0.0f;
    for (int corner : faces[static_cast<size_t>(i)]) {
      depth += projected[static_cast<size_t>(corner)].depth;
    }
    order[static_cast<size_t>(i)] = FaceOrder{i, depth * 0.25f};
  }
  std::sort(order.begin(), order.end(),
            [](const FaceOrder& a, const FaceOrder& b) { return a.depth > b.depth; });

  for (const FaceOrder& face : order) {
    const auto& corners = faces[static_cast<size_t>(face.index)];
    if (std::any_of(corners.begin(), corners.end(),
                    [&](int index) { return projected[static_cast<size_t>(index)].depth <= 0.12f; })) {
      continue;
    }
    std::array<Point3, 4> points{};
    for (int i = 0; i < 4; ++i) {
      points[static_cast<size_t>(i)] = projected[static_cast<size_t>(corners[static_cast<size_t>(i)])];
    }
    drawTriangleQuad(points, shade(primitive.tint, brightness[static_cast<size_t>(face.index)]));
  }
}

inline void drawBillboardPrimitive(const Primitive& primitive) {
  if (primitive.texture.texture == nullptr) {
    return;
  }
  const Vector3 right = cameraRight(camera);
  const float half = primitive.size.x * 0.5f;
  const Vector3 leftBottom = plus(plus(primitive.position, scale(right, -half)),
                                  Vector3{0, -primitive.size.y * 0.5f, 0});
  const Vector3 rightBottom = plus(plus(primitive.position, scale(right, half)),
                                   Vector3{0, -primitive.size.y * 0.5f, 0});
  const Vector3 rightTop = plus(plus(primitive.position, scale(right, half)),
                                Vector3{0, primitive.size.y * 0.5f, 0});
  const Vector3 leftTop = plus(plus(primitive.position, scale(right, -half)),
                               Vector3{0, primitive.size.y * 0.5f, 0});
  const std::array<Point3, 4> positions = {
      project(leftBottom), project(rightBottom), project(rightTop), project(leftTop)};
  if (std::any_of(positions.begin(), positions.end(),
                  [](const Point3& point) { return point.depth <= 0.12f; })) {
    return;
  }

  SDL_SetTextureColorMod(primitive.texture.texture, primitive.tint.r,
                         primitive.tint.g, primitive.tint.b);
  SDL_SetTextureAlphaMod(primitive.texture.texture, primitive.tint.a);
  const float u0 = primitive.source.x / static_cast<float>(primitive.texture.width);
  const float v0 = primitive.source.y / static_cast<float>(primitive.texture.height);
  const float u1 = (primitive.source.x + primitive.source.width) /
                   static_cast<float>(primitive.texture.width);
  const float v1 = (primitive.source.y + primitive.source.height) /
                   static_cast<float>(primitive.texture.height);
  SDL_Vertex vertices[4] = {
      vertex(positions[0], WHITE, SDL_FPoint{u0, v1}),
      vertex(positions[1], WHITE, SDL_FPoint{u1, v1}),
      vertex(positions[2], WHITE, SDL_FPoint{u1, v0}),
      vertex(positions[3], WHITE, SDL_FPoint{u0, v0}),
  };
  const int indices[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(renderer, primitive.texture.texture, vertices, 4, indices, 6);
  SDL_SetTextureColorMod(primitive.texture.texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(primitive.texture.texture, 255);
}

inline void drawRingPrimitive(const Primitive& primitive) {
  constexpr int segments = 36;
  std::array<Point3, segments> points{};
  for (int i = 0; i < segments; ++i) {
    const float angle = static_cast<float>(i) * 6.28318530718f /
                        static_cast<float>(segments);
    points[static_cast<size_t>(i)] = project(Vector3{
        primitive.position.x + std::cos(angle) * primitive.radius,
        primitive.position.y,
        primitive.position.z + std::sin(angle) * primitive.radius,
    });
  }
  SDL_SetRenderDrawColor(renderer, primitive.tint.r, primitive.tint.g,
                         primitive.tint.b, primitive.tint.a);
  for (int i = 0; i < segments; ++i) {
    const Point3& a = points[static_cast<size_t>(i)];
    const Point3& b = points[static_cast<size_t>((i + 1) % segments)];
    if (a.depth > 0.12f && b.depth > 0.12f) {
      SDL_RenderDrawLineF(renderer, a.x, a.y, b.x, b.y);
    }
  }
}

inline const std::unordered_map<char, std::array<unsigned char, 7>>& font() {
  static const std::unordered_map<char, std::array<unsigned char, 7>> glyphs = {
      {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
      {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
      {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
      {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
      {'I', {14, 4, 4, 4, 4, 4, 14}}, {'J', {7, 2, 2, 2, 18, 18, 12}},
      {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
      {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
      {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
      {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
      {'S', {15, 16, 16, 14, 1, 1, 30}}, {'T', {31, 4, 4, 4, 4, 4, 4}},
      {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
      {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
      {'Y', {17, 17, 10, 4, 4, 4, 4}}, {'Z', {31, 1, 2, 4, 8, 16, 31}},
      {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
      {'2', {14, 17, 1, 2, 4, 8, 31}}, {'3', {30, 1, 1, 14, 1, 1, 30}},
      {'4', {2, 6, 10, 18, 31, 2, 2}}, {'5', {31, 16, 16, 30, 1, 1, 30}},
      {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
      {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
      {' ', {0, 0, 0, 0, 0, 0, 0}}, {'-', {0, 0, 0, 31, 0, 0, 0}},
      {'/', {1, 2, 2, 4, 8, 8, 16}}, {':', {0, 4, 4, 0, 4, 4, 0}},
      {'.', {0, 0, 0, 0, 0, 12, 12}}, {'%', {17, 2, 4, 8, 16, 17, 0}},
      {'!', {4, 4, 4, 4, 4, 0, 4}}, {'?', {14, 17, 1, 2, 4, 0, 4}},
      {'+', {0, 4, 4, 31, 4, 4, 0}}, {'_', {0, 0, 0, 0, 0, 0, 31}},
  };
  return glyphs;
}

inline void setDrawColor(Color tint) {
  SDL_SetRenderDrawColor(renderer, tint.r, tint.g, tint.b, tint.a);
}

}  // namespace project_dino_platform

inline Color Fade(Color tint, float alpha) {
  tint.a = static_cast<unsigned char>(std::clamp(tint.a * alpha, 0.0f, 255.0f));
  return tint;
}

inline void SetConfigFlags(unsigned int) {}
inline void SetExitKey(int) {}

inline void SetWindowTitle(const char* title) {
  SDL_SetWindowTitle(project_dino_platform::window, title);
}

inline void InitWindow(int width, int height, const char* title) {
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL could not initialize: %s\n", SDL_GetError());
    std::exit(1);
  }
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
  project_dino_platform::window = SDL_CreateWindow(
      title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (project_dino_platform::window == nullptr) {
    std::fprintf(stderr, "SDL could not create a window: %s\n", SDL_GetError());
    std::exit(1);
  }
  project_dino_platform::renderer = SDL_CreateRenderer(
      project_dino_platform::window, -1, SDL_RENDERER_SOFTWARE);
  if (project_dino_platform::renderer == nullptr) {
    std::fprintf(stderr, "SDL could not create a software renderer: %s\n", SDL_GetError());
    std::exit(1);
  }
  SDL_SetRenderDrawBlendMode(project_dino_platform::renderer, SDL_BLENDMODE_BLEND);
  project_dino_platform::appStart = std::chrono::steady_clock::now();
  project_dino_platform::lastFrame = project_dino_platform::appStart;
}

inline void SetTargetFPS(int fps) {
  project_dino_platform::targetFps = fps;
}

inline bool WindowShouldClose() {
  using namespace project_dino_platform;
  std::fill(pressedKeys.begin(), pressedKeys.end(), false);
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      closeRequested = true;
    } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
      const int key = static_cast<int>(event.key.keysym.scancode);
      if (key >= 0 && key < static_cast<int>(pressedKeys.size())) {
        pressedKeys[static_cast<size_t>(key)] = true;
      }
    }
  }
  const auto now = std::chrono::steady_clock::now();
  frameTime = std::chrono::duration<float>(now - lastFrame).count();
  lastFrame = now;
  return closeRequested;
}

inline float GetFrameTime() {
  return project_dino_platform::frameTime;
}

inline float GetTime() {
  using namespace project_dino_platform;
  return std::chrono::duration<float>(std::chrono::steady_clock::now() - appStart).count();
}

inline int GetScreenWidth() {
  int width = 0;
  SDL_GetRendererOutputSize(project_dino_platform::renderer, &width, nullptr);
  return width;
}

inline int GetScreenHeight() {
  int height = 0;
  SDL_GetRendererOutputSize(project_dino_platform::renderer, nullptr, &height);
  return height;
}

inline bool IsKeyDown(int key) {
  const Uint8* state = SDL_GetKeyboardState(nullptr);
  return key >= 0 && key < SDL_NUM_SCANCODES && state[key] != 0;
}

inline bool IsKeyPressed(int key) {
  using namespace project_dino_platform;
  return key >= 0 && key < static_cast<int>(pressedKeys.size()) &&
         pressedKeys[static_cast<size_t>(key)];
}

inline void ToggleFullscreen() {
  using namespace project_dino_platform;
  fullscreen = !fullscreen;
  SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

inline Image GenImageColor(int width, int height, Color background) {
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
      0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
  if (surface != nullptr) {
    const Uint32 pixel = SDL_MapRGBA(surface->format, background.r, background.g,
                                     background.b, background.a);
    SDL_FillRect(surface, nullptr, pixel);
  }
  return Image{surface};
}

inline void ImageDrawPixel(Image* image, int x, int y, Color tint) {
  if (image == nullptr || image->surface == nullptr ||
      x < 0 || y < 0 || x >= image->surface->w || y >= image->surface->h) {
    return;
  }
  const Uint32 pixel = SDL_MapRGBA(image->surface->format, tint.r, tint.g,
                                   tint.b, tint.a);
  SDL_Rect rect{x, y, 1, 1};
  SDL_FillRect(image->surface, &rect, pixel);
}

inline Texture2D LoadTextureFromImage(Image image) {
  if (image.surface == nullptr) {
    return Texture2D{nullptr, 0, 0};
  }
  SDL_Texture* texture =
      SDL_CreateTextureFromSurface(project_dino_platform::renderer, image.surface);
  if (texture != nullptr) {
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  }
  return Texture2D{texture, image.surface->w, image.surface->h};
}

inline void SetTextureFilter(Texture2D texture, int filter) {
  if (texture.texture != nullptr) {
    SDL_SetTextureScaleMode(texture.texture,
                            filter == TEXTURE_FILTER_POINT ? SDL_ScaleModeNearest
                                                           : SDL_ScaleModeLinear);
  }
}

inline void UnloadImage(Image image) {
  if (image.surface != nullptr) {
    SDL_FreeSurface(image.surface);
  }
}

inline void UnloadTexture(Texture2D texture) {
  if (texture.texture != nullptr) {
    SDL_DestroyTexture(texture.texture);
  }
}

inline void BeginDrawing() {}

inline void ClearBackground(Color tint) {
  using namespace project_dino_platform;
  setDrawColor(tint);
  SDL_RenderClear(renderer);
}

inline void EndDrawing() {
  using namespace project_dino_platform;
  SDL_RenderPresent(renderer);
  if (targetFps > 0) {
    const float target = 1.0f / static_cast<float>(targetFps);
    const float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - lastFrame).count();
    if (elapsed < target) {
      SDL_Delay(static_cast<Uint32>((target - elapsed) * 1000.0f));
    }
  }
}

inline void BeginMode3D(Camera3D source) {
  project_dino_platform::camera = source;
  project_dino_platform::primitives.clear();
  project_dino_platform::in3d = true;
}

inline void EndMode3D() {
  using namespace project_dino_platform;
  std::sort(primitives.begin(), primitives.end(),
            [](const Primitive& a, const Primitive& b) { return a.depth > b.depth; });
  for (const Primitive& primitive : primitives) {
    switch (primitive.kind) {
      case PrimitiveKind::Cube:
      case PrimitiveKind::WireCube:
        drawCubePrimitive(primitive);
        break;
      case PrimitiveKind::Billboard:
        drawBillboardPrimitive(primitive);
        break;
      case PrimitiveKind::Ring:
        drawRingPrimitive(primitive);
        break;
    }
  }
  primitives.clear();
  in3d = false;
}

inline void DrawCubeV(Vector3 position, Vector3 size, Color tint) {
  using namespace project_dino_platform;
  primitives.push_back(Primitive{PrimitiveKind::Cube, position, size, 0.0f, tint,
                                 Texture2D{nullptr, 0, 0}, Rectangle{0, 0, 0, 0},
                                 primitiveDepth(position)});
}

inline void DrawCubeWiresV(Vector3 position, Vector3 size, Color tint) {
  using namespace project_dino_platform;
  primitives.push_back(Primitive{PrimitiveKind::WireCube, position, size, 0.0f, tint,
                                 Texture2D{nullptr, 0, 0}, Rectangle{0, 0, 0, 0},
                                 primitiveDepth(position)});
}

inline void DrawBillboard(Camera3D source, Texture2D texture, Rectangle sourceRect,
                          Vector3 position, float width, float height, Color tint) {
  using namespace project_dino_platform;
  camera = source;
  primitives.push_back(Primitive{PrimitiveKind::Billboard, position,
                                 Vector3{width, height, 0.0f}, 0.0f, tint,
                                 texture, sourceRect, primitiveDepth(position)});
}

inline void DrawBillboard(Camera3D source, Texture2D texture, Vector3 position,
                          float size, Color tint) {
  DrawBillboard(source, texture, Rectangle{0.0f, 0.0f,
                                            static_cast<float>(texture.width),
                                            static_cast<float>(texture.height)},
                position, size, size, tint);
}

inline void DrawCircle3D(Vector3 center, float radius, Vector3, float, Color tint) {
  using namespace project_dino_platform;
  primitives.push_back(Primitive{PrimitiveKind::Ring, center, Vector3{0, 0, 0},
                                 radius, tint, Texture2D{nullptr, 0, 0},
                                 Rectangle{0, 0, 0, 0},
                                 primitiveDepth(center)});
}

inline void DrawRectangle(int x, int y, int width, int height, Color tint) {
  using namespace project_dino_platform;
  SDL_Rect rect{x, y, width, height};
  setDrawColor(tint);
  SDL_RenderFillRect(renderer, &rect);
}

inline void DrawRectangleLinesEx(Rectangle rectangle, float thickness, Color tint) {
  using namespace project_dino_platform;
  SDL_Rect rect{static_cast<int>(rectangle.x), static_cast<int>(rectangle.y),
                static_cast<int>(rectangle.width), static_cast<int>(rectangle.height)};
  setDrawColor(tint);
  const int lineWidth = std::max(1, static_cast<int>(thickness));
  for (int i = 0; i < lineWidth; ++i) {
    SDL_RenderDrawRect(renderer, &rect);
    ++rect.x;
    ++rect.y;
    rect.w -= 2;
    rect.h -= 2;
    if (rect.w <= 0 || rect.h <= 0) {
      break;
    }
  }
}

inline void DrawRectangleGradientV(int x, int y, int width, int height,
                                   Color top, Color bottom) {
  using namespace project_dino_platform;
  for (int row = 0; row < height; ++row) {
    const float t = height > 1 ? static_cast<float>(row) / (height - 1) : 0.0f;
    const Color tint{
        static_cast<unsigned char>(top.r + (bottom.r - top.r) * t),
        static_cast<unsigned char>(top.g + (bottom.g - top.g) * t),
        static_cast<unsigned char>(top.b + (bottom.b - top.b) * t),
        static_cast<unsigned char>(top.a + (bottom.a - top.a) * t),
    };
    setDrawColor(tint);
    SDL_RenderDrawLine(renderer, x, y + row, x + width, y + row);
  }
}

inline void DrawCircle(int centerX, int centerY, float radius, Color tint) {
  using namespace project_dino_platform;
  setDrawColor(tint);
  const int r = std::max(1, static_cast<int>(radius));
  for (int y = -r; y <= r; ++y) {
    const int x = static_cast<int>(std::sqrt(std::max(0, r * r - y * y)));
    SDL_RenderDrawLine(renderer, centerX - x, centerY + y, centerX + x, centerY + y);
  }
}

inline int MeasureText(const char* text, int fontSize) {
  if (text == nullptr) {
    return 0;
  }
  const int scale = std::max(1, fontSize / 12);
  return static_cast<int>(std::string(text).size()) * 6 * scale;
}

inline void DrawText(const char* text, int x, int y, int fontSize, Color tint) {
  using namespace project_dino_platform;
  if (text == nullptr) {
    return;
  }
  const int scale = std::max(1, fontSize / 12);
  int cursor = x;
  for (const char* character = text; *character != '\0'; ++character) {
    char glyph = *character;
    if (glyph >= 'a' && glyph <= 'z') {
      glyph = static_cast<char>(glyph - 'a' + 'A');
    }
    const auto found = font().find(glyph);
    if (found != font().end()) {
      for (int row = 0; row < 7; ++row) {
        const unsigned char pattern = found->second[static_cast<size_t>(row)];
        for (int column = 0; column < 5; ++column) {
          if ((pattern & (1u << (4 - column))) != 0) {
            SDL_Rect pixel{cursor + column * scale, y + row * scale, scale, scale};
            setDrawColor(tint);
            SDL_RenderFillRect(renderer, &pixel);
          }
        }
      }
    }
    cursor += 6 * scale;
  }
}

inline const char* TextFormat(const char* format, ...) {
  static thread_local char buffer[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return buffer;
}

inline void CloseWindow() {
  using namespace project_dino_platform;
  if (renderer != nullptr) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window != nullptr) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }
  SDL_Quit();
}
