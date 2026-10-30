#include "platform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;
constexpr float COURSE_LENGTH = 7200.0f;
constexpr float COURSE_SEGMENT = 30.0f;
constexpr float PLAYER_SPEED = 7.4f;
constexpr float GRAVITY = 23.0f;

Color color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
  return Color{r, g, b, a};
}

float clampf(float value, float low, float high) {
  return std::max(low, std::min(value, high));
}

float distance2D(float ax, float az, float bx, float bz) {
  const float dx = ax - bx;
  const float dz = az - bz;
  return std::sqrt(dx * dx + dz * dz);
}

struct Platform {
  Vector3 center;
  float width;
  float depth;
  Color topColor;
  bool hurts;
  bool checkpoint;
};

struct Trap {
  float x;
  float progress;
  float radius;
  float phase;
  bool moving;
};

struct Enemy {
  Vector3 position;
  int health;
  int maxHealth;
  int wave;
  float attackCooldown;
  float hurtFlash;
  float bobSeed;
  bool alive;
  bool boss;
  float attackTimer{0.0f};
  float defeatTimer{0.0f};
  bool moving{false};
};

struct EnemyWave {
  float progress;
  int enemyCount;
  bool active;
  bool cleared;
  bool boss;
};

struct ClimbWall {
  float progress;
  float height;
};

enum class GameState {
  Title,
  Playing,
  Paused,
  Won
};

enum class Attack {
  None,
  Bite,
  Tail,
  Sweep,
  Claw
};

enum class SpriteAnimation {
  Idle,
  Run,
  Jump,
  Bite,
  Tail,
  Sweep,
  Claw,
  EnemyAttack,
  Damage,
  Defeat,
  Count
};

constexpr int SPRITE_FRAME_COUNT = 4;
constexpr float ENEMY_ATTACK_ANIMATION_DURATION = 0.3f;
constexpr float DEFEAT_ANIMATION_DURATION = 0.72f;

struct AnimatedSprite {
  Texture2D texture;
  int frameWidth;
  int frameHeight;
};

struct Player {
  Vector3 position{0.0f, 0.0f, 0.0f};
  Vector3 velocity{0.0f, 0.0f, 0.0f};
  float health{100.0f};
  float attackCooldown{0.0f};
  float attackTimer{0.0f};
  float invulnerability{0.0f};
  float comboTimer{0.0f};
  int combo{0};
  int kills{0};
  int checkpoint{0};
  int collectibles{0};
  bool grounded{true};
  bool climbing{false};
  Attack attack{Attack::None};
  float hurtTimer{0.0f};
  float defeatTimer{0.0f};
  bool moving{false};
};

Color spritePixelColor(char pixel, Color shade, Color dark, Color highlight,
                       Color accent) {
  switch (pixel) {
    case '1':
      return shade;
    case '2':
      return dark;
    case '3':
      return highlight;
    case '4':
      return accent;
    default:
      return BLANK;
  }
}

void drawAtlasPixel(Image& image, int frameX, int x, int y, int frameWidth,
                    int frameHeight, Color tint) {
  if (x < 0 || y < 0 || x >= frameWidth || y >= frameHeight) {
    return;
  }
  ImageDrawPixel(&image, frameX + x, y, tint);
}

AnimatedSprite makeAnimatedSprite(const std::vector<std::string>& rows,
                                  Color shade, Color dark, Color highlight,
                                  Color accent, bool kraken) {
  const int frameWidth = static_cast<int>(rows.front().size());
  const int frameHeight = static_cast<int>(rows.size());
  const int frameTotal = static_cast<int>(SpriteAnimation::Count) *
                         SPRITE_FRAME_COUNT;
  Image image = GenImageColor(frameWidth * frameTotal, frameHeight, BLANK);

  for (int animationIndex = 0;
       animationIndex < static_cast<int>(SpriteAnimation::Count);
       ++animationIndex) {
    const SpriteAnimation animation =
        static_cast<SpriteAnimation>(animationIndex);
    for (int frame = 0; frame < SPRITE_FRAME_COUNT; ++frame) {
      const int frameX =
          (animationIndex * SPRITE_FRAME_COUNT + frame) * frameWidth;
      for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
          const Color pixelColor =
              spritePixelColor(rows[static_cast<size_t>(y)][static_cast<size_t>(x)],
                               shade, dark, highlight, accent);
          if (pixelColor.a == 0) {
            continue;
          }

          int targetX = x;
          int targetY = y;
          if (animation == SpriteAnimation::Idle && frame == 1) {
            targetY = std::min(frameHeight - 1, y + 1);
          } else if (animation == SpriteAnimation::Run) {
            if (!kraken && y >= 13) {
              targetX += (x < frameWidth / 2 ? 1 : -1) * (frame % 2 == 0 ? 1 : -1);
            } else if (kraken && y >= 11) {
              const int tentacle = x / 4;
              targetX += ((tentacle + frame) % 2 == 0) ? -1 : 1;
            }
          } else if (animation == SpriteAnimation::Jump) {
            targetY = std::max(0, y - (frame < 2 ? 1 : 2));
          } else if (animation == SpriteAnimation::Defeat) {
            if (frame < 2) {
              targetY = std::min(frameHeight - 1, y + frame);
            } else {
              targetX = y;
              targetY = frameHeight - 1 - x;
            }
          }

          drawAtlasPixel(image, frameX, targetX, targetY, frameWidth, frameHeight,
                         pixelColor);
        }
      }

      const auto put = [&](int x, int y, Color tint) {
        drawAtlasPixel(image, frameX, x, y, frameWidth, frameHeight, tint);
      };
      if (animation == SpriteAnimation::Bite && !kraken) {
        // Open jaw and tongue frames make the bite readable at billboard scale.
        put(14, 8, dark);
        put(15, 9, accent);
        if (frame >= 2) {
          put(14, 10, color(229, 90, 83));
          put(15, 10, highlight);
        }
      } else if (animation == SpriteAnimation::Tail && !kraken) {
        const int swing = (frame < 2) ? 1 : -1;
        put(3, 9, dark);
        put(2, 10 + swing, shade);
        put(1, 11 + swing, dark);
        put(0, 12 + swing, accent);
        if (frame == 1 || frame == 3) {
          put(0, 9, highlight);
          put(1, 8, highlight);
        }
      } else if (animation == SpriteAnimation::Sweep && !kraken) {
        put(8, 14, dark);
        put(9, 14, shade);
        put(10, 15, shade);
        put(11, 15, highlight);
        put(12, 15, accent);
        if (frame >= 2) {
          put(13, 14, highlight);
          put(14, 13, accent);
        }
      } else if (animation == SpriteAnimation::Claw && !kraken) {
        put(12, 10, dark);
        put(13, 10, shade);
        put(14, 10, shade);
        put(15, 11, highlight);
        put(14, 12, accent);
        if (frame >= 2) {
          put(15, 9, highlight);
          put(14, 8, accent);
        }
      } else if (animation == SpriteAnimation::EnemyAttack && kraken) {
        const int reach = frame + 2;
        for (int offset = 0; offset < reach; ++offset) {
          put(2 - offset, 10 + offset / 2, shade);
          put(13 + offset, 10 + offset / 2, shade);
        }
        put(std::max(0, 2 - reach), 11 + reach / 2, accent);
        put(std::min(frameWidth - 1, 13 + reach), 11 + reach / 2, accent);
      } else if (animation == SpriteAnimation::Damage) {
        const Color spark = color(255, 108, 130);
        if (frame != 2) {
          put(13, 4, spark);
          put(15, 6, highlight);
          put(1, 8, spark);
          if (frame == 1 || frame == 3) {
            put(14, 2, accent);
            put(0, 6, highlight);
          }
        }
      } else if (animation == SpriteAnimation::Defeat && frame >= 2) {
        put(3, 5, accent);
        put(5, 3, highlight);
        put(11, 4, accent);
        put(13, 6, highlight);
      }
    }
  }

  Texture2D texture = LoadTextureFromImage(image);
  UnloadImage(image);
  SetTextureFilter(texture, TEXTURE_FILTER_POINT);
  return AnimatedSprite{texture, frameWidth, frameHeight};
}

