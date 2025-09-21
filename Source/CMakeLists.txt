cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(UE1 CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
  endif()
endif()

# Options

option(USE_SDL "Use NSDLDrv instead of WinDrv" ON)
option(BUILD_EDITOR "Build Editor (GUI support requires Windows)" ON)
option(BUILD_SOFTDRV "Build SoftDrv (x86/MSVC only)" OFF)
option(BUILD_NOPENGLDRV "Build NOpenGLDrv" OFF)
option(BUILD_NOPENGLESDRV "Build NOpenGLESDrv" ON)
option(BUILD_NULLSOUNDDRV "Build SoundDrv (Null driver)" ON)
option(BUILD_NOPENALDRV "Build NOpenALDrv" ON)
option(BUILD_WINDRV "Build WinDrv" OFF)
option(BUILD_STATIC "Link everything into a single binary" OFF)

if(VITA)
  set(BUILD_STATIC ON)
endif()

if(BUILD_STATIC)
  if(MSVC)
    message(FATAL_ERROR "Static mode is not supported with MSVC")
  endif()
  set(LIB_TYPE STATIC)
  add_definitions(-DUNREAL_STATIC)
  message(STATUS "Static mode enabled")
else()
  set(LIB_TYPE SHARED)
endif()

# Determine target arch and get normalized arch string

include(TargetArch)
target_architecture(TARGET_ARCH)

set(TARGET_IS_X86 FALSE)
set(TARGET_IS_ARM FALSE)
set(TARGET_IS_64BIT FALSE)
set(TARGET_IS_BIG_ENDIAN FALSE)

if(TARGET_ARCH MATCHES "arm")
  set(TARGET_IS_ARM TRUE)
  if(TARGET_ARCH MATCHES "64")
    # arm64/aarch64
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "arm64")
  else()
    # arm
    set(TARGET_ARCH "arm")
  endif()
elseif((TARGET_ARCH MATCHES "86") OR (TARGET_ARCH STREQUAL "ia64"))
  set(TARGET_IS_X86 TRUE)
  if(TARGET_ARCH MATCHES "64")
    # x86_64/amd64
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "x86_64")
  else()
    # x86
    set(TARGET_ARCH "x86")
  endif()
endif()

message(STATUS "Target arch: ${TARGET_ARCH}")
message(STATUS "64-bit: ${TARGET_IS_64BIT}")

if(TARGET_IS_64BIT)
  if(TARGET_IS_X86 AND NOT MSVC)
    message(STATUS "Building x86 binary with x86_64 compiler")
    set(TARGET_IS_64BIT FALSE)
    set(TARGET_ARCH "x86")
    add_compile_options(-m32)
  else()
    message(FATAL_ERROR "64-bit platforms are currently not supported. If you're building on Windows with MSVC, try -A Win32.")
  endif()
endif()

# Determine target OS/platform

set(TARGET_IS_WINDOWS FALSE)
set(TARGET_IS_UNIX FALSE)
if(WIN32)
  set(TARGET_IS_WINDOWS TRUE)
else()
  # TODO
  set(TARGET_IS_UNIX TRUE)
endif()

message(STATUS "Windows target: ${TARGET_IS_WINDOWS}")

# Common compiler options

if(TARGET_IS_64BIT)
  add_definitions(-DPLATFORM_64BIT)
else()
  add_definitions(-DPLATFORM_32BIT)
endif()

if(TARGET_IS_X86)
  add_definitions(-DPLATFORM_X86)
elseif(TARGET_IS_ARM)
  add_definitions(-DPLATFORM_ARM)
  add_compile_options(-fsigned-char -fno-short-enums)
  add_link_options(-fno-short-enums)
  if(VITA)
    add_compile_options(-mno-unaligned-access -fno-optimize-sibling-calls)
    add_link_options(-mno-unaligned-access -Wl,-q)
  endif()
endif()

if(TARGET_IS_BIG_ENDIAN)
  add_definitions(-DPLATFORM_BIG_ENDIAN)
else()
  add_definitions(-DPLATFORM_LITTLE_ENDIAN)
endif()

if(TARGET_IS_WINDOWS)
  add_definitions(-DPLATFORM_WIN32)
  add_definitions(-DWIN32)
  add_definitions(-DWINDOWS_IGNORE_PACKING_MISMATCH)
else()
  # TODO
  add_definitions(-DPLATFORM_POSIX)
  if(VITA)
    add_definitions(-DPLATFORM_PSVITA)
  else()
    add_definitions(-DPLATFORM_CASE_SENSITIVE_FS)
    set(CMAKE_EXECUTABLE_SUFFIX ".bin")
  endif()
endif()

if(USE_SDL)
  add_definitions(-DPLATFORM_SDL)
endif()

if(MSVC)
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with MSVC")
  add_definitions(-DPLATFORM_MSVC)
  add_compile_options(/nologo /fp:precise /analyze- /MP /MD /Gd /Gy /GS /Gm- /EHsc /Zi /Zc:wchar_t /Zc:inline /Zp4)
  add_compile_options(/wd4291 /wd4297)
  if(TARGET_IS_64BIT)
    set(MSVC_LIBDIR "Win64")
  else()
    set(MSVC_LIBDIR "Win32")
  endif()
