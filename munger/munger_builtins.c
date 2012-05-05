/******************************************************************************
 * munger_builtins.c 
 *
 * GOAT-Plugs: GCC Obfuscation Augmentation Tools
 *
 * Copyright (C) 2011 Matt Davis (enferex) of 757Labs
 * (www.757Labs.org)
 *
 * munger_builtins.c is part of the GOAT-Plugs GCC plugin set.
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Runtime function that decodes the data */
void *__decode(void *decoded, void *orig, unsigned int length)
{
    unsigned i;

    /* If we have already decoded this.... do nothing */
    if (decoded)
      return (char *)decoded;

    decoded = calloc(1, length);

    for (i=0; i<length; ++i)
      ((char *)decoded)[i] = (((char*)orig)[i] ^ -1);
    
    return decoded;
}

#ifdef __cplusplus
} /* extern C */
#endif