AnimatedSprite makeDinoSprite() {
  return makeAnimatedSprite({
      "                ",
      "       333      ",
      "     331113     ",
      "    31111113    ",
      "   3111111113   ",
      "   31131111113  ",
      "   311111111113 ",
      "  31111111111113",
      "  22111111111114",
      "  22211111111114",
      "   2221111111113",
      "    21111111113 ",
      "     211111113  ",
      "     22  2211   ",
      "     22  22     ",
      "    222  222    ",
  }, color(91, 181, 74), color(37, 87, 47), color(174, 222, 113),
     color(245, 210, 131), false);
}

AnimatedSprite makeKrakenSprite() {
  return makeAnimatedSprite({
      "      44  44    ",
      "   4444444444   ",
      "  422444444224  ",
      "  422244442224  ",
      "   4222222224   ",
      "   4212242224   ",
      "   4444444444   ",
      "  441444444144  ",
      "  441444444144  ",
      "   4144444414   ",
      "   1414444141   ",
      "  141  41  141  ",
      "  41   41   14  ",
      "  1    41    1  ",
      "       44       ",
      "       44       ",
  }, color(129, 69, 184), color(71, 35, 112), color(201, 132, 230),
     color(255, 95, 112), true);
}

SpriteAnimation playerAnimation(const Player& player) {
  if (player.defeatTimer > 0.0f) {
    return SpriteAnimation::Defeat;
  }
  if (player.hurtTimer > 0.0f) {
    return SpriteAnimation::Damage;
  }
  if (player.attackTimer > 0.0f) {
    switch (player.attack) {
      case Attack::Bite:
        return SpriteAnimation::Bite;
      case Attack::Tail:
        return SpriteAnimation::Tail;
      case Attack::Sweep:
        return SpriteAnimation::Sweep;
      case Attack::Claw:
        return SpriteAnimation::Claw;
      case Attack::None:
        break;
    }
  }
  if (!player.grounded || player.climbing) {
    return SpriteAnimation::Jump;
  }
  return player.moving ? SpriteAnimation::Run : SpriteAnimation::Idle;
}

SpriteAnimation enemyAnimation(const Enemy& enemy) {
  if (!enemy.alive) {
    return SpriteAnimation::Defeat;
  }
  if (enemy.hurtFlash > 0.0f) {
    return SpriteAnimation::Damage;
  }
  if (enemy.attackTimer > 0.0f) {
    return SpriteAnimation::EnemyAttack;
  }
  return enemy.moving ? SpriteAnimation::Run : SpriteAnimation::Idle;
}

int animationFrame(SpriteAnimation animation, float time, float seed,
                   float actionProgress = -1.0f) {
  if (actionProgress >= 0.0f) {
    return std::min(SPRITE_FRAME_COUNT - 1,
                    std::max(0, static_cast<int>(actionProgress *
                                                 SPRITE_FRAME_COUNT)));
  }
  const float rate = (animation == SpriteAnimation::Run) ? 10.0f :
                     (animation == SpriteAnimation::Damage) ? 14.0f :
                     (animation == SpriteAnimation::Jump) ? 5.0f : 2.2f;
  const int frameCount = (animation == SpriteAnimation::Idle) ? 2 : SPRITE_FRAME_COUNT;
  int frame = static_cast<int>(std::floor((time + seed) * rate)) % frameCount;
  if (frame < 0) {
    frame += frameCount;
  }
  return frame;
}

Rectangle spriteFrameSource(const AnimatedSprite& sprite,
                            SpriteAnimation animation, int frame) {
  const int frameIndex = static_cast<int>(animation) * SPRITE_FRAME_COUNT + frame;
  return Rectangle{static_cast<float>(frameIndex * sprite.frameWidth), 0.0f,
                   static_cast<float>(sprite.frameWidth),
                   static_cast<float>(sprite.frameHeight)};
}

void drawPanel(int x, int y, int width, int height, Color fill, Color border) {
  DrawRectangle(x, y, width, height, fill);
  DrawRectangleLinesEx(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                  static_cast<float>(width), static_cast<float>(height)},
                       2.0f, border);
}

void drawTextShadow(const char* text, int x, int y, int fontSize, Color textColor) {
  DrawText(text, x + 2, y + 3, fontSize, color(0, 0, 0, 140));
  DrawText(text, x, y, fontSize, textColor);
}

void addPlatform(std::vector<Platform>& platforms, float x, float progress,
                 float width, float depth, float y, Color topColor,
                 bool hurts = false, bool checkpoint = false) {
  platforms.push_back(Platform{
      Vector3{x, y, -progress},
      width,
      depth,
      topColor,
      hurts,
      checkpoint,
  });
}