else()
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with GCC or Clang")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG)
    add_compile_options(-O0)
  else()
    add_compile_options(-O2)
  endif()
  add_compile_options(-ffunction-sections -fno-strict-aliasing)
  add_compile_options(-Wno-write-strings)
endif()

# Dependencies

if(USE_SDL)
  if(MSVC)
    if(NOT DEFINED SDL2_INCLUDE_DIR)
      set(SDL2_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../Thirdparty/SDL2/include")
    endif()
    if(NOT DEFINED SDL2_LIBRARY)
      set(SDL2_LIBRARY "${CMAKE_SOURCE_DIR}/../Thirdparty/SDL2/lib/${MSVC_LIBDIR}/SDL2.lib")
    endif()
  else()
    # Won't bother with find_package as we're probably cross compiling.
    if(NOT DEFINED SDL2_INCLUDE_DIR)
      set(SDL2_INCLUDE_DIR )
    endif()
    if(NOT DEFINED SDL2_LIBRARY)
      set(SDL2_LIBRARY "-lSDL2")
    endif()
  endif()
endif()

if(BUILD_NOPENALDRV)
  if(MSVC)
    if(NOT DEFINED OPENAL_INCLUDE_DIR)
      set(OPENAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../Thirdparty/openal-soft/include")
    endif()
    if(NOT DEFINED OPENAL_LIBRARY)
      set(OPENAL_LIBRARY "${CMAKE_SOURCE_DIR}/../Thirdparty/openal-soft/lib/${MSVC_LIBDIR}/OpenAL32.lib")
    endif()
    if(NOT DEFINED LIBXMP_INCLUDE_DIR)
      set(LIBXMP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../Thirdparty/libxmp-lite/include/libxmp-lite")
    endif()
    if(NOT DEFINED LIBXMP_LIBRARY)
      set(LIBXMP_LIBRARY "${CMAKE_SOURCE_DIR}/../Thirdparty/libxmp-lite/lib/${MSVC_LIBDIR}/libxmp-lite.lib")
    endif()
  else()
    # Won't bother with find_package as we're probably cross compiling.
    if(NOT DEFINED OPENAL_INCLUDE_DIR)
      set(OPENAL_INCLUDE_DIR )
    endif()
    if(NOT DEFINED OPENAL_LIBRARY)
      set(OPENAL_LIBRARY "-lopenal")
    endif()
    if(NOT DEFINED LIBXMP_INCLUDE_DIR)
      set(LIBXMP_INCLUDE_DIR )
    endif()
    if(NOT DEFINED LIBXMP_LIBRARY)
      set(LIBXMP_LIBRARY "-lxmp")
    endif()
  endif()
endif()

# Subprojects

set(INSTALL_TARGETS Core Engine Render IpDrv Fire Unreal)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_subdirectory(Core)
add_subdirectory(Engine)
add_subdirectory(Render)
add_subdirectory(IpDrv)
add_subdirectory(Fire)

if(USE_SDL)
  # SDL2
  add_subdirectory(NSDLDrv)
  list(APPEND INSTALL_TARGETS NSDLDrv)
else()
  # archaic WinAPI shit
  set(BUILD_WINDRV ON)
  set(BUILD_SOFTDRV ON)
  set(BUILD_NOPENGLDRV OFF)
  set(BUILD_NOPENGLESDRV OFF)
endif()

if(BUILD_WINDRV)
  add_subdirectory(Window)
  add_subdirectory(WinDrv)
  list(APPEND INSTALL_TARGETS Window WinDrv)
endif()

if(BUILD_SOFTDRV)
  add_subdirectory(SoftDrv)
  list(APPEND INSTALL_TARGETS SoftDrv)
endif()

if(BUILD_NOPENGLDRV)
  add_subdirectory(NOpenGLDrv)
  list(APPEND INSTALL_TARGETS NOpenGLDrv)
endif()

if(BUILD_NOPENGLESDRV)
  add_subdirectory(NOpenGLESDrv)
  list(APPEND INSTALL_TARGETS NOpenGLESDrv)
endif()

if(BUILD_NULLSOUNDDRV)
  add_subdirectory(SoundDrv)
  list(APPEND INSTALL_TARGETS SoundDrv)
endif()

if(BUILD_NOPENALDRV)
  add_subdirectory(NOpenALDrv)
  list(APPEND INSTALL_TARGETS NOpenALDrv)
endif()

if(BUILD_EDITOR)
  # GUI requires WinDrv
  add_subdirectory(Editor)
  list(APPEND INSTALL_TARGETS Editor)
endif()

add_subdirectory(Unreal)

# assemble all modules and pdbs in one dir

set(INSTALL_FILES )

foreach(TGT ${INSTALL_TARGETS})
  list(APPEND INSTALL_FILES "$<TARGET_FILE:${TGT}>")
  if(MSVC)
    list(APPEND INSTALL_FILES "$<TARGET_PDB_FILE:${TGT}>")
  endif()
endforeach()

install(FILES ${INSTALL_FILES} DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
