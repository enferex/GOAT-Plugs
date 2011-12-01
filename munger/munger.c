/******************************************************************************
 * munger.c 
 *
 * GOAT-Plugs: GCC Obfuscation Augmentation Tools
 * 
 * Munger plugin - Simple readonly data obfuscation.
 *
 * Copyright (C) 2011 Matt Davis (enferex) of 757Labs
 * (www.757Labs.org)
 *
 * munger.c is part of the GOAT-Plugs GCC plugin set.
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

/* munger.c: Simple readonly data obfuscation
 * 
 * This plugin obfuscates a binary by storing all readonly strings as encoded
 * values.  This prevents/deters some simple forensics measures where someone
 * might inspect the binary for hard-coded strings.  Usually this can be done
 * via "strings".  Now, with this plugin, the value in the binary is not
 * readable, as it is encoded.  At runtime, the binary will automatically decode
 * the value and use it.  You write code as you normally would and this plugin
 * does all the work.
 *
 * Example:
 *    printf("Somewaycool shell code");
 *
 * Your Binary would have that string stored as "gibberish" but at run time the
 * "gibberish" is decoded and returned as a proper value.
 * 
 * This plugin works by walking each statement in the input source code, after
 * gcc has converted it to SSA and presents us with a GIMPLE representation for
 * each function.
 *
 * If we encounter a value that is stored in the readonly section that is a
 * string, we create a global variable for that readonly constant.  This global
 * is emitted into the resulting binary.  We then, before the statement that
 * would use the readonly value, create an assignment to the global variable.
 *
 * In the binary we store the encoded value, and at runtime we resolve this
 * encoded value, by creating a global variable (mentioned above) setting that
 * global only once in the program, and we pass that global to any routine that
 * uses the readonly.
 * 
 * [BEFORE TRANSFORMATION]
 * ... code ...
 * some_function(use_a_readonly_string);
 *... code ...
 *
 * [AFTER TRANSFORMATION]
 * ... code ...
 * GLOBAL_FOR_READONLY_VARIABLE = decode(use_readonly_string);
 * some_function(GLOBAL_FOR_READONLY_VARIABLE);
 * ... code ...
 *
 * decode() looks like this:
 *      IF READONLY HAS BEEN DECODED ALREADY RETURN THAT
 *      ELSE ALLOCATE MEMORY TO GLOBAL AND DECODE READONLY
 */

#include <stdio.h>
#include <coretypes.h>
#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>
#include <tree-flow.h>
#include <tree-pass.h>
#include <vec.h>


/* Store all readonlys we encounter */
typedef struct _encdec_d *encdec_t;
struct _encdec_d
{
    tree enc_node;
    tree dec_node;
};
DEF_VEC_P(encdec_t);
DEF_VEC_ALLOC_P(encdec_t, gc);
VEC(encdec_t,gc) *readonlyz;


/* Required for the plugin to work */
int plugin_is_GPL_compatible = 1;


/* Help info about the plugin if one were to use gcc --version --help */
static struct plugin_info munger_info =
{
    .version = "0.2",
    .help = "Encodes readonly constant string data at compile time.  The string "
            "is then decoded automatically at runtime."
};


/* Ensure the gcc version will work with our plugin */
static struct plugin_gcc_version munger_ver =
{
    .basever = "4.6",
};


/* We don't need to run any tests before we execute our plugin pass */
static bool munger_gate(void)
{
    return true;
}


/* Add 'node' to our vec of readonly vars */
static tree add_unique(tree node)
{
    unsigned ii;
    tree dec_node;
    encdec_t ed;

    for (ii=0; VEC_iterate(encdec_t, readonlyz, ii, ed); ++ii)
      if (ed->enc_node == node)
        return ed->dec_node;

    /* Create a global variable, thanks to "init_ic_make_global_vars()" */
    dec_node = build_decl(UNKNOWN_LOCATION, VAR_DECL, NULL_TREE, ptr_type_node);
    DECL_NAME(dec_node) = create_tmp_var_name("FOO");
    TREE_STATIC(dec_node) = 1;
    varpool_finalize_decl(dec_node);
    varpool_mark_needed_node(varpool_node(dec_node));

    /* Remember the node */
    ed = xmalloc(sizeof(encdec_t));
    ed->enc_node = node;
    ed->dec_node = dec_node;
    VEC_safe_push(encdec_t, gc, readonlyz, ed);

    return dec_node;
}


/* Initial global data we use to insert function calls to our built-in function,
 * which is coded in munger_builtin.c
 */
static tree test_decode_fndecl;
static void init_builtins(void)
{
    static int initted = 0;
    tree voidp_ptr_ptr_uint_fn_type;

    /* Only initialize this once */
    if (initted)
      return;

    voidp_ptr_ptr_uint_fn_type = build_function_type_list(
        ptr_type_node, ptr_type_node,
        ptr_type_node, uint32_type_node, NULL_TREE);

    /* __decode is located in munger_builtin.c */
    test_decode_fndecl = 
        build_fn_decl("__decode", voidp_ptr_ptr_uint_fn_type);

    initted = 1;
}


