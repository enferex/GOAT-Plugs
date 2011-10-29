/******************************************************************************
 * slimer_builtins.c 
 *
 * GOAT-Plugs: GCC Obfuscation Augmentation Tools
 * 
 * Copyright (C) 2011 Matt Davis (enferex) of 757Labs
 * (www.757Labs.org)
 *
 * Special thanks to JPanic for schooling me on ideas and junk instructions
 *
 * slimer_builtins.c is part of the GOAT-Plugs GCC plugin set.
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

static void **__funcs;


void *__slimer_get_funcs(void)
{
    return __funcs;
}


void __slimer_init(int n_funcs)
{
    __funcs = calloc(1, sizeof(void *) * n_funcs);
}


void __slimer_add_fn(void *fn, int index)
{
    __funcs[index] = fn;
}