void buildCourse(std::vector<Platform>& platforms, std::vector<Trap>& traps,
                 std::vector<EnemyWave>& waves, std::vector<ClimbWall>& walls) {
  const std::array<Color, 6> groundColors = {
      color(93, 130, 67), color(119, 134, 70), color(93, 136, 96),
      color(118, 112, 70), color(84, 129, 121), color(134, 106, 75)};

  const int segmentCount = static_cast<int>(COURSE_LENGTH / COURSE_SEGMENT);
  for (int i = 0; i < segmentCount; ++i) {
    const float progress = (static_cast<float>(i) + 0.5f) * COURSE_SEGMENT;
    const bool gap = (i % 17 == 8) || (i % 29 == 21);
    const bool narrow = (i % 13 >= 9 && i % 13 <= 11);
    const float width = narrow ? 4.5f : 10.0f;
    const Color tint = groundColors[static_cast<size_t>((i / 20) % groundColors.size())];

    if (!gap) {
      const bool thornBed = (i % 14 == 6);
      addPlatform(platforms, 0.0f, progress, width, COURSE_SEGMENT,
                  0.0f, thornBed ? color(125, 102, 56) : tint, thornBed);
    } else {
      addPlatform(platforms, -2.0f, progress - 8.0f, 4.1f, 8.2f, 0.0f,
                  color(116, 115, 77));
      addPlatform(platforms, 2.3f, progress, 4.0f, 8.0f, 0.55f,
                  color(124, 130, 91));
      addPlatform(platforms, -1.5f, progress + 8.2f, 4.4f, 8.4f, 0.0f,
                  color(116, 115, 77));
    }

    if (i % 12 == 3) {
      traps.push_back(Trap{(i % 24 == 3) ? -2.4f : 2.4f,
                           progress + 8.0f, 1.65f, static_cast<float>(i) * 0.61f, false});
    }
    if (i % 16 == 12) {
      traps.push_back(Trap{0.0f, progress + 10.0f, 2.1f,
                           static_cast<float>(i) * 0.49f, true});
    }

    if (i % 20 == 18) {
      addPlatform(platforms, (i % 40 == 18) ? -7.0f : 7.0f, progress,
                  3.6f, 5.5f, 1.1f, color(163, 139, 83));
    }
  }

  for (int act = 0; act < 6; ++act) {
    const float base = static_cast<float>(act) * 1200.0f;
    waves.push_back(EnemyWave{base + 130.0f, 3, false, false, false});
    waves.push_back(EnemyWave{base + 310.0f, 4, false, false, false});
    waves.push_back(EnemyWave{base + 490.0f, 4, false, false, false});
    waves.push_back(EnemyWave{base + 730.0f, 3, false, false, false});
    waves.push_back(EnemyWave{base + 910.0f, 4, false, false, false});
    waves.push_back(EnemyWave{base + 1090.0f, 4, false, false, false});

    for (int wallInAct = 0; wallInAct < 2; ++wallInAct) {
      const float wallProgress = base + 560.0f + wallInAct * 600.0f;
      if (wallProgress < COURSE_LENGTH - 80.0f) {
        walls.push_back(ClimbWall{wallProgress, 4.6f});
        addPlatform(platforms, 0.0f, wallProgress + 9.0f, 9.0f, 18.0f,
                    4.65f, color(109, 145, 82));
        addPlatform(platforms, 0.0f, wallProgress + 22.0f, 10.0f, 10.0f,
                    0.0f, color(93, 130, 67));
      }
    }

    if (act > 0) {
      addPlatform(platforms, 0.0f, base + 6.0f, 12.0f, 24.0f,
                  0.0f, color(218, 180, 94), false, true);
    }
  }

  waves.push_back(EnemyWave{COURSE_LENGTH - 35.0f, 1, false, false, true});
}

bool platformContains(const Platform& platform, float x, float z) {
  return std::fabs(x - platform.center.x) <= platform.width * 0.5f &&
         std::fabs(z - platform.center.z) <= platform.depth * 0.5f;
}

float platformTopAt(const std::vector<Platform>& platforms, float x, float z,
                    float previousY, float currentY, bool& landed, bool& hurts) {
  float highest = -1000.0f;
  landed = false;
  hurts = false;

  for (const Platform& platform : platforms) {
    if (!platformContains(platform, x, z)) {
      continue;
    }
    const float top = platform.center.y;
    if (currentY <= top + 0.2f && previousY >= top - 0.3f && top > highest) {
      highest = top;
      landed = true;
      hurts = platform.hurts;
    } else if (std::fabs(currentY - top) < 0.06f && top > highest) {
      highest = top;
      landed = true;
      hurts = platform.hurts;
    }
  }
  return highest;
}

void drawCube(Vector3 position, Vector3 size, Color tint, bool outline = false) {
  DrawCubeV(position, size, tint);
  if (outline) {
    DrawCubeWiresV(position, size, Fade(color(25, 47, 37), 0.65f));
  }
}

void drawJungle(float progress, const std::vector<Platform>& platforms) {
  for (const Platform& platform : platforms) {
    const float distance = std::fabs(platform.center.z + progress);
    if (distance > 150.0f) {
      continue;
    }

    const Vector3 size{platform.width, 0.8f, platform.depth};
    drawCube(Vector3{platform.center.x, platform.center.y - 0.42f, platform.center.z},
             size, color(74, 84, 56));
    drawCube(Vector3{platform.center.x, platform.center.y - 0.03f, platform.center.z},
             Vector3{platform.width, 0.12f, platform.depth}, platform.topColor, true);

    if (platform.hurts) {
      for (int thorn = -3; thorn <= 3; ++thorn) {
        const float x = platform.center.x + static_cast<float>(thorn) * 1.2f;
        drawCube(Vector3{x, platform.center.y + 0.45f, platform.center.z},
                 Vector3{0.28f, 0.85f, 0.3f}, color(205, 93, 55));
      }
    }
    if (platform.checkpoint) {
      drawCube(Vector3{platform.center.x - 2.7f, platform.center.y + 1.0f,
                       platform.center.z},
               Vector3{0.28f, 2.1f, 0.28f}, color(227, 184, 81));
      drawCube(Vector3{platform.center.x - 2.1f, platform.center.y + 1.7f,
                       platform.center.z},
               Vector3{1.2f, 0.65f, 0.18f}, color(226, 86, 61));
    }
  }

  const int firstTree = std::max(0, static_cast<int>((progress - 170.0f) / 24.0f));
  const int lastTree = std::min(300, static_cast<int>((progress + 170.0f) / 24.0f));
  for (int i = firstTree; i <= lastTree; ++i) {
    const float treeProgress = static_cast<float>(i) * 24.0f;
    const float z = -treeProgress;
    const float side = (i % 2 == 0) ? -1.0f : 1.0f;
    for (int lane = 0; lane < 2; ++lane) {
      const float x = side * (10.5f + static_cast<float>(lane) * 3.8f);
      const float shift = lane == 0 ? 0.0f : 12.0f;
      const float trunkHeight = 5.0f + static_cast<float>((i + lane) % 3);
      drawCube(Vector3{x, trunkHeight * 0.5f, z - shift},
               Vector3{1.6f, trunkHeight, 1.7f}, color(96, 70, 48));
      drawCube(Vector3{x, trunkHeight + 1.0f, z - shift},
               Vector3{5.8f, 3.0f, 4.3f},
               (i % 3 == 0) ? color(48, 121, 67) : color(59, 139, 73));
      drawCube(Vector3{x + side * 1.7f, trunkHeight + 0.3f, z - shift - 1.2f},
               Vector3{2.5f, 2.0f, 2.6f}, color(76, 157, 78));
    }
  }

  // Far canopy silhouettes keep the horizon dense without a heavy scene.
  for (int i = -7; i <= 7; ++i) {
    const float x = static_cast<float>(i) * 7.0f;
    drawCube(Vector3{x, 15.0f, -progress - 165.0f},
             Vector3{9.0f, 12.0f, 18.0f},
             (i % 2 == 0) ? color(42, 101, 69) : color(55, 119, 77));
  }
}