/* Find the STRING_CST node stashed in node */
static tree get_str_cst(tree node)
{
    tree str;

    str = node;

    /* Filter out types we are ignoring */
    if (TREE_CODE(node) == VAR_DECL)
    {
        if (!(str = DECL_INITIAL(node))) /* nop expr  */
          return NULL_TREE;
        else if (TREE_CODE(str) == INTEGER_CST) /* Ignore single chars */
          return NULL_TREE;

        str = TREE_OPERAND(str, 0); /* addr expr */
    }
    else if (TREE_CODE(node) == ADDR_EXPR)
      str = TREE_OPERAND(str, 0);

    if (!str)
      return NULL_TREE;

    /* We only deal with readonly stuff */
    if (!TYPE_READONLY(str) && (TREE_CODE(str) != ARRAY_REF))
      return NULL_TREE;
   
    if (TREE_CODE(str) != STRING_CST) 
      str = TREE_OPERAND(str, 0);

    if (TREE_CODE(str) != STRING_CST)
      return NULL_TREE;
    else
      return str;
}


/* Emit code which will call __decode()
 * Returns the lhs variable this function creates
 */
static tree insert_decode_bn(gimple stmt, tree lhs, tree arg)
{
    gimple call;
    gimple_stmt_iterator gsi;
    tree str, size_node, new_lhs;

    str = get_str_cst(arg);
    
    /* Build a node to hold the size */
    size_node = build_int_cstu(uint32_type_node, TREE_STRING_LENGTH(str));

    /* If lhs has already been decoded, do nothing */
    new_lhs = create_tmp_var(ptr_type_node, "FOO");
    new_lhs = make_ssa_name(new_lhs, stmt);

    /* Insert the code for the 'new_lhs = decode();' statement */
    call = gimple_build_call(test_decode_fndecl, 3, lhs, arg, size_node);;
    gimple_call_set_lhs(call, new_lhs);
    gsi = gsi_for_stmt(stmt);
    gsi_insert_before(&gsi, call, GSI_NEW_STMT);
    return new_lhs;
}


/* Simple algorithm to encode a readonly string: xor with -1 (thanks Robert
 * Morris).
 */
static void encode(tree node)
{
    int i;

    for (i=0; i<TREE_STRING_LENGTH(node); ++i)
      ((char *)TREE_STRING_POINTER(node))[i] =
        TREE_STRING_POINTER(node)[i] ^ -1;
}


/* Locate read only string constants */
static void process_readonlys(gimple stmt)
{
    int i;
    gimple assign_global;
    gimple_stmt_iterator gsi;
    tree op, decoded_op, orig, decoded_var, lhs;

    /* For each operand in stmt */
    for (i=0; i<gimple_num_ops(stmt); ++i)
    {
        if (!(op = gimple_op(stmt, i)))
          continue;

        /* Store the base type and trace through the node until we find the
         * string type, if it even is a string constant.
         */
        orig = op;
        if (!(op = get_str_cst(op)))
          continue;

        /* Encode the data */
        encode(op);

        /* Only store this guy once and return the value we emit into the binary
         * as the decoded version.
         */
        decoded_op = add_unique(orig);

        /* Create a ssa instance of a variable that we put the global into */
        decoded_var = create_tmp_reg(ptr_type_node, "FOO");
        decoded_var = make_ssa_name(decoded_var, stmt);
        DECL_ARTIFICIAL(decoded_var) = 1;

        /* Assign the global to the ssa name instance */
        assign_global = gimple_build_assign_stat(decoded_var, decoded_op);
        gsi = gsi_for_stmt(stmt);
        gsi_insert_before(&gsi, assign_global, GSI_NEW_STMT);

        /* Set the global which points to the  and forget it... (thanks Ron Popeil) */
        lhs = insert_decode_bn(stmt, decoded_var, orig);
        gimple_set_op(stmt, i, lhs);
    }
}


/* This is the plugin callback that gets triggered based on data specified in
 * the pass struct defined below.
 * Returns 0 on success, error otherwise
 */
static unsigned int munger_exec(void)
{
    basic_block bb;
    gimple_stmt_iterator gsi;

    init_builtins();

    FOR_EACH_BB(bb)
      for (gsi=gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi))
        process_readonlys(gsi_stmt(gsi));

#ifdef GOAT_DEBUG
    debug_function(current_function_decl, 0);
#endif

    return 0;
}


static struct gimple_opt_pass munger_pass = 
{
    .pass.type = GIMPLE_PASS,
    .pass.name = "munger", /* For use in the dump file          */
    .pass.gate = munger_gate,
    .pass.execute = munger_exec, /* Your pass handler/callback */
    .pass.todo_flags_finish = TODO_update_ssa|TODO_verify_ssa|TODO_cleanup_cfg,
};


/* Return 0 on success or error code on failure */
int plugin_init(struct plugin_name_args   *info,  /* Argument info  */
                struct plugin_gcc_version *ver)   /* Version of GCC */
{
    struct register_pass_info pass;

     if (strncmp(ver->basever, munger_ver.basever, strlen("4.6")))
       return -1; /* Incorrect version of gcc */

    /* Get called after gcc has produced the SSA representation of the program.
     * After the first SSA pass.
     */
    pass.pass = &munger_pass.pass;
    pass.reference_pass_name = "ssa";
    pass.ref_pass_instance_number = 1;
    pass.pos_op = PASS_POS_INSERT_AFTER;

    /* Tell gcc we want to be called after the first SSA pass */
    register_callback("munger", PLUGIN_PASS_MANAGER_SETUP, NULL, &pass);

    /* Tell gcc some information about us... just for use in --help and
     * --version
     */
    register_callback("munger", PLUGIN_INFO, NULL, &munger_info);

    /* Successful initialization */ 
    return 0;
}
