
cmake_minimum_required(VERSION 3.22)

set(PROJ "wl-egl-texture")
project(${PROJ})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER "clang")
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-fsycl -fsycl-targets=nvptx64-nvidia-cuda -Wno-unknown-cuda-version")

#############################################################################
include(FetchContent)

FetchContent_Populate(aux
  GIT_REPOSITORY https://github.com/mavrogato/aux
  GIT_TAG main)

FetchContent_Populate(wl-protocol
  GIT_REPOSITORY https://gitlab.freedesktop.org/wayland/wayland-protocols
  GIT_TAG main)
set(XDG_WM_BASE_PROTOCOL ${wl-protocol_SOURCE_DIR}/stable/xdg-shell/xdg-shell.xml)
add_custom_command(
  OUTPUT xdg-wm-base-private.c
  COMMAND wayland-scanner client-header ${XDG_WM_BASE_PROTOCOL} xdg-wm-base-client.h
  COMMAND wayland-scanner private-code  ${XDG_WM_BASE_PROTOCOL} xdg-wm-base-private.c)

include_directories(
  ${aux_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR})

#############################################################################

add_library(${PROJ}-protocol
  ${CMAKE_CURRENT_BINARY_DIR}/xdg-wm-base-private.c)

target_compile_options(${PROJ}-protocol
  PRIVATE
  -Wall
  -Wextra
  -Werror)

add_executable(${PROJ}
  ${PROJ}.cc)

target_compile_options(${PROJ}
  PRIVATE
  -std=libc++
  -Wall
  -Wextra
  -Werror)

target_link_libraries(${PROJ}
  PRIVATE
  ${PROJ}-protocol
  EGL
  GL
  wayland-client)

add_custom_target(run
  DEPENDS ${PROJ}
  COMMAND ${PROJ})