void drawTraps(const std::vector<Trap>& traps, float progress, float time) {
  for (const Trap& trap : traps) {
    if (std::fabs(trap.progress - progress) > 95.0f) {
      continue;
    }
    const float z = -trap.progress;
    if (trap.moving) {
      const float x = std::sin(time * 2.1f + trap.phase) * 4.0f;
      drawCube(Vector3{x, 1.35f, z}, Vector3{5.0f, 1.3f, 1.4f},
               color(132, 81, 46), true);
      drawCube(Vector3{x - 2.25f, 1.35f, z},
               Vector3{0.38f, 1.55f, 1.55f}, color(187, 123, 59));
      drawCube(Vector3{x + 2.25f, 1.35f, z},
               Vector3{0.38f, 1.55f, 1.55f}, color(187, 123, 59));
    } else {
      drawCube(Vector3{trap.x, 0.42f, z},
               Vector3{3.3f, 0.55f, 3.4f}, color(123, 93, 63));
      for (int spike = -1; spike <= 1; ++spike) {
        drawCube(Vector3{trap.x + static_cast<float>(spike) * 0.8f,
                         0.92f, z},
                 Vector3{0.35f, 0.72f, 0.4f}, color(205, 84, 57));
      }
    }
  }
}

void drawClimbWalls(const std::vector<ClimbWall>& walls, float progress) {
  for (const ClimbWall& wall : walls) {
    if (std::fabs(wall.progress - progress) > 90.0f) {
      continue;
    }
    drawCube(Vector3{0.0f, wall.height * 0.5f, -wall.progress},
             Vector3{9.5f, wall.height, 1.3f}, color(111, 85, 58), true);
    for (int rung = 1; rung <= 4; ++rung) {
      drawCube(Vector3{0.0f, static_cast<float>(rung) * 0.85f,
                       -wall.progress - 0.72f},
               Vector3{5.5f, 0.18f, 0.2f}, color(189, 164, 106));
    }
    drawCube(Vector3{0.0f, wall.height + 0.5f, -wall.progress},
             Vector3{10.0f, 0.55f, 1.8f}, color(73, 133, 71));
  }
}

void drawWaveGate(const EnemyWave& wave, int waveIndex, int activeWave) {
  if (wave.cleared || std::abs(waveIndex - activeWave) > 1) {
    return;
  }
  const float z = -(wave.progress + 18.0f);
  const Color gateColor = wave.active ? color(255, 113, 87, 220) : color(118, 79, 65, 200);
  drawCube(Vector3{-5.0f, 2.3f, z}, Vector3{0.55f, 4.6f, 0.75f}, gateColor);
  drawCube(Vector3{5.0f, 2.3f, z}, Vector3{0.55f, 4.6f, 0.75f}, gateColor);
  drawCube(Vector3{0.0f, 4.3f, z}, Vector3{10.5f, 0.45f, 0.75f}, gateColor);
}

void drawAttackEffect(const Player& player) {
  if (player.attackTimer <= 0.0f) {
    return;
  }
  const Color effect = Fade(color(242, 205, 113), 0.74f);
  const float radius = (player.attack == Attack::Tail) ? 3.6f : 2.2f;
  DrawCircle3D(Vector3{player.position.x, player.position.y + 0.9f,
                       player.position.z - 0.5f},
               radius, Vector3{1.0f, 0.0f, 0.0f}, 90.0f, effect);
}

void drawHud(const Player& player, float elapsed, float progress,
             int activeWave, const std::vector<EnemyWave>& waves, const char* toast,
             float toastTimer) {
  const int screenW = GetScreenWidth();
  const int screenH = GetScreenHeight();
  drawPanel(22, 20, 330, 112, color(15, 29, 31, 225), color(172, 197, 130, 200));
  drawTextShadow("PROJECT DINO", 40, 34, 19, color(232, 213, 153));
  DrawRectangle(40, 66, 275, 15, color(57, 64, 55));
  DrawRectangle(40, 66, static_cast<int>(275.0f * clampf(player.health / 100.0f, 0, 1)),
                15, color(212, 79, 67));
  drawTextShadow(TextFormat("HEALTH  %d", static_cast<int>(std::max(0.0f, player.health))),
                 40, 88, 16, RAYWHITE);
  drawTextShadow(TextFormat("CHECKPOINT %d / 6", player.checkpoint + 1),
                 192, 88, 15, color(205, 212, 185));

  const int panelW = 258;
  drawPanel(screenW - panelW - 22, 20, panelW, 88,
            color(15, 29, 31, 220), color(172, 197, 130, 200));
  const int minutes = static_cast<int>(elapsed) / 60;
  const int seconds = static_cast<int>(elapsed) % 60;
  drawTextShadow(TextFormat("TIME  %02d:%02d", minutes, seconds),
                 screenW - panelW, 38, 18, RAYWHITE);
  drawTextShadow(TextFormat("COURSE  %02d%%", static_cast<int>(progress / COURSE_LENGTH * 100.0f)),
                 screenW - panelW, 70, 16, color(207, 213, 180));

  const float progressWidth = std::min(450.0f, static_cast<float>(screenW) * 0.38f);
  const float barX = (static_cast<float>(screenW) - progressWidth) * 0.5f;
  DrawRectangle(static_cast<int>(barX), screenH - 31, static_cast<int>(progressWidth), 9,
                color(20, 32, 30, 210));
  DrawRectangle(static_cast<int>(barX), screenH - 31,
                static_cast<int>(progressWidth * clampf(progress / COURSE_LENGTH, 0, 1)),
                9, color(218, 174, 81));

  const int act = std::min(5, static_cast<int>(progress / 1200.0f));
  const std::array<const char*, 6> actNames = {
      "FERNWAKE TRAIL", "THE ROOTWALL", "SUNKEN CANOPY",
      "SPIREWOOD RUN", "BLACKWATER BASIN", "HEART OF THE WILD"};
  drawTextShadow(actNames[static_cast<size_t>(act)], screenW / 2 - 120, 24, 17,
                 color(235, 223, 177));
  if (activeWave >= 0 && activeWave < static_cast<int>(waves.size()) &&
      waves[static_cast<size_t>(activeWave)].active) {
    drawPanel(screenW / 2 - 140, 54, 280, 36,
              color(58, 22, 29, 220), color(226, 96, 91));
    drawTextShadow(waves[static_cast<size_t>(activeWave)].boss
                       ? "THE KRAKEN BOD GUARDIAN"
                       : "CLEAR THE KRAKEN BODS",
                   screenW / 2 - 125, 62, 16, RAYWHITE);
  }

  drawPanel(22, screenH - 94, 526, 67, color(15, 29, 31, 205),
            color(172, 197, 130, 170));
  drawTextShadow("WASD move   SPACE jump   E climb", 38, screenH - 82, 15,
                 color(230, 229, 208));
  drawTextShadow("J bite   K tail combo   L leg-sweep   U claw-kick",
                 38, screenH - 55, 14, color(230, 229, 208));

  if (toastTimer > 0.0f && toast[0] != '\0') {
    const int textWidth = MeasureText(toast, 19);
    drawPanel((screenW - textWidth - 34) / 2, screenH - 132,
              textWidth + 34, 38, color(17, 35, 29, 225),
              color(220, 190, 110, 210));
    drawTextShadow(toast, (screenW - textWidth) / 2, screenH - 123, 19,
                   color(245, 225, 163));
  }
}

