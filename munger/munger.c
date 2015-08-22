/******************************************************************************
 * munger.c 
 *
 * GOAT-Plugs: GCC Obfuscation Augmentation Tools
 * 
 * Munger plugin - Simple readonly data obfuscation.
 *
 * Copyright (C) 2011, 2012, 2015 Matt Davis (enferex) of 757Labs
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

#include <gcc-plugin.h>
#include <coretypes.h>
#include <tree.h>
#include <tree-pass.h>
#include <tree-ssa-alias.h>
#include <function.h>
#include <cgraph.h>
#include <internal-fn.h>
#include <stringpool.h>
#include <gimple-expr.h>
#include <gimple.h>
#include <gimple-iterator.h>
#include <gimple-ssa.h>
#include <vec.h>
#include <diagnostic.h>
#include <tree-ssanames.h>


/* Store all readonlys we encounter */
typedef struct _encdec_d *encdec_t;
struct _encdec_d
{
    tree strcst;   /* String const of the original node */
    tree dec_node; /* Global for this node              */
};
vec<encdec_t> readonlyz = vNULL;


/* Required for the plugin to work */
int plugin_is_GPL_compatible = 1;


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
   
    if (TREE_OPERAND_LENGTH(str) > 0 && TREE_CODE(str) != STRING_CST)
      str = TREE_OPERAND(str, 0);

    if (TREE_CODE(str) != STRING_CST)
      return NULL_TREE;
    else {
      return str;
    }
}


/* Add 'node' to our vec of readonly vars */
static tree add_unique(tree node)
{
    unsigned ii;
    tree dec_node;
    encdec_t ed;

    FOR_EACH_VEC_ELT (readonlyz, ii, ed)
      if (ed->strcst == node)
        return ed->dec_node;

    /* Create a global variable, thanks to "init_ic_make_global_vars()" */
    dec_node = build_decl(UNKNOWN_LOCATION, VAR_DECL, NULL_TREE, ptr_type_node);
    DECL_NAME(dec_node) = create_tmp_var_name("MUNGER_GLOBAL");
    DECL_ARTIFICIAL(dec_node) = 1;
    TREE_STATIC(dec_node) = 1;
    varpool_finalize_decl(dec_node);

    /* Remember the node */
    ed = (encdec_t)xmalloc(sizeof(struct _encdec_d));
    ed->dec_node = dec_node;
    ed->strcst = get_str_cst(node);
    readonlyz.safe_push(ed);

    return dec_node;
}


/* Emit code which will call __decode()
 * Returns the lhs variable this function creates (decoded data).
 * This also sets the global
 */
static tree insert_decode_bn(gimple stmt, tree lhs, tree arg)
{
    unsigned ii;
    gimple call;
    gimple_stmt_iterator gsi;
    encdec_t ed;
    tree str, size_node;

    /* Build a node to hold the size */
    str = get_str_cst(arg);
    size_node = build_int_cstu(uint32_type_node, TREE_STRING_LENGTH(str));

    /* Build the call DECODED = __decode() */
    call = gimple_build_call(test_decode_fndecl, 3, lhs, arg, size_node);;

    /* Insert the code for the 'DECODED = __decode();' statement */
    gsi = gsi_for_stmt(stmt);
    gsi_insert_before(&gsi, call, GSI_NEW_STMT);

    /* Get the global associated to the strcst */
    FOR_EACH_VEC_ELT(readonlyz, ii, ed)
      if (ed->strcst == str)
        break;

    /* Set the global to the decoded value (to avoid decoding multiple times) */
    gimple_call_set_lhs(call, ed->dec_node);

    /* Return the tmp variable which has the decoded value in it */
    return ed->dec_node;
}


/* Algo to encode a readonly string: xor with -1 (thanks Robert Morris) */
static void encode(tree node)
{
    int ii;
    encdec_t ed;

    /* Do no encode the data if we already have */
    FOR_EACH_VEC_ELT(readonlyz, ii, ed)
      if (ed->strcst == node)
        return;

    for (ii=0; ii<TREE_STRING_LENGTH(node); ++ii)
      ((char *)TREE_STRING_POINTER(node))[ii] =
        TREE_STRING_POINTER(node)[ii] ^ -1;
}


/* Locate read only string constants */
static void process_readonlys(gimple stmt)
{
    unsigned i;
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
        decoded_op = add_unique(op);

        /* Create a ssa instance of a variable that we put the global into */
        decoded_var = create_tmp_var(ptr_type_node, "MUNGER_ARG");
        decoded_var = make_ssa_name(decoded_var, stmt);

        /* Assign the global to the ssa name instance */
        assign_global = gimple_build_assign_stat(decoded_var, decoded_op);
        gsi = gsi_for_stmt(stmt);
        gsi_insert_before(&gsi, assign_global, GSI_NEW_STMT);

        /* Set the global which points to the decoded  data and forget it... */
        lhs = insert_decode_bn(stmt, decoded_var, orig);
        gimple_set_op(stmt, i, lhs);
        update_stmt(stmt);
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

    FOR_EACH_BB_FN(bb, cfun)
      for (gsi=gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi))
        process_readonlys(gsi_stmt(gsi));

#ifdef GOAT_DEBUG
    debug_function(current_function_decl, 0);
#endif

    return 0;
}


/* Permit only gcc version 4.9 */
static inline bool munger_version_check(const struct plugin_gcc_version *ver)
{
    if ((strncmp(ver->basever, "4.9", strlen("4.9")) == 0))
      return true;

    error("[GOAT-Plugs] The munger plugin is not supported for this version of "
          "the compiler, try a 4.9.x series");

    return false;
}

namespace {
const pass_data pass_data_munger =
{
    GIMPLE_PASS, /* Type           */
    "munger",    /* Name           */
    0,           /* opt-info flags */
    false,       /* Has gate       */
    true,        /* Has exec       */
    TV_NONE,     /* Time var id    */
    0,           /* Prop. required */
    0,           /* Prop. provided */
    0,           /* Prop destroyed */
    0,           /* Flags start    */
    TODO_update_ssa | TODO_verify_ssa | TODO_cleanup_cfg /* Flags finish */
};

class pass_munger : public gimple_opt_pass
{
public:
    pass_munger() : gimple_opt_pass(pass_data_munger, NULL) {;}
    unsigned int execute() { return munger_exec(); }
};
} // Anonymous namespace


/* Return 0 on success or error code on failure */
int plugin_init(struct plugin_name_args   *info,  /* Argument info  */
                struct plugin_gcc_version *ver)   /* Version of GCC */
{
    struct register_pass_info pass;
    static struct plugin_info munger_info;

    /* Version info */
    munger_info.version = "0.4";
    munger_info.help = "Encodes readonly constant string data at compile "
                       "time.  The string is then decoded automatically "
                       "at runtime.";

    if (!munger_version_check(ver))
      return -1; /* Incorrect version of gcc */

    /* Initalize the GIMPLE pass info */

    /* Get called after gcc has produced the SSA representation of the program.
     * After the first SSA pass.
     */
    pass.pass = new pass_munger();
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
