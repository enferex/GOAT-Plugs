/******************************************************************************
 * slimer.c 
 *
 * GOAT-Plugs: GCC Obfuscation Augmentation Tools
 * 
 * Slimer plugin - Inserts junk functions and calls them non-determinstically at
 * runtime.
 *
 * Copyright (C) 2011 Matt Davis (enferex) of 757Labs
 * (www.757Labs.org)
 *
 * Special thanks to JPanic for schooling me on ideas and junk instructions
 *
 * slimer.c is part of the GOAT-Plugs GCC plugin set.
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
#include <cgraph.h>
#include <tree.h>
#include <tree-flow.h>
#include <tree-pass.h>
#include <tree-iterator.h>


/* Special thanks to JPanic for schooling me on ideas and junk instructions */


/* Required for the plugin to work */
int plugin_is_GPL_compatible = 1;


#define TAG "[slimer] "


/* Arguments passed at compile time:
 * -fplugin-arg-slimer-numfuncs
 * -fplugin-arg-slimer-maxcalls
 */
static int n_funcs = 0;
static int max_calls = 0;


/* Help info about the plugin if one were to use gcc's --version --help */
static struct plugin_info slimer_info =
{
    .version = "0.1",
    .help = "Software obfuscating plugin.  This creates 'numfuncs' of junk "
            "functions called pseudo-randomly at runtime.\n"
            "The calls to this will be placed between assignment and function "
            "call statements.  'maxcalls' will add at most this many calls to the "
            "junk functions.  This is not a guaranteed value, since calls are "
            "inserted pseudo-randomly\n"
            "-fplugin-arg-slimer-numfuncs\n"
            "-fplugin-arg-slimer-maxcalls",
};


/* How we test to ensure the gcc version will work with our plugin */
static struct plugin_gcc_version nopper_ver =
{
    .basever = "4.6",
};


/* Func decl tree nodes, so we avoid processing a function multiple times */
static VEC(tree,gc) *analyized_fns;


/* List of tree decl nodes for all of junk functions we create */
static VEC(tree,gc) *fakes = NULL;


/* Returns 'true' if we have already processed the function */
static bool has_been_processed(tree fndecl)
{
    unsigned i;
    tree decl;

    /* If we have already analyized it, then return true */
    FOR_EACH_VEC_ELT(tree, analyized_fns, i, decl)
      if (decl == fndecl)
        return true;

    /* If it's a junk function, return true */
    FOR_EACH_VEC_ELT(tree, fakes, i, decl)
      if (decl == fndecl)
        return true;

    return false;
}


/* TODO: The compiler will remove much junk, use the jpanic plugin instead
static tree gen_junk(void)
{
    unsigned i;
    char name[8] = {0};
    tree stmts = NULL_TREE, stmt;

    for (i=0; i<5; ++i)
    {
        snprintf(name, 7, "foo%d", i);
        tree tmp = create_tmp_var(integer_type_node, name);
        stmt = build1(DECL_EXPR, void_type_node, tmp);
        append_to_statement_list(stmt, &stmts);
    }

    return stmts;
}
*/


static tree build_junk_fn(unsigned id)
{
    char fnname[32] = {0};
    tree decl, resdecl, initial, proto;

    /* Func decl */
    snprintf(fnname, 31, "__func%d", id);
    proto = build_varargs_function_type_list(void_type_node, NULL_TREE);
    decl = build_fn_decl(fnname, proto);
    SET_DECL_ASSEMBLER_NAME(decl, get_identifier(fnname));
    
    printf(TAG "Creating junk function: %s\n", fnname);
   
    /* Result */ 
    resdecl=build_decl(BUILTINS_LOCATION,RESULT_DECL,NULL_TREE,void_type_node);
    DECL_ARTIFICIAL(resdecl) = 1;
    DECL_CONTEXT(resdecl) = decl;
    DECL_RESULT(decl) = resdecl;
    
    /* Initial */
    initial = make_node(BLOCK);
    TREE_USED(initial) = 1;
    DECL_INITIAL(decl) = initial;
    DECL_UNINLINABLE(decl) = 1;
    DECL_EXTERNAL(decl) = 0;
    DECL_PRESERVE_P(decl) = 1;

    /* Func decl */
    TREE_USED(decl) = 1;
    TREE_PUBLIC(decl) = 1;
    TREE_STATIC(decl) = 1;
    DECL_ARTIFICIAL(decl) = 1;

    /* Make the function */
    push_struct_function(decl);
    /* DECL_SAVED_TREE(decl) = gen_junk(); */
    cfun->function_end_locus = BUILTINS_LOCATION;
    gimplify_function_tree(decl);

    /* Update */
    cgraph_add_new_function(decl, false);
    cgraph_mark_needed_node(cgraph_node(decl));
    current_function_decl = NULL_TREE;
    pop_cfun();

    return decl;
}


