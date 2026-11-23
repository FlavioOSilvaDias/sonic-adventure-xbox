# Xbox

The port of this byte-matching decompilation of *Sonic Adventure* (JP, HDR-0001, v1.007) to the original Xbox with [nxdk](https://github.com/XboxDev/nxdk), by way of a native build. `dependencies.md` lists the software to install.

- [Goal and limits](#goal-and-limits)
- [What has been implemented](#what-has-been-implemented)
- [Architecture](#architecture)
- [Target resolution](#target-resolution)
- [New features](#new-features)
- [Known issues](#known-issues)
- [Fixed bugs](#fixed-bugs)
- [Debug flags](#debug-flags)
- [Experimental flags](#experimental-flags)
- [Building](#building)
- [Future plans](#future-plans)
- [How the port was made](#how-the-port-was-made)

## Goal and limits

- **Goal:** the game running natively on a real Xbox, looking and sounding as close to the Dreamcast as possible, while being Xbox-native in how it gets there: no emulation of the Dreamcast's internals where a native design does the job (a GPU renderer instead of the tile accelerator's lists, x87 and SSE math instead of an emulated XMTRX, the APU's voices instead of a software mixer).
- **The game's own code:** the decompilation's C, built for x86, with its math made more native to and compatible with the Xbox: C over the port's own models of the SH-4's FPU instructions (`xbox/include/sh4.h`). No SH-4 code runs on the Xbox.
- **Out of scope:** the VMU selection screen, the internet menu and mail, the hand-off to Dream Passport. Their code compiles and stays unreachable, but are not priorities for this port for now.
- **The matching build is untouched:** `make` and `make check` read nothing the port adds. Every edit to a shared source is under `XBOX`, or `GCC` and `XBOX`.
- **License:** the port links Flycast's GPL-2.0 AICA and ARM7 code, so the port is GPL-2.0. The console's folder, the ISO and the packs hold the game's code and data: for the builder's own console only.

Checks that must keep passing:

- `make check`: "4309 files match SHA256SUMS".
- `xbox/test/all.sh`: 22 SH-4 translation tests, 0 real differences (a zero's sign and a NaN's bits don't count).
- `xbox/test/sh4_sse.sh`: the x87 FIPR and FTRV against the C they replaced, 0 differences.
- Docs: `git add -N XBOX.md && PYTHONPATH=tools:tests python3 -m unittest tests.test_docs; git reset -q XBOX.md`

## What has been implemented

- **The whole game builds and runs:** all 253 images (1ST_READ.BIN and its overlays) linked for i386 at their console addresses and loaded as on the console. Played on a real Xbox: logo, title, menus, Sonic's story, Emerald Coast, Speed Highway, Red Mountain and other stages; saves go to a memory card image on the hard disk.
- **The port layer** (`xbox/port/`) stands in for the SDK's hardware layers: interrupts and timers, disc files, pads, fibers, Kamui's lists and video memory, the renderer, movies, math, the overlay and the reports.
- **Two hosts:** a native PC program (SDL2 and OpenGL, `xbox/host/pc/`) for debugging and headless runs, and the Xbox XBE (nxdk and pbkit, `xbox/host/xbox/`).
- **Graphics on the NV2A:** the PowerVR's lists drawn through pbkit with a vertex program and ten combiner programs; per-pixel table fog; two-sample antialiasing; a subtle bloom in stages; textures converted ahead of time to the NV2A's formats (DXT1/DXT3, swizzled 16-bit for 2D art), with mipmaps where the Dreamcast has them.
- **Models on the NV2A:** a model that needs no clipping is transformed by the NV2A from vertices the drawer writes into an arena (2 × 29,768 vertices, in a room of their own beside the lists' vertex ring). Models that haven't changed are kept in a store and neither shaded nor written again; kept models with normals are lit by the NV2A through a palette texture. A meshset that doesn't fit the arena goes through the lists whole.
- **Translucency:** the translucent list sorted back to front per triangle; alpha-blended strips draw with a separate depth pass; additive strips draw last.
- **Sound:** the disc's own sound driver (masaru.drv) runs, translated ahead of time from ARM to native code, over Flycast's AICA registers and timers. On the Xbox the channels play on the MCPX APU's hardware voices, from samples converted ahead of time into a sound pack. The music's ADX streams are decoded by the game's own ADX library.
- **Movies:** Sofdec's player replaced by pl_mpeg's demuxer and libmpeg2's decoder (its MMX and MMXEXT IDCT and motion compensation, as XBMC used on the Xbox), with the ADX audio as the clock; on the Xbox they play from `movies.pak`, each frame a linear 16-bit texture the host fills from its planes in one pass.
- **Packaging:** `make xbox` updates the console's folder in place: eight files, of which a build changes one or two. `make xbox-disc` creates an ISO, but this is untested at the moment.
- **SH-4 math, original behavior:** Ninja's math bodies, the game's assembly (`math.src`, `vector.src`, `mesh.src` and the rest) and SHC's inline bodies are C in `xbox/`, each checked bit for bit against the original assembly under `qemu-sh4` (`xbox/test/`).

## Architecture

- **Images:** each image is linked for i386 at its console address by `tools/pclink.py` from make dc's link spec. Data keeps its RAM address; code goes to code areas past `0x10000000`. An overlay's unbound name binds to 1ST_READ.BIN's, the port's XMTRX among them (`sh4_xmtrx`, one for the game, as the console's one register).
- **Memory:** the port's RAM is area 3, the console's 16 MB then the port layer's own memory, visible at `0x0c000000`, `0x8c000000`, `0xac000000` and the store queues' `0xe0000000`.
- **`struct host_api`** (`xbox/port/host.h`) is the line between the shared port and the two hosts: files, time, the blank, pads, cards, video, AICA registers, stats.
- **Floats:** SSE single precision in the SH-4's mode (rounding toward zero, flush to zero, denormals as zero), set by each host before `main`; the x87 at 53 bits and rounding toward zero for FIPR's and FTRV's double sums, as Flycast computes them. FSRRA is exact here; FSCA uses the console's own table.
- **`xbox/include/sh4.h`:** the port's XMTRX and its models of FIPR, FTRV, FSRRA, FSCA, FTRC and FMAC. `ninja.h` includes it under `XBOX` in place of `ninja_zam.h`. FIPR and FTRV are x87 asm in `xbox/include/sse/sh4_sse.h`, bit-identical to the C they replaced.
- **The blank** interrupts the game's thread through the port's interrupt entry, as the console's does; the game's speed follows the SH-4's timer, which the port models. On the Xbox the host breaks in from the kernel's clock interrupt, wherever the game is; its trampoline saves the x87/SSE state (FXSAVE) before its own chores, whose sound emulation computes in those registers. Without that, the game resumed on the chores' values, and objects and land pieces dropped out for a frame on the console alone. Host code at interrupt level must not touch x87/SSE outside that save.

## Target resolution

- **640×480**, 4:3, the Dreamcast's own resolution, with **two-sample antialiasing** (a 1280×480 surface resolved to the screen), triple-buffered with vsync, at the game's 30 frames a second.
- **16:9:** the port can squeeze the picture for a widescreen TV (`HOST_WIDESCREEN`: by default the dashboard's widescreen setting; 0 off, 1 on; 720p always has it).
- **720p** (`HOST_720P=1`, where the dashboard allows it): the game's 640×480 scene scaled to 1280×720 on a 1280×720 surface copied to the screen, with 16:9 on and no antialiasing (experimental).

## New features

Features the Dreamcast version doesn't have:

- **Right stick camera:** left and right turn the camera as the triggers do, past a 20% dead zone; up and down do nothing.
- **Analog dead zone** on the left stick (12% by default, `HOST_DEADZONE=PERCENT`), as the Xbox joystick tends to drift more than the DC's.
- **Bloom** in stages and fields (`HOST_BLOOM`, `HOST_BLOOM_THRESHOLD`).
- **On-screen performance overlay** (`HOST_OVERLAY=1`, off by default) and **reports** on the pad's Back button (see Debug flags).
- **Return to the dashboard:** both triggers with Start and Back.

## Known issues

- **Land at the draw radius:** a land piece right at the edge of the stage's draw radius can blink for a frame while the camera hovers on that edge, as on the Dreamcast (a mountain's piece on Emerald Coast's last cliff, with the camera pitched down). It is the game's own land selection, left as the console has it.
- **The translucent sort** is per triangle, where the PowerVR's is per pixel: crossing triangles can sort wrong.

## Fixed bugs

- **Objects and land vanishing for a frame (console only).** Springs, palms, rocks, rings, item boxes, the land's pieces and the sea dropped out for a single frame, mid-screen, with their steps run; on Emerald Coast up to 23 objects at once, or a piece of the sea drawn black. The PC host never showed it. The cause was the blank's trampoline on the Xbox host (`xbox/host/xbox/host.c`): it ran the host's own chores, the sound's ARM7 and AICA emulation above all, before the port's interrupt entry saved the x87/SSE state, so the chores' float work overwrote whatever the game had in its XMM and x87 registers wherever the blank broke in, and the game resumed on those values (a view test or a matrix wrong for a frame). The PC was spared because Linux saves the FPU state for a signal handler. The trampoline now saves the state with FXSAVE before the chores, runs them with an empty x87 stack and the game's control words, and restores it after. Confirmed on a console at 60 frames a second, with antialiasing off.
- **Code overwritten by Ninja's model clipper (a crash on the HUD or Tikal's light).** `sdk/ninja/njModelGlobal.src`, Ninja's model workspace (`vtx2`, `vtx4`, `_njm_col` and the rest), had no x86 side, so `tools/pclink.py` took its names for functions nothing defined and gave each a 16-byte stub in `1ST_READ.BIN`'s code. A model triangle clipped at the near plane (unlit and textured, as in Lost World) built its corners in `vtx2`, 64 bytes each, over the stubs and on past `image_code_end` into the `.text_last` functions, and `present_sprite` then ran a corner's packet as code. `xbox/sdk/ninja/njModelGlobal.c` gives the workspace in `.bss`, laid out as the `.src` has it. Found with a write watch on the code by the CPU's debug registers.
- **A crash after an event's follow move lost its task.** An event's mover that follows a task (`events/shared/mover.c`) read the task's work through null once the task was destroyed with steps left, as in Mystic Ruins after Egg Hornet (EV0008). The Dreamcast reads its boot ROM at 0x20 and goes on; the Xbox, with nothing mapped at 0, faulted. Under `NONMATCHING` such a move now ends, its target where it last set it.
- **Models drawn black by Ninja's simple drawer (Chao, Knuckles' Emerald shards, Lost World's rotating cube).** `tools/sh2c.py`, which translates Ninja's SH-4 inline assembly to C, loaded the back bank (XMTRX) from `sh4_xmtrx` only for a function using FRCHG, FTRV or an XDn operand. `njModelS.c`'s `SimpleCalcModelLight` reaches XMTRX through FSCHG's pair moves alone, so it read zeros, lit every vertex NaN, and its models drew black on both hosts. FSCHG now counts as a use of the back bank; no other translated function was affected. Confirmed on xemu with a Chao in the garden.
- **Particles blinking (the Egg Carrier's mist in the attract demo, and other 3D sprites).** A sprite that goes into one of the renderer's runs takes its colors, alpha included, from the polygon header before its vertices (`emit_face`, `xbox/engine/sa1/emit.h`). The game's `sprite_draw_3d` (`engine/dc/sprite.c`) sends the vertices first and writes the header after, as `sprite.src` does, so each sprite took whatever that slot last held, an earlier frame's header or another draw's, and drew at a random alpha, often none. Under `XBOX` it now writes the sprite's colors into the header's room before the sends (`emit_face_ahead`); its real header replaces them. `njDrawSprite3D`, which writes its header first, never blinked. Confirmed on the PC host.
- **Movies too slow and juddering.** pl_mpeg's decoder, all C and built at `-Os`, ran the opening at some 20, then 27, of its 30 frames a second, each frame swizzled into a 512 by 512 texture texel by texel. The video is now libmpeg2's (MMX and MMXEXT, as XBMC used on the Xbox), each frame a linear 16-bit texture the host writes in one pass from its planes (host_api's `video_movie_frame`), and two textures alternate so an upload never waits for the NV2A. The judder was the clock: the ADX stream's samples played step as its server copies them to sound RAM, and followed picture by picture they showed pictures for one blank and three by turns. A movie now shows one picture each frame period, and follows the clock only where it strays two pictures from it. Played smoothly at 30 frames a second on a console.
- **The split frame's pacing at 60 frames a second.** Kamui's statistics left the render's time at 0, so `src/system/present.c`'s split judged frames to fit that ran late, and the split ended and engaged by turns. `xbox/port/kamui.c` now gives the render's length, the lists read and handed to the host, as the console's Kamui gives the tile accelerator's intake.
- **The translucent sort's reads of write-combined memory.** The sort by triangle and the strips' checks of their vertices read the runs' vertices back from the host's write-combined memory, which the CPU reads uncached. The drawer now keeps the depths and the vertices' validity in cached memory as it writes them (`xbox/engine/sa1/emit.h`, `port_run_depths`).
- **Memory.** Kamui's lists take 512 KB instead of 4.1 MB, the arena of models has a room of its own beside the lists' vertex ring, the push buffer is 2 MB, and pbkit's own three frame buffers are used, so the back-buffer patch to nxdk is gone.

Changes made with these fixes:

- **The right stick's up and down** no longer raise and lower the camera: they do nothing, as the console's pad has no such stick. Left and right still turn it, past a 20% dead zone.
- **The sky** is drawn last of the opaque list, and profiled apart (`port_sky_task`).
- **The triggers-and-d-pad chord** no longer flips 16:9, 60 frames a second or the timing readout's pages: `host.txt` sets them (`HOST_WIDESCREEN`, `HOST_60FPS=1`), and a save's load no longer changes them. The triggers and the d-pad reach the game as they are.
- **720p:** where the dashboard refuses it, the picture is 480p.
- **The draw distance setting** (`HOST_DRAW_DISTANCE`) is gone: a distance other than the console's broke some levels. The land, the far clip and the range objects are made at are the console's again.
- **Test switches removed** once the bugs above were found: the blank's holds (`HOST_BLANK_DEFER`, `HOST_BLANK_PARTS`), the flash and land detector (`HOST_FLASH_WATCH`, which freed about 18 KB of `1ST_READ.BIN`'s data), the old waiting present (`HOST_SYNC_PRESENT`), the vertex check, the draw dumps, the hardware write watches, the audio and video tests, the sound tests and probes, and the renderer's and present's on/off switches (`HOST_KEEP`, `HOST_SORT`, `HOST_GPU_MODELS`, `HOST_SKY_LAST`, `HOST_TILES`, `HOST_FOG_TABLE`, `HOST_DEPTH_BOTTOM`, `HOST_GUARD`, `HOST_CULL_FLIP`, `HOST_DEPTH_FIXED`, `HOST_TEXTURE_RECHECK`, `HOST_TEXTURE_POOL`, `HOST_IDLE_PRESENT`, `HOST_PRESENT_WAIT`, `HOST_BLANK_LOCK`, among others). What they turned on or off is now fixed at its default. The renderer's model paths and the translucent sort's mode can still be set with `HOST_POKE` (`port_gpu_models`, `port_sort_triangles`).

## Debug flags

Settings are `KEY=VALUE` lines in `host.txt` on the Xbox (`E:\SONICADV\host.txt`), or environment variables for the PC host. The console's `host.txt` defaults to `HOST_PROFILE=0`, `HOST_LOG_FILE=0`, `HOST_REPORT=1`, `HOST_CRASH_LOG=1`. The log is `E:\SONICADV\log.txt`, the reports `E:\SONICADV\reportNN.txt`.

Logging and reports:

| Setting | Effect |
| --- | --- |
| `HOST_REPORT=0` | the pad's Back no longer writes a report (on by default) |
| `HOST_REPORT_AT=FRAME` | writes a report at that frame, as Back does |
| `HOST_CRASH_LOG=0` | no log written at a fault, halt or stall (on by default) |
| `HOST_LOG_FILE=1` | writes the log every 10 seconds |
| `HOST_PROFILE=1` | the profile every 600 blanks: frame rate, sampled functions, counts, the sound's cost |
| `HOST_GPU_PROFILE=1` | the NV2A's time for each part of a picture (clear, movie, each list's draws of list vertices and of models, with the translucent depth passes in the translucent's, resolve, bloom), each part drawn alone, an empty wait's cost taken off each wait, in the log every 10 s and in reports; the frame rate falls, and no slow host calls are logged (Xbox) |
| `HOST_GPU_PROFILE=2` | as 1, and one picture a second timed draw by draw: the 16 costliest draws in the report (Xbox) |
| `HOST_WATCH=SYMBOL` | logs a 1ST_READ.BIN variable when it changes |

Scripting and capture:

| Setting | Effect |
| --- | --- |
| `HOST_CLOCK=frames` | pads, shots, pokes and quit count rendered frames, not blanks |
| `HOST_PADS=600:8,610:0,2400:0/128/0` | scripts controller A: `PDD_DGT_` bits in hex, optional `/X/Y` stick |
| `HOST_POKE=300:menu_state+0:1:1` | writes a 1ST_READ.BIN variable at a blank (this one opens every stage); a report's `port_place` line restarts at its place |
| `HOST_SHOT=F,...` | saves those frames (`build/pc/shot_N.ppm` on the PC, PNGs from xemu) |
| `HOST_QUIT=N` | quits at blank or frame N |
| `HOST_HEADLESS=1`, `HOST_SCALE=2`, `HOST_SLOW=3` | no window, a doubled window, console time three times slower (PC) |
| `HOST_SAVES=DIR` | where the card files go (PC) |
| `HOST_PACK=PATH` | the texture pack to use (PC) |
| `HOST_WAV=PATH`, `HOST_SOUND_RECORD=PATH` | the sound to a WAV, or the game's side of the AICA recorded for `sndreplay` (PC) |

Pokes of note: `port_movie_skip+0:1`, `port_overlay+0:1`, `port_profile+0:1`, `port_report+0:1`, `port_gpu_models+0:N`, `port_sweep_cap+0:1` (widen far collision sweeps), `port_sort_triangles+0:N` (the translucent sort: 0 by strip, 1 by a triangle's nearest vertex, 2 by its middle).

The watch (on whenever reports are): logs frames in play that draw under four fifths of the strips or models of both frames beside them, with the models missing; and dumps player 0's last 32 frames and makes a report when its place or speed leaves all reason.

## Experimental flags

| Setting | Effect |
| --- | --- |
| `HOST_WIDESCREEN=0`, `=1`, `=2` | 4:3; 16:9; or the dashboard's widescreen setting (the default). 720p is always 16:9 |
| `HOST_60FPS=1` | 60 frames a second: each stage frame split into two pictures |
| `HOST_720P=1` | 1280×720 output, 16:9, no antialiasing |
| `HOST_MOVIES=0` | skips the movies (on by default where `movies.pak` is on the console) |
| `HOST_MSAA=0` | no antialiasing: the scene on a 640×480 surface, copied to the screen as the antialiased one is resolved |
| `HOST_VSYNC=0` | doesn't wait for the blank: the clock's blanks instead of the display's |
| `HOST_BLOOM=PERCENT`, `HOST_BLOOM_THRESHOLD=PERCENT` | the bloom's strength (40, 0 for none) and threshold (75) |
| `HOST_DEADZONE=PERCENT` | the left stick's dead zone (12) |
| `HOST_OVERLAY=1` | the on-screen figures (off by default) |

## Building

This section assumes no knowledge of the project. Follow steps 1 to 6 in order, typing each command in a terminal. Steps 1 to 4 are done once; after a change to the code, only step 5 is needed.

- [What the build makes](#what-the-build-makes)
- [Before you start](#before-you-start)
- [Step 1: install the tools](#step-1-install-the-tools): [Ubuntu](#ubuntu), [Fedora](#fedora), [macOS](#macos) or [Windows](#windows-wsl2)
- [Step 2: get the source](#step-2-get-the-source)
- [Step 3: get nxdk and the DSP assembler](#step-3-get-nxdk-and-the-dsp-assembler)
- [Step 4: add your disc](#step-4-add-your-disc)
- [Step 5: build](#step-5-build)
- [Step 6: run it](#step-6-run-it)
- [If something goes wrong](#if-something-goes-wrong)
- [More about the build](#more-about-the-build)

### What the build makes

The repository holds the game's code but none of its graphics, sound, levels or movies. The build compiles the code for the Xbox, takes the rest from your own copy of the Dreamcast disc, and makes:

- **`build/xbox/iso/`**: a folder to copy to the hard disk of a modded Xbox (`make xbox`);
- **`build/xbox/sonic.iso`**: the same files as a disc image, for the xemu emulator (`make xbox-disc`).

Both hold the game's code and data, so they are for your own console only.

### Before you start

You need:

- **A computer** with Linux, macOS on Apple silicon, or Windows 10 or 11 with WSL2. Ubuntu 26.04 and macOS 27.2 are tested; Fedora isn't.
- **About 10 GB of free disk space**, your disc's files included.
- **An internet connection** for the first build, which downloads and checks some of its own tools.
- **Your own copy of the disc**: the Japanese *Sonic Adventure*, HDR-0001, version 1.007, dumped as a `.gdi` file and its three tracks. Other releases don't work. The build can't download it.
- **Time:** the first build compiles a whole compiler (GCC for the Dreamcast's SH-4 processor) and then the game. Allow a couple of hours.

### Step 1: install the tools

Follow the one part below for your system.

#### Ubuntu

For 26.04 LTS, the tested release:

```sh
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y \
  build-essential git make curl tar gzip xz-utils coreutils \
  python3 python3-venv \
  clang lld llvm \
  gcc-multilib g++-multilib libc6-dev-i386 \
  bison flex cmake cargo \
  qemu-user \
  libsdl2-dev:i386 libgl-dev:i386 libegl-dev:i386 libgl1-mesa-dri:i386 \
  libx11-dev:i386 libwayland-dev:i386 \
  libasound2-dev:i386 libpulse-dev:i386
```

Check the versions. `clang` and `ld.lld` must be 21 or later:

```sh
clang --version
ld.lld --version
```

On a release whose clang is older than 21 (24.04 has 18), add `clang-21 lld-21 llvm-21` from apt.llvm.org and put that version's `clang`, `ld.lld` and `llvm-*` first on `PATH`.

`dependencies.md` says what each package is for.

#### Fedora

For 43 or later, which have LLVM 21 (untested):

```sh
sudo dnf install -y \
  make gcc gcc-c++ git curl tar gzip xz coreutils m4 \
  python3 \
  clang lld llvm \
  glibc-devel.i686 libstdc++-devel.i686 libgcc.i686 \
  bison flex cmake cargo \
  qemu-user \
  SDL2-devel.i686 mesa-libGL-devel.i686 mesa-libEGL-devel.i686 mesa-dri-drivers.i686 \
  libX11-devel.i686 wayland-devel.i686 \
  alsa-lib-devel.i686 pulseaudio-libs-devel.i686
```

`SDL2-devel.i686` is provided by `sdl2-compat-devel` on current Fedora, which works the same for the PC host. Check `clang --version` and `ld.lld --version` as on Ubuntu.

#### macOS

1. Install Xcode's command-line tools, and accept the dialog that opens:

   ```sh
   xcode-select --install
   ```

2. Install [Homebrew](https://brew.sh) with the command on its site. It asks for your password.

3. Install the packages:

   ```sh
   eval "$(/opt/homebrew/bin/brew shellenv)"
   brew install make llvm@21 lld@21 gnu-sed cmake rust
   ```

4. Put those tools ahead of Apple's. This lasts until the terminal is closed, so **do it again in each new terminal you build from**, or add these three lines to `~/.zshrc`:

   ```sh
   eval "$(/opt/homebrew/bin/brew shellenv)"
   B=/opt/homebrew/opt
   export PATH="$B/llvm@21/bin:$B/lld@21/bin:$B/make/libexec/gnubin:$B/gnu-sed/libexec/gnubin:$PATH"
   ```

5. Check that they are the ones found:

   ```sh
   make --version
   sed --version
   clang --version
   ld.lld --version
   ```

   Expect GNU Make 4.3 or later, GNU sed, Homebrew clang 21 and Homebrew LLD 21.

Why each is needed: Apple's `make` is 3.81, which the Makefile refuses, and `xbox/host/xbox/run.sh` runs `make`, not Homebrew's `gmake`; `pc.mk` uses `sed -i` as GNU's `sed` reads it, on which macOS's fails; and a newer LLVM, which `brew install llvm` gives, is untried. Use a native terminal: the Makefile refuses one under Rosetta 2.

#### Windows (WSL2)

The build runs inside Ubuntu, under Windows. Windows itself needs nothing else installed for it.

1. In an administrator PowerShell, run `wsl --install -d Ubuntu`, then reboot.
2. Open Ubuntu from the Start menu. Type every later command there, beginning with [Ubuntu](#ubuntu)'s above.
3. Keep the project in Ubuntu's own file system (for example `~/sonic-adventure-xbox`), not under `/mnt/c`, where the build is far slower. Move a checkout with `tar`, which keeps executable bits.

Windows' drives are under `/mnt` in Ubuntu: `C:\Users` is `/mnt/c/Users`.

### Step 2: get the source

```sh
cd ~
git clone https://github.com/FlavioOSilvaDias/sonic-adventure-xbox.git
cd sonic-adventure-xbox
```

Run the commands of every later step from this folder.

### Step 3: get nxdk and the DSP assembler

[nxdk](https://github.com/XboxDev/nxdk) is the kit that makes Xbox programs. The build looks for it in `~/refs/nxdk`. These commands download it, switch it to the revision the port was built with (newer ones are untried), and build its own tools:

```sh
git clone --recursive https://github.com/XboxDev/nxdk ~/refs/nxdk
git -C ~/refs/nxdk checkout 004fefbfe8fd93a5ea8ee9a95ff187b0f58ec8ed
git -C ~/refs/nxdk submodule update --init --recursive
make -C ~/refs/nxdk NXDK_DIR="$HOME/refs/nxdk" tools
```

The last command can't be skipped: `make xbox-disc` runs nxdk's `extract-xiso` but doesn't build it.

Then install the assembler for the Xbox's sound processor:

```sh
cargo install dsp56300-asm --version 1.0.0 --root ~/refs/dsp-tools
```

Cargo warns that `~/refs/dsp-tools/bin` isn't on `PATH`. Ignore it: the build runs the assembler from there.

Check both. This should list two files:

```sh
ls ~/refs/nxdk/tools/extract-xiso/build/extract-xiso ~/refs/dsp-tools/bin/dsp56300-asm
```

### Step 4: add your disc

Copy the `.gdi` file and its three tracks into the project's `orig/` folder. The `.gdi` may have any name, but there must be only one, and the tracks must keep the names written inside it.

Git ignores these files, so they can't be committed by accident. The build checks the game's files against `SHA256SUMS` and stops if the disc is another release.

### Step 5: build

For the folder to copy to a console:

```sh
make xbox
```

For that folder and the disc image for xemu:

```sh
make xbox-disc
```

Run one `make` at a time. Warnings are normal. The build succeeded if its last line is, for `make xbox`:

```text
run.sh: build/xbox/iso/, 1.1G: copy all of it to the console, or the files newer than its own
```

and for `make xbox-disc`:

```text
run.sh: build/xbox/sonic.iso, 1.1G
```

Along the way, both print `253 images linked; 0 over their areas`.

If the build stops or you interrupt it, run the same command again: it carries on from where it was. With nothing changed, a build takes about a minute. To build again after updating the source, run `git pull`, then the same command.

### Step 6: run it

**In xemu:** you can run `build/xbox/sonic.iso` on [xemu](https://xemu.app). The image is untested on a console, and a macOS build hasn't been run in xemu or on a console yet.

**On a real Xbox:** it must be modded.

1. Copy everything in `build/xbox/iso/` to `E:\SONICADV\` on the console, for example by FTP.
2. Start `default.xbe` from there.

Settings are lines of `E:\SONICADV\host.txt` ([Debug flags](#debug-flags)). The build can write them too, as `make xbox XBOX_SETTINGS="HOST_OVERLAY=1"`.

**xemu runs from WSL2, for developers:** put xemu for Windows in a folder of your own and set `XEMU_DIR` to its WSL path (for example `export XEMU_DIR=/mnt/c/Users/<you>/Tools/xemu`). `xbox/host/xbox/run.sh SECONDS [KEY=VALUE...]` builds the host, packs an ISO, runs xemu through Windows' `powershell.exe` (WSL's interop, on by default), then prints the log and converts the shots to `build/xbox/shot_fN.png`. One run at a time. These runs drive the Windows xemu, so on native Linux and macOS load the image by hand.

### If something goes wrong

| What you see | What to do |
|---|---|
| `the build needs GNU make 4.3 or later, and this is 3.81` | macOS: the terminal lacks step 1's `PATH` lines. Run them and try again. |
| `the build needs a native make, and this one runs under Rosetta 2` | macOS: use a terminal that doesn't open with Rosetta, and Homebrew in `/opt/homebrew`. |
| `the matching build needs Linux on x86` | macOS: plain `make` and `make check` don't run there. Use `make xbox`. |
| `sed:` errors as the x86 code compiles | macOS: `sed` isn't GNU's. Run step 1's `PATH` lines. |
| `not found; see README.md#build` | Install the command it names (step 1). |
| `no disc in orig/` | Copy the disc into `orig/` (step 4). |
| `orig/ has 2 .gdi files, and make takes one` | Leave one `.gdi` in `orig/`. |
| `isn't the original` | The disc is another release, or a bad dump. |
| `extract-xiso` not found | Run step 3's `make -C ~/refs/nxdk … tools`. |
| `dsp56300-asm` not found | Run step 3's `cargo install`. |
| `ld.lld`, `llvm-nm` or `llvm-readelf` not found | LLVM isn't installed or isn't on `PATH` (step 1). |
| A `tools/sh-elf` made on another computer is refused | Delete `tools/sh-elf` and build again. |

### More about the build

- **The steps of `make xbox`**, each skipped where it's up to date: `make dc` (the Dreamcast port's images and their link spec, which the x86 images link by; the first run builds GCC 16.2 for SH-4 into `tools/sh-elf`), the x86 images and their packs, the disc extracted from `orig/` into `build/disc`, then the Xbox host and the console's packs. It needs no `make check`, which builds the matching images, a separate goal.
- **Time, building in parallel:** on Ubuntu, 19 minutes from an empty `build/`, on top of GCC's own build. On an M1 Pro, 15 minutes, after a `make dc-check` of 9½ minutes had built GCC and some of the objects.
- **Space:** on Linux, about 5.3 GB in `build/` and 1.2 GB in `tools/sh-elf`; on macOS, 6.3 GB and 130 MB.
- **Two objects never compile for x86**, the loader's `kmsurfac` workaround and `syCache`, which the port replaces. Each build retries them, and `xbox-pc-test` reports `compiled 3356 of 3358 objects`.
- **nxdk and nxdk-audio:** `xbox/host/xbox/run.sh` puts `~/refs/nxdk/bin` (`NXDK_DIR`) on `PATH` itself. It applies the port's pbkit patch, `xbox/third_party/nxdk-pbkit-lost-swap.patch`, to that checkout once. It clones nxdk-audio from https://github.com/Ryzee119/nxdk-audio to `~/refs/nxdk-audio` (`NXDK_AUDIO_DIR`) at the port's revision and applies `xbox/third_party/nxdk-audio-loop-start.patch` once.
- **Tested on macOS with** an M1 Pro with 16 GB, macOS 27.2, Xcode's Python 3.9.6, LLVM and lld 21.1.8, GNU make 4.4.1, GNU sed 4.10, CMake 4.5.1 and cargo 1.100.0.

Optional, for debugging, on Linux and WSL2. The first checks the 32-bit libraries and `qemu-sh4`; the rest are the x86 images' report and the PC test host, the game on that host in a window, and the math tests (after `make xbox`: the port's C against the shipped SH-4 assembly, under `qemu-sh4`), each line of which reports `0 real`:

```sh
qemu-sh4 --version | head -1
echo 'int main(void){return 0;}' > /tmp/t.c && cc -m32 /tmp/t.c -o /tmp/t32 -lSDL2 -lGL && echo "32-bit SDL2/GL ok"
make xbox-pc-test xbox-pc-test-link xbox-pc-test-host
make xbox-pc-test-run
sh xbox/test/all.sh
```

- The PC host's keyboard: arrows the stick; I, J, K, L the d-pad; Z, X, A, S are A, B, X, Y; Q, W the triggers; Enter Start; Esc quits.
- The PC host's window under WSL2 needs WSLg, which current Windows 10 and 11 include: in Ubuntu, `echo $DISPLAY $WAYLAND_DISPLAY` should print `:0 wayland-0`.
- None of these run on macOS: the PC host is a 32-bit Linux program, and `qemu-sh4`'s user-mode emulation is Linux's alone.

## Future plans

- **A renderer without the tile accelerator's lists:** runs handed straight to the host instead of written into Kamui's lists and read back.
- **More work on the NV2A:** clipping by the NV2A instead of the drawer.
- **Performance:** a profiled console run after the x87 FIPR and FTRV; packed SSE where the original's operations are separate floats; the push buffer, now 2 MB (937 KB of the old 1 MB in Speed Highway's third act).
- **Memory:** account for the free pages; give the textures' pool and the arenas more room.
- **Movies:** their room, and a run on the console.
- **Tooling:** a set of frames compared on the PC by one command; the sound compared with Flycast again; an FTP push to the console.

## Upstream

The tree is upstream's 6e5a40e (reshaped/sonic-adventure), merged on 2026-11-20 into the port's first commit, deb4c2a, which forked from 25a3d31. The port follows upstream's Dreamcast port, `make dc`, not its native PC game: its C, guards and composition (`config/dreamcast.mk`), compiled for x86 under `NONMATCHING` and `XBOX`, never `DREAMCAST`. To take a newer upstream, apply `git diff 6e5a40e NEW` to the tree, then check `make check`, `make dc`, `make xbox-pc-test xbox-pc-test-link` and an xemu run.

- **Shipped sides:** `polygon.c`'s console rewrites (`engine/tools/marks.py`'s `CONSOLE`, by `tools/pcconsole.py`) take their shipped side, as upstream's native game does.
- **The drawer:** every one of upstream's live draws (`sa_present_live`'s formats) goes to the renderer as a run, the host's own vertices, rather than as the tile accelerator's in Kamui's lists; the lists keep each draw's header and the run's mark, in the draws' order (`xbox/engine/sa1/emit.h`). Models draw through upstream's layer, `src/draw/model.c`, into the port's walk, `xbox/engine/sa1/model.c`, the old make gcc drawer with its GPU path, which projects and clips on the CPU what the NV2A can't take whole. Sprites, screen polygons, 2D polygons and lines, 3D lines and easy models run upstream's sends unchanged, through thin wrappers in `xbox/engine/sa1/` and `xbox/engine/sa1/strip.h`, with small `XBOX` branches in `engine/dc/sa1/sprite.h`, `engine/dc/sa1/easy.h` and `engine/dc/njsprite.h`; the easy models' kernel is `xbox/engine/batch.h`'s C. Ninja's print is one run a print (`sdk/ninja/njDebug.c`). `xbox/engine/` comes before `engine/dc/` on the include path, so a header there takes the place of the Dreamcast's. Still in the lists: Ninja's own draws outside those formats, as `njDrawTriangle3D` and `njDrawPolygon3D`, a few dozen vertices a frame in the demos.
- **SH4ZAM:** the Dreamcast engine's sprites and screen polygons run SH4ZAM's software back end, on the port's XMTRX (`xbox/port/xmtrx.c`), with `xbox/include/zam_xbox.h` in place of `ninja_zam.h`'s helpers.
- **Left out:** `engine/dc/owner.c` (the port's Kamui owns the frame) and `engine/dc/crash.c` (`port_stop` logs and exits).
- **Not yet re-applied:** the port's split-frame view fix (a far sai, the amenbo's start step) on upstream's reworked `present.c`; re-apply it if those return.

## How the port was made

The port started from the GCC port, now upstream's Dreamcast port, not the matching build: [PORTS.md](PORTS.md) replaces `.src` files with C and keeps SHC's results under guards. The port defines `XBOX` and compiles with `NONMATCHING` defined too, never `DREAMCAST`.

- **Stage 0, the PC build:** `make xbox-pc-test` (then `make pc`) compiles every C source for i386 with Clang, SSE math, the float pass and the guards.
- **Stage 1, no SH-4 code:** every game `.src` file but the movie player's became C in `xbox/`, each checked by `xbox/test/run.sh` under `qemu-sh4` against the original assembly. Ninja's inline bodies became C by `tools/sh4lift.py` and `tools/sh2c.py`.
- **Stage 2, the images on x86:** each image linked at its area and loaded as a file; the host and the port layer.
- **Stage 3, graphics:** Ninja's interface kept, Kamui's lists drawn by the port's renderer; movies through pl_mpeg.
- **Stage 4, sound:** the AICA emulated and the disc's driver run on it.
- **Stage 5, the Xbox:** the nxdk host, pbkit in place of OpenGL, the APU in place of the mixer, the packs, then the console work: pacing and vsync, the kept and lit models, fog, bloom, the translucent sort, the sound pack, the math made more native to the Xbox, the pbkit lost-swap fix, and the bigger models' arena.
