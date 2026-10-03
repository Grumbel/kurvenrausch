<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Kurvenrausch for the R36S (ArkOS) and other PortMaster handhelds

A classic pseudo-3D racer: one lap around the world. Native aarch64 build,
linked against an ArkOS-era (Ubuntu 19.10) userland; it uses the device's
SDL2. The 320x240 picture fills the 640x480 screen at exactly twice the size.

## Install

Copy `Kurvenrausch.sh` and the `kurvenrausch/` folder to `/roms/ports/`, or
put the zip into `ports/PortMaster/autoinstall/` and start PortMaster once.

## Controls

| Button       | Action                       |
|--------------|------------------------------|
| D-pad, stick | steer                        |
| A / R2       | accelerate                   |
| B / L2       | brake; stopped, let go and press again: reverse |
| Y, R1        | nitro                        |
| X            | horn                         |
| L1           | handbrake                    |
| Start        | pause menu                   |
| Select       | camera view                  |
| Right stick  | radio (flick left or right)  |
| Select+Start | quit                         |

Lap times and choices are kept in `kurvenrausch/conf/`.

Source and licence (GPL-3.0-or-later): https://github.com/Grumbel/kurvenrausch