static void insert_call(gimple stmt, tree decl)
{
    gimple call;
    gimple_stmt_iterator gsi;

    /* Call the function */
    call = gimple_build_call(decl, 0);
    gsi = gsi_for_stmt(stmt);
    gsi_insert_before(&gsi, call, GSI_NEW_STMT);

    /* So we don't process this bad-boy */
    VEC_safe_push(tree, gc, analyized_fns, decl);
}


static void gen_fake_funcs(int n_funcs)
{
    unsigned i;

    fakes = VEC_alloc(tree, gc, 0);
    for (i=0; i<n_funcs; ++i)
      VEC_safe_push(tree, gc, fakes, build_junk_fn(i));
}


static void insert_call_to_junk_fn(gimple stmt)
{
    tree tv, rv, fn, rhs, tmp;
    gimple gimp;
    gimple_stmt_iterator gsi;
    static bool has_initted;
    static tree decl, proto, decl_get_funcs, proto_get_funcs, fn_ptr_type;
    
    printf("slimer: Inserting junk function call at line: %d\n",
            gimple_lineno(stmt));

    /* Get random value modulo n_funcs for index into runtime __funcs array of
     * junk functions:
     *     rv = time % n_funcs;
     *     fn = __funcs + rv;
     *     call fn
     */

    /* Build instances */
    if (!has_initted)
    {
        proto = build_function_type_list(
            uint64_type_node, ptr_type_node, NULL_TREE);
        decl = build_fn_decl("time", proto);
        DECL_EXTERNAL(decl) = 1;

        proto_get_funcs = build_function_type_list(ptr_type_node, NULL_TREE);
        decl_get_funcs = build_fn_decl("__slimer_get_funcs", proto_get_funcs);

        fn_ptr_type = build_function_type_list(
            void_type_node, void_type_node, NULL_TREE);

        has_initted = true;
    }

    /* time_tmp = time(NULL); */
    tv = create_tmp_var(uint64_type_node, "time_tmp");
    tv = make_ssa_name(tv, NULL);
    gimp = gimple_build_call(decl, 1, null_pointer_node);
    gimple_set_lhs(gimp, tv);
    gsi = gsi_for_stmt(stmt);
    gsi_insert_before(&gsi, gimp, GSI_SAME_STMT);

    /* rv_tmp = time_temp % n_funcs */
    rv = create_tmp_var(uint64_type_node, "rv_tmp");
    rv = make_ssa_name(rv, NULL);
    rhs = build_int_cst(integer_type_node, n_funcs);
    gimp = gimple_build_assign_with_ops(TRUNC_MOD_EXPR, rv, tv, rhs);
    gsi_insert_before(&gsi, gimp, GSI_SAME_STMT);

    /* tmp = __slimer_get_funcs(); TODO: Get rid of __slimer_get_funcs()
     * rv = rv * sizeof(void *)
     * fn_tmp = tmp + rv
     */
    tree pp_type = build_pointer_type(ptr_type_node);
    tmp = create_tmp_var(pp_type, "tmp");
    tmp = make_ssa_name(tmp, NULL);
    gimp = gimple_build_call(decl_get_funcs, 0);
    gimple_set_lhs(gimp, tmp);
    gsi_insert_before(&gsi, gimp, GSI_SAME_STMT);

    /* rv = rv * sizeof(void *))
     * FIXME: THIS IS NOT SUFFICIENT FOR CROSS COMPILING FOR ARCHITECTURES THAT
     * HAVE ADDRESS SIZES sizeof(void *)
     */
    tree addr_size = build_int_cst(integer_type_node, sizeof(void *));
    gimp = gimple_build_assign_with_ops(MULT_EXPR, rv, rv, addr_size);
    gsi_insert_before(&gsi, gimp, GSI_SAME_STMT);

    fn = create_tmp_var(pp_type, "fn_tmp");
    fn = make_ssa_name(fn, NULL);
    gimp = gimple_build_assign_with_ops(PLUS_EXPR, fn, tmp, rv);
    gsi_insert_before(&gsi, gimp, GSI_SAME_STMT);

    /* the_fn = *fn */
    tree f = build_pointer_type(fn_ptr_type); 
    tree the_fn = create_tmp_var(f, "the_func_ptr");
    the_fn = make_ssa_name(the_fn, NULL);
    gimp = gimple_build_assign(the_fn, build_simple_mem_ref(fn));
    gsi_insert_before(&gsi, gimp, GSI_SAME_STMT);

    /* call the_fn */
    gimp = gimple_build_call(the_fn, 0);
    gsi_insert_before(&gsi, gimp, GSI_SAME_STMT);

#ifdef GOAT_DEBUG
    debug_function(cfun->decl, 0);
#endif
}


/* Insert a call to the runtime function "__slimer_add_fn" which will add the
 * "junk" function created at compile-time to an array at runtime
 */