void showTitle() {
  const int screenW = GetScreenWidth();
  const int screenH = GetScreenHeight();
  DrawRectangleGradientV(0, 0, screenW, screenH,
                         color(21, 61, 57), color(11, 29, 31));
  for (int i = 0; i < 14; ++i) {
    const float x = static_cast<float>((i * 143) % screenW);
    const float y = static_cast<float>(screenH - 130 - (i * 37) % 260);
    DrawCircle(static_cast<int>(x), static_cast<int>(y), 105.0f + (i % 3) * 20.0f,
               Fade((i % 2 == 0) ? color(52, 110, 66) : color(66, 130, 75), 0.26f));
  }

  drawTextShadow("PROJECT", screenW / 2 - 210, screenH / 2 - 185, 43,
                 color(212, 227, 171));
  drawTextShadow("DINO", screenW / 2 - 215, screenH / 2 - 128, 88,
                 color(245, 199, 100));
  DrawRectangle(screenW / 2 - 210, screenH / 2 - 24, 420, 3,
                color(212, 227, 171, 190));
  drawTextShadow("BRAMBLEWAKE: THE KRAKEN BOD INCURSION",
                 screenW / 2 - 310, screenH / 2 + 3, 21, RAYWHITE);
  drawTextShadow("ONE STAGE  /  SIX WILDS  /  A 30-MINUTE JUNGLE GAUNTLET",
                 screenW / 2 - 333, screenH / 2 + 43, 16,
                 color(211, 215, 186));

  const int cardY = screenH / 2 + 95;
  drawPanel(screenW / 2 - 190, cardY, 380, 82,
            color(13, 31, 31, 215), color(202, 181, 119));
  drawTextShadow("01  /  BRAMBLEWAKE TRAIL", screenW / 2 - 157, cardY + 13, 19,
                 color(244, 222, 157));
  drawTextShadow("One connected obstacle-course stage", screenW / 2 - 157,
                 cardY + 43, 15, color(215, 220, 199));
  drawTextShadow("PRESS ENTER TO BEGIN", screenW / 2 - 142, cardY + 112, 19,
                 color(244, 212, 126));
  drawTextShadow("WASD move   SPACE jump   E climb   J/K/L/U attack",
                 screenW / 2 - 276, cardY + 151, 15, color(205, 215, 196));
}

void startGame(Player& player, std::vector<Enemy>& enemies,
               std::vector<EnemyWave>& waves, float& elapsed, int& activeWave,
               GameState& state, char* toast, float& toastTimer) {
  player = Player{};
  enemies.clear();
  for (EnemyWave& wave : waves) {
    wave.active = false;
    wave.cleared = false;
  }
  elapsed = 0.0f;
  activeWave = -1;
  toast[0] = '\0';
  toastTimer = 0.0f;
  state = GameState::Playing;
}

void spawnWave(std::vector<Enemy>& enemies, const EnemyWave& wave, int waveIndex) {
  if (wave.boss) {
    enemies.push_back(Enemy{
        Vector3{0.0f, 0.0f, -(wave.progress + 32.0f)},
        14, 14, waveIndex, 0.8f, 0.0f, static_cast<float>(waveIndex) * 1.8f,
        true, true});
    return;
  }

  for (int i = 0; i < wave.enemyCount; ++i) {
    const float x = (i % 2 == 0 ? -1.0f : 1.0f) *
                    (1.8f + static_cast<float>(i / 2) * 1.8f);
    const float z = -(wave.progress + 30.0f + static_cast<float>(i % 2) * 2.5f);
    enemies.push_back(Enemy{
        Vector3{x, 0.0f, z}, 2, 2, waveIndex, 0.4f, 0.0f,
        static_cast<float>(waveIndex * 7 + i) * 0.53f, true, false});
  }
}

void setToast(char* toast, size_t size, const char* text, float& timer) {
  std::snprintf(toast, size, "%s", text);
  timer = 2.7f;
}

void beginAttack(Player& player, Attack attack, std::vector<Enemy>& enemies,
                 char* toast, size_t toastSize, float& toastTimer) {
  if (player.attackCooldown > 0.0f || player.climbing) {
    return;
  }
  player.attack = attack;
  player.attackTimer = 0.36f;
  player.attackCooldown = attack == Attack::Tail ? 0.46f : 0.52f;
  player.combo = (player.comboTimer > 0.0f) ? player.combo + 1 : 1;
  player.comboTimer = 1.2f;

  const float radius = (attack == Attack::Tail) ? 4.6f :
                       (attack == Attack::Sweep) ? 3.7f :
                       (attack == Attack::Claw) ? 3.0f : 2.25f;
  const bool behindAllowed = attack == Attack::Tail || attack == Attack::Sweep;
  int hitCount = 0;
  for (Enemy& enemy : enemies) {
    if (!enemy.alive || distance2D(player.position.x, player.position.z,
                                   enemy.position.x, enemy.position.z) > radius) {
      continue;
    }
    const float forwardDot = -(enemy.position.z - player.position.z);
    if (!behindAllowed && forwardDot < -0.25f) {
      continue;
    }
    const int damage = attack == Attack::Bite ? 2 : (attack == Attack::Claw ? 2 : 1);
    enemy.health -= damage;
    enemy.hurtFlash = 0.18f;
    if (enemy.health <= 0) {
      enemy.alive = false;
      enemy.defeatTimer = DEFEAT_ANIMATION_DURATION;
      ++player.kills;
      player.health = std::min(100.0f, player.health + (enemy.boss ? 14.0f : 3.0f));
    }
    ++hitCount;
  }

  const char* names[] = {"", "BITE", "TAIL WHIP", "LEG-SWEEP", "CLAW-KICK"};
  const char* name = names[static_cast<int>(attack)];
  if (hitCount > 0) {
    setToast(toast, toastSize,
             TextFormat("%s  /  %d HIT%s", name, hitCount > 1 ? hitCount : 1,
                        player.combo > 1 ? "  /  COMBO" : ""),
             toastTimer);
  } else {
    setToast(toast, toastSize, name, toastTimer);
  }
}

