#!/bin/bash
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Kurvenrausch: PortMaster launcher for the R36S / ArkOS (and alike).

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/kurvenrausch"
cd "$GAMEDIR" || exit 1
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# Lap times and choices stay with the port.
export XDG_STATE_HOME="$GAMEDIR/conf"
# The pad's layout from PortMaster, for SDL's game controller API.
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# gptokeyb: Select+Start quits.
$GPTOKEYB "kurvenrausch" &
pm_platform_helper "$GAMEDIR/kurvenrausch"
./kurvenrausch --fullscreen
pm_finish
