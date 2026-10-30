# Project Dino

A native C++ third-person action-game test run. The game is built with C++17 and
SDL2's software renderer; sprites are generated in code as pixel art and
displayed as camera-facing billboards inside a block-built 3D jungle.

## Run

```sh
cmake -S game -B game/build -DCMAKE_BUILD_TYPE=Release
cmake --build game/build -j2
./game/build/project-dino
```

The Replit desktop preview runs those same commands in its game window.

## Build for Windows

On Windows, install Visual Studio 2022 with the **Desktop development with C++**
workload, CMake 3.20 or newer, and Git. From a PowerShell prompt in the project
root, run:

```powershell
cmake -S game -B game/build-windows -G "Visual Studio 17 2022" -A x64
cmake --build game/build-windows --config Release --parallel
```

CMake downloads the pinned SDL2 2.32.10 source and builds it with the game. The
finished, 64-bit Windows build is in
`game\build-windows\Release\project-dino.exe`; CMake copies `SDL2.dll` beside
it. Launch the `.exe` directly from that directory. The MSVC C++ runtime is
linked statically, so the SDL2 DLL is the only bundled runtime file required.
The first configure needs an internet connection to fetch SDL2.

To create a shareable ZIP after building, run:

```powershell
cmake --build game/build-windows --config Release --target package-windows --parallel
```

The archive is written to
`game\build-windows\project-dino-windows-x64.zip`. It contains
`project-dino.exe`, `SDL2.dll`, and SDL2's `LICENSE.txt` as
`SDL2-LICENSE.txt`. Extract it to a folder and launch `project-dino.exe` there;
the game needs no separate SDL2 installation or other runtime setup. For example:

```powershell
Expand-Archive -Path .\game\build-windows\project-dino-windows-x64.zip -DestinationPath .\ProjectDino
& .\ProjectDino\project-dino.exe
```

## Controls

- **WASD / arrow keys** — move
- **Space** — jump
- **E** — climb the marked root walls
- **J** — bite
- **K** — tail whip
- **L** — leg sweep
- **U** — repeated claw-kick
- **Esc** — pause
- **F11** — toggle fullscreen

## Stage

**Bramblewake Trail** is one connected, 7,200-unit course through six named
jungle acts. It includes narrow ledges, gaps with offset stepping stones, thorn
beds, rolling log traps, climbable root walls, checkpoint platforms, 36
mandatory kraken-bod ambush gates, and a final guardian. The long route and
required fights are paced as a roughly 30-minute first-clear target; player
skill and time spent fighting can change the actual run time.

This is an early gameplay test run, not a finished commercial game. Art is
procedural placeholder pixel art, and the first build focuses on the requested
movement, combat, hazard, checkpoint, and course loop.