void damagePlayer(Player& player, float damage, char* toast, size_t toastSize,
                  float& toastTimer) {
  if (player.invulnerability > 0.0f) {
    return;
  }
  player.health -= damage;
  player.invulnerability = 0.8f;
  player.hurtTimer = 0.32f;
  setToast(toast, toastSize, "A KRAKEN BOD HIT YOU", toastTimer);
}

void drawWorld(const Camera3D& camera, Player& player,
               const std::vector<Platform>& platforms, const std::vector<Trap>& traps,
               const std::vector<ClimbWall>& walls, const std::vector<EnemyWave>& waves,
               int activeWave, const std::vector<Enemy>& enemies,
               const AnimatedSprite& dinoSprite,
               const AnimatedSprite& krakenSprite, float progress, float time) {
  BeginMode3D(camera);
  drawJungle(progress, platforms);
  drawTraps(traps, progress, time);
  drawClimbWalls(walls, progress);
  for (int i = 0; i < static_cast<int>(waves.size()); ++i) {
    drawWaveGate(waves[static_cast<size_t>(i)], i, activeWave);
  }

  for (const Enemy& enemy : enemies) {
    if ((!enemy.alive && enemy.defeatTimer <= 0.0f) ||
        std::fabs(-enemy.position.z - progress) > 125.0f) {
      continue;
    }
    const SpriteAnimation animation = enemyAnimation(enemy);
    float actionProgress = -1.0f;
    if (animation == SpriteAnimation::EnemyAttack) {
      actionProgress = 1.0f - enemy.attackTimer / ENEMY_ATTACK_ANIMATION_DURATION;
    } else if (animation == SpriteAnimation::Defeat) {
      actionProgress = 1.0f - enemy.defeatTimer / DEFEAT_ANIMATION_DURATION;
    }
    const int frame = animationFrame(animation, time, enemy.bobSeed, actionProgress);
    DrawBillboard(camera, krakenSprite.texture,
                  spriteFrameSource(krakenSprite, animation, frame),
                  Vector3{enemy.position.x, enemy.position.y + 1.2f +
                              std::sin(time * 4.1f + enemy.bobSeed) * 0.13f,
                          enemy.position.z},
                  enemy.boss ? 5.2f : 2.5f,
                  enemy.boss ? 5.2f : 2.5f,
                  enemy.hurtFlash > 0.0f ? color(255, 210, 210) : WHITE);
    if (enemy.boss) {
      const float barWidth = 5.5f;
      const float ratio = clampf(static_cast<float>(enemy.health) /
                                     static_cast<float>(enemy.maxHealth),
                                 0.0f, 1.0f);
      drawCube(Vector3{enemy.position.x, 4.1f, enemy.position.z},
               Vector3{barWidth, 0.18f, 0.18f}, color(54, 41, 56));
      if (ratio > 0.0f) {
        drawCube(Vector3{enemy.position.x - barWidth * (1.0f - ratio) * 0.5f,
                         4.1f, enemy.position.z - 0.02f},
                 Vector3{barWidth * ratio, 0.2f, 0.22f}, color(239, 83, 104));
      }
    }
  }

  const SpriteAnimation playerPose = playerAnimation(player);
  float playerActionProgress = -1.0f;
  if (playerPose == SpriteAnimation::Bite || playerPose == SpriteAnimation::Tail ||
      playerPose == SpriteAnimation::Sweep || playerPose == SpriteAnimation::Claw) {
    playerActionProgress = 1.0f - player.attackTimer / 0.36f;
  } else if (playerPose == SpriteAnimation::Defeat) {
    playerActionProgress = 1.0f - player.defeatTimer / DEFEAT_ANIMATION_DURATION;
  }
  const int playerFrame =
      animationFrame(playerPose, time, 0.0f, playerActionProgress);
  DrawBillboard(camera, dinoSprite.texture,
                spriteFrameSource(dinoSprite, playerPose, playerFrame),
                Vector3{player.position.x, player.position.y + 1.55f,
                        player.position.z},
                3.1f,
                3.1f,
                (player.invulnerability > 0.0f &&
                 static_cast<int>(time * 16.0f) % 2 == 0)
                    ? Fade(WHITE, 0.45f)
                    : WHITE);
  drawAttackEffect(player);

  const float finishProgress = COURSE_LENGTH;
  if (std::fabs(progress - finishProgress) < 170.0f) {
    drawCube(Vector3{0.0f, 3.2f, -finishProgress},
             Vector3{10.0f, 6.4f, 0.6f}, color(230, 191, 87));
    drawCube(Vector3{0.0f, 6.6f, -finishProgress},
             Vector3{10.0f, 0.5f, 0.8f}, color(230, 191, 87));
  }
  EndMode3D();
}

}  // namespace

