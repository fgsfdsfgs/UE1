## What?

Unreal Engine 1 v200 source with modifications to make it run on modern systems.  
Requires assets from the original Unreal v200 retail release or from the v205 demo. Other versions have not been tested.

## Changes from original source

* Added SDL2 windowing/client driver (NSDLDrv).
* Added GLES2 and fixed pipeline GL graphics drivers (NOpenGLESDrv and NOpenGLDrv).
* Added OpenAL + libxmp audio driver (NOpenALDrv).
* Added GCC support and fixed a bunch of related bugs.
* Supported platforms: Windows (x86), Linux (x86, ARM32) and PSVita (ARM32).
* Editor UI is not supported.

## Running

### Linux and Windows
1. Install the original retail v200 release of Unreal or the v205 demo.
2. Copy over the new files:
   * If you downloaded a ZIP from the Releases section:
     1. Unzip said ZIP to the `Unreal` folder. Overwrite everything.
   * If you built the game yourself:
     1. Copy the .dll/.so/.exe/.bin files you built to `Unreal/System`. Overwrite everything.
     2. Copy the contents of `Engine/Config` to `Unreal/System`. Overwrite everything.
3. Run `System/Unreal.exe`.

### PSVita
1. Ensure you have libshacccg installed.
2. Install the original retail v200 release of Unreal or the v205 demo onto your PC.
3. Copy the contents of the `Unreal` folder to `ux0:/data/unreal/` on your PSVita.
4. Copy the `unreal` folder from `unreal-arm-psvita-gcc.zip` to `ux0:/data/`. Overwrite everything.
5. Install `unreal.vpk` from `unreal-arm-psvita-gcc.zip`.
6. Run Unreal.

## Building

### Windows x86 (MSYS2/MinGW)
1. Install MSYS2.
2. Open the `MINGW32` prompt. **Do not** use the `MINGW64` or `MSYS` prompts.
3. Install dependencies: `pacman -S git make mingw-w64-i686-toolchain mingw-w64-i686-cmake mingw-w64-i686-SDL2 mingw-w64-i686-openal mingw-w64-i686-libxmp`
4. Build:
   ```
   cmake -G"Unix Makefiles" -Bbuild Source
   cmake --build build -j4 -- -O && cmake --install build
   ```
5. The resulting files will be in `build/RelWithDebInfo` by default.

### Windows x86 (Visual Studio)
1. Install VS2019 or VS2022. Dependencies are included in the repo.
2. Build:
   ```
   cmake -Bbuild -G"Visual Studio 16 2019" -A Win32 Source # or -G"Visual Studio 17 2022"
   cmake --build build && cmake --install build --config RelWithDebInfo
   ```
3. The resulting files will be in `build/RelWithDebInfo` by default.

### Linux x86
1. Install git, make, cmake, gcc, g++, sdl2, libopenal, libxmp.
   * If cross-compiling from x86_64, also install 32-bit versions of the libraries and gcc-multilib/g++-multilib.
   * On Debian x86_64 this process looks something like this:
     ```
     sudo dpkg --add-architecture i386
     sudo apt-get -y update
     sudo apt-get -y install git gcc g++ gcc-multilib g++-multilib make cmake
     sudo apt-get -y install libsdl2-dev libopenal-dev libxmp-dev libsdl2-dev:i386 libopenal-dev:i386 libxmp-dev:i386
     ```
2. Build:
   ```
   cmake -G"Unix Makefiles" -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -Bbuild Source # if on x86_64
   cmake -G"Unix Makefiles" -Bbuild Source # if on i686
   cmake --build build -j4 -- -O && cmake --install build
   ```
3. The resulting files will be in `build/RelWithDebInfo` by default.

### Linux ARM
1. Install git, make, cmake, gcc, g++, sdl2, libopenal, libxmp.
   * If cross-compiling from ARM64, also install armhf versions of the libraries and arm-linux-gnueabihf-gcc/g++.
   * On Debian x86_64 or ARM64 this process looks something like this:
     ```
     sudo dpkg --add-architecture armhf
     sudo apt-get -y update
     sudo apt-get -y install git gcc g++ crossbuild-essential-armhf make cmake
     sudo apt-get -y install libsdl2-dev libopenal-dev libxmp-dev libsdl2-dev:armhf libopenal-dev:armhf libxmp-dev:armhf
     ```
2. Build:
   ```
   cmake -G"Unix Makefiles" -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc-12 -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++-12 -Bbuild Source
   cmake --build build -j4 -- -O && cmake --install build
   ```
3. The resulting files will be in `build/RelWithDebInfo` by default.

### PSVita (on Linux or WSL)
1. Install VitaSDK with all VDPM packages and ensure the `VITASDK` environment variable is set and `$VITASDK/bin` is in your `PATH`.
2. Build and install vitaGL:
   ```
   git clone --recursive https://github.com/Rinnegatamante/vitaGL
   make -C vitaGL HAVE_GLSL_SUPPORT=1 CIRCULAR_VERTEX_POOL=2 MATH_SPEEDHACK=1 INDICES_SPEEDHACK=1 PRIMITIVES_SPEEDHACK=1 -j install
   ```
3. Build and install SDL2:
   ```
   git clone --recursive --branch vitagl https://github.com/Northfear/SDL
   pushd SDL
   cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=${VITASDK}/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DVIDEO_VITA_VGL=ON
   cmake --build build -- -j
   cmake --install build
   popd
   ```
4. Build the VPK:
   ```
   cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE="${VITASDK}/share/vita.toolchain.cmake" -Bbuild -DCMAKE_BUILD_TYPE=RelWithDebInfo Source
   cmake --build build -j
   ```
5. The VPK will be in `build/Unreal/`.

## Note

Unreal Engine, Unreal and any related trademarks or copyrights are owned by Epic Games. This repository is not affiliated with or endorsed by Epic Games. 
This is based on the v200 source available elsewhere on the Internet, with assets and third party proprietary libraries removed. 
Do not use for commercial purposes.