static void insert_add_fn(gimple stmt, int index)
{
    tree fn;
    gimple call;
    gimple_stmt_iterator gsi;
    static tree decl, proto, idx;

    if (!decl || !proto)
    {
        proto = build_function_type_list(void_type_node, ptr_type_node,
                                         integer_type_node, NULL_TREE);
        decl = build_fn_decl("__slimer_add_fn", proto);
    
        /* Add this fndecl to our list of things we do not process */
        VEC_safe_push(tree, gc, analyized_fns, decl);
    }

    /* Create a constant value and pointer to the function we are to add */
    idx = build_int_cst(integer_type_node, index);
    fn = build_addr(VEC_index(tree, fakes, index), NULL_TREE);
    call = gimple_build_call(decl, 2, fn, idx);
    gsi = gsi_for_stmt(stmt);
    gsi_insert_before(&gsi, call, GSI_NEW_STMT);
}


/* Insert call to __slimer_init to initalize things at runtime */
static void insert_slimer_init(void)
{
    int i;
    gimple stmt;
    tree decl, proto;

    proto = build_function_type_list(void_type_node, integer_type_node, NULL_TREE);
    decl = build_fn_decl("__slimer_init", proto);
    stmt = gsi_stmt(gsi_start_bb(ENTRY_BLOCK_PTR->next_bb));
    insert_call(stmt, decl);

    for (i=0; i<n_funcs; ++i)
      insert_add_fn(stmt, i);

    /* Add this fndecl to our list of things we do not process */
    VEC_safe_push(tree, gc, analyized_fns, decl);
}


static unsigned int slimer_exec(void)
{
    basic_block bb;
    gimple stmt;
    gimple_stmt_iterator gsi;

    if (has_been_processed(cfun->decl))
      return 0;

    if (DECL_EXTERNAL(cfun->decl))
      return 0;

    if (get_identifier(get_name(cfun->decl)) == get_identifier("main"))
      insert_slimer_init();

    /* Go through the basic blocks of this function */
    FOR_EACH_BB(bb)
      for (gsi=gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi))
      {
          stmt = gsi_stmt(gsi);
          if (is_gimple_call(stmt) || is_gimple_assign(stmt))
          {
              /* If its a call to a function we added already (junk or some
               * initlization functions), or a function we have previously
               * analyized, avoid inserting junk data.
               */
              if (is_gimple_call(stmt) &&
                  !has_been_processed(gimple_call_fn(stmt)))
                continue;
              else if ((max_calls > 0) && ((rand() % 2) == 0))
              {
                  insert_call_to_junk_fn(stmt);
                  --max_calls;
              }
          }
      }

    /* Mark as being analyized so we avoid trying to junkify it again */
    VEC_safe_push(tree, gc, analyized_fns, cfun->decl);
    return 0;
}


static void slimer_init_junk_fns(void *gcc_data, void *user_data)
{
    gen_fake_funcs(n_funcs);
}


static bool slimer_gate(void)
{
    return true;
}


static struct gimple_opt_pass slimer_pass = 
{
    .pass.type = GIMPLE_PASS,
    .pass.name = "slimer",
    .pass.gate = slimer_gate,
    .pass.execute = slimer_exec,
    .pass.todo_flags_finish = TODO_update_ssa|TODO_verify_ssa|TODO_cleanup_cfg,
};


/* Return 0 on success or error code on failure */
int plugin_init(struct plugin_name_args   *info,
                struct plugin_gcc_version *ver)
{
    unsigned i;
    struct register_pass_info pass;

    /* Version check */
    if (strncmp(ver->basever, nopper_ver.basever, strlen("4.6")))
      return -1;

    srand(time(NULL));

    pass.pass = &slimer_pass.pass;
    pass.reference_pass_name = "ssa";
    pass.ref_pass_instance_number = 1;
    pass.pos_op = PASS_POS_INSERT_AFTER;

    register_callback("slimer", PLUGIN_START_UNIT, &slimer_init_junk_fns, NULL);
    register_callback("slimer", PLUGIN_PASS_MANAGER_SETUP, NULL, &pass);
    register_callback("slimer", PLUGIN_INFO, NULL, &slimer_info);

    /* Get the args */
    for (i=0; i<info->argc; ++i)
      if (strncmp("numfuncs", info->argv[i].key, 8) == 0)
        n_funcs = atoi(info->argv[i].value);
      else if (strncmp("maxcalls", info->argv[i].key, 8) == 0)
        max_calls = atoi(info->argv[i].value);

    /* If bad arguments, do nothing */
    if (n_funcs < 0)
      n_funcs = 0;

    if (max_calls < 0 || n_funcs == 0)
       max_calls = 0;

    /* Let the user know how stupid they are */
    printf(TAG "Number of junk functions to generate: %d\n"
           TAG "Maximum number of calls to junk functions: %d\n",
           n_funcs, max_calls);

    return 0;
}