int main() {
  const bool smokeTest = std::getenv("PROJECT_DINO_SMOKE_TEST") != nullptr;
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Project Dino — Bramblewake");
  SetTargetFPS(60);
  SetExitKey(KEY_NULL);

  AnimatedSprite dinoSprite = makeDinoSprite();
  AnimatedSprite krakenSprite = makeKrakenSprite();

  std::vector<Platform> platforms;
  std::vector<Trap> traps;
  std::vector<EnemyWave> waves;
  std::vector<ClimbWall> walls;
  std::vector<Enemy> enemies;
  buildCourse(platforms, traps, waves, walls);

  Player player;
  GameState state = GameState::Title;
  float elapsed = 0.0f;
  int activeWave = -1;
  float toastTimer = 0.0f;
  char toast[128] = "";
  bool smokeTestStateReported = false;
  GameState smokeTestReportedState = GameState::Title;
  Camera3D camera{};
  camera.position = Vector3{0.0f, 4.2f, 8.0f};
  camera.target = Vector3{0.0f, 1.4f, -4.0f};
  camera.up = Vector3{0.0f, 1.0f, 0.0f};
  camera.fovy = 58.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  while (!WindowShouldClose()) {
    const float dt = std::min(GetFrameTime(), 0.04f);
    const float time = static_cast<float>(GetTime());

    if (state == GameState::Title) {
      if (IsKeyPressed(KEY_ENTER)) {
        startGame(player, enemies, waves, elapsed, activeWave, state, toast, toastTimer);
      }
    } else if (state == GameState::Won) {
      if (IsKeyPressed(KEY_ENTER)) {
        state = GameState::Title;
      }
    } else if (state == GameState::Paused) {
      if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
        state = GameState::Playing;
      }
    } else if (state == GameState::Playing) {
      elapsed += dt;
      player.attackCooldown = std::max(0.0f, player.attackCooldown - dt);
      player.attackTimer = std::max(0.0f, player.attackTimer - dt);
      player.invulnerability = std::max(0.0f, player.invulnerability - dt);
      player.hurtTimer = std::max(0.0f, player.hurtTimer - dt);
      player.defeatTimer = std::max(0.0f, player.defeatTimer - dt);
      player.comboTimer = std::max(0.0f, player.comboTimer - dt);
      toastTimer = std::max(0.0f, toastTimer - dt);
      for (Enemy& enemy : enemies) {
        enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);
        enemy.hurtFlash = std::max(0.0f, enemy.hurtFlash - dt);
        enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);
        enemy.defeatTimer = std::max(0.0f, enemy.defeatTimer - dt);
      }

      if (IsKeyPressed(KEY_ESCAPE)) {
        state = GameState::Paused;
      }
      if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
      }

      const float progress = -player.position.z;
      const int checkpointIndex = std::min(5, static_cast<int>(progress / 1200.0f));
      if (checkpointIndex > player.checkpoint) {
        player.checkpoint = checkpointIndex;
        setToast(toast, sizeof(toast), "CHECKPOINT REACHED", toastTimer);
      }

      for (int i = 0; i < static_cast<int>(waves.size()); ++i) {
        EnemyWave& wave = waves[static_cast<size_t>(i)];
        if (!wave.active && !wave.cleared && progress >= wave.progress - 34.0f) {
          wave.active = true;
          activeWave = i;
          spawnWave(enemies, wave, i);
          setToast(toast, sizeof(toast),
                   wave.boss ? "THE KRAKEN BOD GUARDIAN BLOCKS THE GOAL"
                             : "AMBUSH! CLEAR THE KRAKEN BODS",
                   toastTimer);
          break;
        }
      }

      float moveX = 0.0f;
      float moveForward = 0.0f;
      if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) moveX -= 1.0f;
      if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) moveX += 1.0f;
      if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) moveForward += 1.0f;
      if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) moveForward -= 1.0f;
      if (player.defeatTimer > 0.0f) {
        moveX = 0.0f;
        moveForward = 0.0f;
      }
      const float previousPlayerX = player.position.x;
      const float previousPlayerZ = player.position.z;

      bool touchingWall = false;
      float wallProgress = 0.0f;
      float wallTop = 0.0f;
      for (const ClimbWall& wall : walls) {
        if (std::fabs(player.position.z + wall.progress) < 1.45f &&
            std::fabs(player.position.x) < 5.2f) {
          touchingWall = true;
          wallProgress = wall.progress;
          wallTop = wall.height;
          break;
        }
      }

      if (touchingWall && player.position.y < wallTop && IsKeyDown(KEY_E) &&
          player.defeatTimer <= 0.0f) {
        player.climbing = true;
        player.position.z = -wallProgress + 1.45f;
        player.velocity.y = 0.0f;
        player.position.y = std::min(wallTop + 0.4f, player.position.y + 4.0f * dt);
        player.grounded = false;
      } else {
        if (player.climbing && (!touchingWall || player.position.y >= wallTop)) {
          player.climbing = false;
          player.position.y = std::max(player.position.y, wallTop + 0.25f);
          player.velocity.y = 0.0f;
        }

        const float oldY = player.position.y;
        player.position.x += moveX * PLAYER_SPEED * dt;
        if (touchingWall && player.position.y < wallTop && moveForward > 0.0f) {
          player.position.z = -wallProgress + 1.45f;
        } else {
          player.position.z -= moveForward * PLAYER_SPEED * dt;
        }
        player.position.x = clampf(player.position.x, -12.0f, 12.0f);

        if (IsKeyPressed(KEY_SPACE) && player.grounded &&
            player.defeatTimer <= 0.0f) {
          player.velocity.y = 9.7f;
          player.grounded = false;
        }
        player.velocity.y -= GRAVITY * dt;
        player.position.y += player.velocity.y * dt;

        bool landed = false;
        bool onHazardSurface = false;
        const float ground = platformTopAt(platforms, player.position.x,
                                           player.position.z, oldY,
                                           player.position.y, landed,
                                           onHazardSurface);
        if (landed && player.velocity.y <= 0.0f) {
          player.position.y = ground;
          player.velocity.y = 0.0f;
          player.grounded = true;
          if (onHazardSurface) {
            damagePlayer(player, 3.5f, toast, sizeof(toast), toastTimer);
          }
        } else {
          player.grounded = false;
        }
      }
      player.moving = player.defeatTimer <= 0.0f &&
                      (std::fabs(player.position.x - previousPlayerX) > 0.001f ||
                       std::fabs(player.position.z - previousPlayerZ) > 0.001f ||
                       player.climbing);

      if (player.defeatTimer <= 0.0f && IsKeyPressed(KEY_J)) {
        beginAttack(player, Attack::Bite, enemies, toast, sizeof(toast), toastTimer);
      } else if (player.defeatTimer <= 0.0f && IsKeyPressed(KEY_K)) {
        beginAttack(player, Attack::Tail, enemies, toast, sizeof(toast), toastTimer);
      } else if (player.defeatTimer <= 0.0f && IsKeyPressed(KEY_L)) {
        beginAttack(player, Attack::Sweep, enemies, toast, sizeof(toast), toastTimer);
      } else if (player.defeatTimer <= 0.0f && IsKeyPressed(KEY_U)) {
        beginAttack(player, Attack::Claw, enemies, toast, sizeof(toast), toastTimer);
      }

      const float currentProgress = -player.position.z;
      if (activeWave >= 0 && activeWave < static_cast<int>(waves.size())) {
        const EnemyWave& wave = waves[static_cast<size_t>(activeWave)];
        if (wave.active && !wave.cleared) {
          const float barrierProgress = wave.progress + 18.0f;
          if (currentProgress > barrierProgress &&
              std::fabs(player.position.x) < 6.8f) {
            player.position.z = -(barrierProgress - 0.7f);
          }
        }
      }

      for (Enemy& enemy : enemies) {
        if (!enemy.alive) {
          continue;
        }
        enemy.moving = false;
        const float dx = player.position.x - enemy.position.x;
        const float dz = player.position.z - enemy.position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance > 3.4f) {
          const float speed = enemy.boss ? 2.7f : 3.3f;
          enemy.position.x += (distance > 0.01f ? dx / distance : 0.0f) * speed * dt;
          enemy.position.z += (distance > 0.01f ? dz / distance : 0.0f) * speed * dt;
          enemy.position.x = clampf(enemy.position.x, -5.0f, 5.0f);
          enemy.moving = true;
        } else if (enemy.attackCooldown <= 0.0f && player.position.y < 2.2f) {
          damagePlayer(player, enemy.boss ? 18.0f : 12.0f,
                       toast, sizeof(toast), toastTimer);
          enemy.attackCooldown = enemy.boss ? 0.75f : 1.05f;
          enemy.attackTimer = ENEMY_ATTACK_ANIMATION_DURATION;
        }
      }

      if (activeWave >= 0 && activeWave < static_cast<int>(waves.size())) {
        EnemyWave& wave = waves[static_cast<size_t>(activeWave)];
        bool anyAlive = false;
        for (const Enemy& enemy : enemies) {
          if (enemy.alive && enemy.wave == activeWave) {
            anyAlive = true;
            break;
          }
        }
        if (wave.active && !anyAlive) {
          wave.active = false;
          wave.cleared = true;
          setToast(toast, sizeof(toast),
                   wave.boss ? "GUARDIAN DEFEATED — REACH THE GOAL"
                             : "PATH CLEAR — KEEP MOVING",
                   toastTimer);
          activeWave = -1;
        }
      }

      for (const Trap& trap : traps) {
        if (std::fabs(trap.progress - currentProgress) > 3.5f) {
          continue;
        }
        const float hazardX = trap.moving
                                  ? std::sin(time * 2.1f + trap.phase) * 4.0f
                                  : trap.x;
        if (distance2D(player.position.x, player.position.z,
                       hazardX, -trap.progress) < trap.radius &&
            player.position.y < 1.1f) {
          damagePlayer(player, trap.moving ? 18.0f : 13.0f,
                       toast, sizeof(toast), toastTimer);
        }
      }

      if (player.position.y < -8.0f) {
        player.position = Vector3{0.0f, 0.0f, -player.checkpoint * 1200.0f};
        player.velocity = Vector3{0.0f, 0.0f, 0.0f};
        player.health = std::max(50.0f, player.health - 20.0f);
        setToast(toast, sizeof(toast), "FALL! RETURNED TO CHECKPOINT", toastTimer);
      }

      if (player.health <= 0.0f) {
        player.health = 100.0f;
        player.defeatTimer = DEFEAT_ANIMATION_DURATION;
        player.hurtTimer = 0.0f;
        player.attackTimer = 0.0f;
        player.attack = Attack::None;
        player.invulnerability = 0.9f;
        player.position = Vector3{0.0f, 0.0f, -player.checkpoint * 1200.0f};
        player.velocity = Vector3{0.0f, 0.0f, 0.0f};
        for (Enemy& enemy : enemies) {
          if (enemy.wave >= 0 && enemy.wave < static_cast<int>(waves.size()) &&
              waves[static_cast<size_t>(enemy.wave)].active) {
            enemy.alive = false;
          }
        }
        if (activeWave >= 0 && activeWave < static_cast<int>(waves.size())) {
          waves[static_cast<size_t>(activeWave)].active = false;
        }
        activeWave = -1;
        setToast(toast, sizeof(toast), "RECOVERED AT THE LAST CHECKPOINT", toastTimer);
      }

      if (currentProgress >= COURSE_LENGTH - 4.0f &&
          (activeWave < 0 || waves[static_cast<size_t>(activeWave)].cleared)) {
        state = GameState::Won;
      }

      const float targetCamY = player.position.y + 4.4f;
      camera.position = Vector3{player.position.x + 0.65f, targetCamY,
                                player.position.z + 8.2f};
      camera.target = Vector3{player.position.x, player.position.y + 1.35f,
                              player.position.z - 5.4f};
    }

    BeginDrawing();
    ClearBackground(color(74, 126, 119));
    if (state == GameState::Title) {
      showTitle();
    } else if (state == GameState::Playing || state == GameState::Paused ||
               state == GameState::Won) {
      const float progress = clampf(-player.position.z, 0.0f, COURSE_LENGTH);
      drawWorld(camera, player, platforms, traps, walls, waves, activeWave,
                enemies, dinoSprite, krakenSprite, progress, time);
      drawHud(player, elapsed, progress, activeWave, waves, toast, toastTimer);

      if (state == GameState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      color(8, 17, 20, 180));
        drawPanel(GetScreenWidth() / 2 - 190, GetScreenHeight() / 2 - 73,
                  380, 146, color(16, 35, 35, 235), color(210, 190, 120));
        drawTextShadow("PAUSED", GetScreenWidth() / 2 - 65,
                       GetScreenHeight() / 2 - 41, 35, color(244, 213, 134));
        drawTextShadow("Press ESC or ENTER to return",
                       GetScreenWidth() / 2 - 141,
                       GetScreenHeight() / 2 + 17, 17, RAYWHITE);
      } else if (state == GameState::Won) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      color(8, 17, 20, 185));
        drawPanel(GetScreenWidth() / 2 - 250, GetScreenHeight() / 2 - 115,
                  500, 230, color(16, 35, 35, 240), color(225, 195, 111));
        drawTextShadow("THE JUNGLE STANDS", GetScreenWidth() / 2 - 185,
                       GetScreenHeight() / 2 - 74, 33, color(242, 211, 130));
        drawTextShadow("Guardian defeated. The invasion is broken.",
                       GetScreenWidth() / 2 - 198,
                       GetScreenHeight() / 2 - 16, 17, RAYWHITE);
        drawTextShadow(TextFormat("TIME  %02d:%02d    KRAKEN BODS  %d",
                                 static_cast<int>(elapsed) / 60,
                                 static_cast<int>(elapsed) % 60, player.kills),
                       GetScreenWidth() / 2 - 191,
                       GetScreenHeight() / 2 + 23, 17, color(211, 220, 189));
        drawTextShadow("Press ENTER to return to the level select",
                       GetScreenWidth() / 2 - 187,
                       GetScreenHeight() / 2 + 69, 16, color(244, 213, 134));
      }
    }
    EndDrawing();

    // The Windows ZIP smoke test observes the window title only after a frame
    // for the corresponding state has been rendered.
    if (smokeTest &&
        (!smokeTestStateReported || state != smokeTestReportedState)) {
      const char* smokeTitle = "PROJECT_DINO_SMOKE_OTHER";
      if (state == GameState::Title) {
        smokeTitle = "PROJECT_DINO_SMOKE_TITLE";
      } else if (state == GameState::Playing) {
        smokeTitle = "PROJECT_DINO_SMOKE_RUNNING";
      }
      SetWindowTitle(smokeTitle);
      smokeTestReportedState = state;
      smokeTestStateReported = true;
    }
  }

  UnloadTexture(dinoSprite.texture);
  UnloadTexture(krakenSprite.texture);
  CloseWindow();
  return 0;
}
