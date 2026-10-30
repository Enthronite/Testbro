cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(ProjectDino LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  include(FetchContent)

  # Build the pinned SDL2 release alongside the game so no SDL SDK install is needed.
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.32.10
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL2)

  add_executable(project-dino WIN32 src/main.cpp)
  target_link_libraries(project-dino PRIVATE SDL2::SDL2main SDL2::SDL2)
  add_custom_command(
    TARGET project-dino POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>"
            "$<TARGET_FILE_DIR:project-dino>"
  )

  FetchContent_GetProperties(SDL2 SOURCE_DIR SDL2_SOURCE_DIR)
  set(WINDOWS_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/windows-package")
  set(WINDOWS_PACKAGE_ARCHIVE
      "${CMAKE_CURRENT_BINARY_DIR}/project-dino-windows-x64.zip")
  file(MAKE_DIRECTORY "${WINDOWS_PACKAGE_DIR}")
  add_custom_target(package-windows
    COMMAND ${CMAKE_COMMAND} -E rm -f
            "${WINDOWS_PACKAGE_DIR}/project-dino.exe"
            "${WINDOWS_PACKAGE_DIR}/SDL2.dll"
            "${WINDOWS_PACKAGE_DIR}/SDL2-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:project-dino>"
            "${WINDOWS_PACKAGE_DIR}/project-dino.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>"
            "${WINDOWS_PACKAGE_DIR}/SDL2.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_SOURCE_DIR}/LICENSE.txt"
            "${WINDOWS_PACKAGE_DIR}/SDL2-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${WINDOWS_PACKAGE_ARCHIVE}"
    COMMAND ${CMAKE_COMMAND} -E tar cfv "${WINDOWS_PACKAGE_ARCHIVE}"
            --format=zip project-dino.exe SDL2.dll SDL2-LICENSE.txt
    DEPENDS project-dino
    WORKING_DIRECTORY "${WINDOWS_PACKAGE_DIR}"
    COMMENT "Packaging Project Dino for Windows"
    VERBATIM
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

  add_executable(project-dino src/main.cpp)
  target_link_libraries(project-dino PRIVATE PkgConfig::SDL2)
endif()

if(MSVC)
  target_compile_options(project-dino PRIVATE /W4 /permissive-)
else()
  target_compile_options(project-dino PRIVATE -Wall -Wextra -Wpedantic)
endif()

