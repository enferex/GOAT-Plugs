define pg
call debug_generic_stmt($arg0)
end

define pgimp
call debug_gimple_stmt($arg0)
end

define pt
call debug_tree($arg0)
end

define pf
pg cfun->decl
end

define dcf
call debug_function(cfun->decl, 0)
end

define dbm
call debug_bitmap($arg0)
end
