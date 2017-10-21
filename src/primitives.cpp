// Copyright (C) 2017 Weird Constructor
// For more license info refer to the the bottom of this file.

#define BIN_OP_LOOPS(op) \
        if (args.m_len > 1) { \
            if (out.m_type == T_DBL) { \
                double &o = out.m_d.d; \
                for (size_t i = 1; i < args.m_len; i++) \
                    o = o op args.m_data[i].to_dbl(); \
            } else { \
                int64_t &o = out.m_d.i; \
                for (size_t i = 1; i < args.m_len; i++) \
                    o = o op args.m_data[i].to_int(); \
            } \
        }

#define NO_ARG_NIL if (args.m_len <= 0) { out = Atom(); return; }

START_PRIM()
    NO_ARG_NIL;
    out = args.m_data[0];
    BIN_OP_LOOPS(+)
END_PRIM(+);

START_PRIM()
    NO_ARG_NIL;
    out = args.m_data[0];
    BIN_OP_LOOPS(*)
END_PRIM(*);

START_PRIM()
    NO_ARG_NIL;
    out = args.m_data[0];
    if (args.m_len == 1)
    {
        if (out.m_type == T_DBL) out.m_d.d = 1.0 / out.m_d.d;
        else                     out.m_d.i = 1 / out.m_d.i;
        return;
    }
    BIN_OP_LOOPS(/)
END_PRIM(/);

START_PRIM()
    NO_ARG_NIL;
    out = args.m_data[0];
    if (args.m_len == 1)
    {
        if (out.m_type == T_DBL) out.m_d.d *= -1.0;
        else                     out.m_d.i *= -1;
        return;
    }
    BIN_OP_LOOPS(-)
END_PRIM(-);

#define REQ_GT_ARGC(prim, cnt)    if (args.m_len < cnt) error("Not enough arguments to " #prim ", expected " #cnt);
#define REQ_EQ_ARGC(prim, cnt)    if (args.m_len != cnt) error("Wrong number of arguments to " #prim ", expected " #cnt);
#define BIN_CMP_OP_NUM(op) \
    out = Atom(T_BOOL); \
    Atom last = args.m_data[0]; \
    for (size_t i = 1; i < args.m_len; i++) \
    { \
        if (last.m_type == T_DBL) \
            out.m_d.b = last.m_d.d op args.m_data[i].m_d.d; \
        else \
            out.m_d.b = last.m_d.i op args.m_data[i].m_d.i; \
        if (!out.m_d.b) return; \
        last = args.m_data[i]; \
    }

START_PRIM()
    REQ_GT_ARGC(=, 2);
    BIN_CMP_OP_NUM(==);
END_PRIM(=);
START_PRIM()
    REQ_GT_ARGC(<, 2);
    BIN_CMP_OP_NUM(<);
END_PRIM(<);
START_PRIM()
    REQ_GT_ARGC(>, 2);
    BIN_CMP_OP_NUM(>);
END_PRIM(>);
START_PRIM()
    REQ_GT_ARGC(<=, 2);
    BIN_CMP_OP_NUM(<=);
END_PRIM(<=);
START_PRIM()
    REQ_GT_ARGC(>=, 2);
    BIN_CMP_OP_NUM(>=);
END_PRIM(>=);

#define A0  (args.m_data[0])
#define A1  (args.m_data[1])
#define A2  (args.m_data[2])
#define A3  (args.m_data[3])

START_PRIM()
    out = Atom(T_VEC);
    AtomVec *l = m_rt->m_gc.allocate_vector(args.m_len);
    out.m_d.vec = l;
    for (size_t i = 0; i < args.m_len; i++)
        l->m_data[i] = args.m_data[i];
    l->m_len = args.m_len;
END_PRIM(list);

START_PRIM()
    REQ_EQ_ARGC(eq?, 2);
    out = Atom(T_BOOL);
    out.m_d.b = A0.eqv(A1);
END_PRIM(eqv?);

START_PRIM()
    REQ_EQ_ARGC(not, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.is_false();
END_PRIM(not);

START_PRIM()
    REQ_EQ_ARGC(symbol?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_SYM;
END_PRIM(symbol?);

START_PRIM()
    REQ_EQ_ARGC(keyword?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_KW;
END_PRIM(keyword?);

START_PRIM()
    REQ_EQ_ARGC(nil?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_NIL;
END_PRIM_DOC(nil?,
"@lists procedure (nil? _value_)\n"
"\n"
"Returns true if _value_ is the `nil` value.\n"
"And only then.\n"
"\n"
"    (nil?   nil)     ;=> #t\n"
"    (nil?   [])      ;=> #f\n"
"    (nil?   '())     ;=> #f\n"
"    (nil?   {})      ;=> #f\n"
"    (nil?   0)       ;=> #f\n"
"    (nil?   "")      ;=> #f\n"
);

START_PRIM()
    REQ_EQ_ARGC(null?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_VEC && A0.m_d.vec->m_len == 0;
END_PRIM_DOC(null?,
"@lists (null? _list_)\n"
"\n"
"Returns `#t` if _list_ is an empty list.\n"
"Same as `(empty? ...)`.\n"
"\n"
"But don't mix this up with `(nil? ...)`, which detects\n"
"the `nil` value.\n"
"\n"
"    (null?  [1 2 3]) ;=> #f\n"
"    (null?  '())     ;=> #t\n"
"    (null?  [])      ;=> #t\n"
"    (null?  "")      ;=> #t\n"
"\n"
"See also: `nil?`\n"
);

START_PRIM()
    REQ_EQ_ARGC(exact?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_INT;
END_PRIM(exact?);

START_PRIM()
    REQ_EQ_ARGC(inexact?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_DBL;
END_PRIM(inexact?);

START_PRIM()
    REQ_EQ_ARGC(number?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_DBL || A0.m_type == T_INT;
END_PRIM(number?);

START_PRIM()
    REQ_EQ_ARGC(string?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_STR;
END_PRIM(string?);

START_PRIM()
    REQ_EQ_ARGC(boolean?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_BOOL;
END_PRIM(boolean?);

START_PRIM()
    REQ_EQ_ARGC(list?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_VEC;
END_PRIM(list?);

START_PRIM()
    REQ_EQ_ARGC(map?, 1);
    out = Atom(T_BOOL);
    out.m_d.b = A0.m_type == T_MAP;
END_PRIM(map?);

START_PRIM()
    REQ_EQ_ARGC(procedure?, 1);
    out = Atom(T_BOOL);
    out.m_d.b =
           A0.m_type == T_PRIM
        || A0.m_type == T_CLOS
        || (A0.m_type == T_UD && A0.m_d.ud->type() == "VM-PROG");
END_PRIM(procedure?);

START_PRIM()
    REQ_EQ_ARGC(@, 2);
    if (A1.m_type == T_VEC)
    {
        int64_t idx = A0.to_int();
        if (idx < 0) out = Atom();
        else         out = A1.at((size_t) idx);
    }
    else if (A1.m_type == T_MAP)
    {
        out = A1.at(A0);
    }
    else
        error("Can apply '@' only to lists or maps", A1);
END_PRIM(@);

START_PRIM()
    REQ_EQ_ARGC(@!, 3);
    if (A1.m_type == T_VEC)
    {
        int64_t i = A0.to_int();
        if (i >= 0)
            A1.m_d.vec->set((size_t) A0.to_int(), A2);
        out = A2;
    }
    else if (A1.m_type == T_MAP)
    {
        A1.m_d.map->set(A0, A2);
        out = A2;
    }
    else
        error("Can apply '@!' only to lists or maps", A1);
END_PRIM(@!);

START_PRIM()
    REQ_EQ_ARGC(type, 1);
    out = Atom(T_SYM);
    switch (A0.m_type)
    {
        // TODO: use X macro!
        case T_NIL:    out.m_d.sym = m_rt->m_gc.new_symbol("nil");       break;
        case T_INT:    out.m_d.sym = m_rt->m_gc.new_symbol("exact");     break;
        case T_DBL:    out.m_d.sym = m_rt->m_gc.new_symbol("inexact");   break;
        case T_BOOL:   out.m_d.sym = m_rt->m_gc.new_symbol("boolean");   break;
        case T_STR:    out.m_d.sym = m_rt->m_gc.new_symbol("string");    break;
        case T_SYM:    out.m_d.sym = m_rt->m_gc.new_symbol("symbol");    break;
        case T_KW:     out.m_d.sym = m_rt->m_gc.new_symbol("keyword");   break;
        case T_VEC:    out.m_d.sym = m_rt->m_gc.new_symbol("list");      break;
        case T_MAP:    out.m_d.sym = m_rt->m_gc.new_symbol("map");       break;
        case T_PRIM:   out.m_d.sym = m_rt->m_gc.new_symbol("procedure"); break;
        case T_SYNTAX: out.m_d.sym = m_rt->m_gc.new_symbol("syntax");    break;
        case T_CLOS:   out.m_d.sym = m_rt->m_gc.new_symbol("procedure"); break;
        case T_UD:     out.m_d.sym = m_rt->m_gc.new_symbol("userdata");  break;
        default:       out.m_d.sym = m_rt->m_gc.new_symbol("<unknown>"); break;
    }
END_PRIM(type)

START_PRIM()
    REQ_EQ_ARGC(length, 1);
    out = Atom(T_INT);
    if (A0.m_type == T_VEC)         out.m_d.i = A0.m_d.vec->m_len;
    else if (A0.m_type == T_MAP)    out.m_d.i = A0.m_d.map->m_map.size();
    else                            out.m_d.i = 0;
END_PRIM(length)

START_PRIM()
    REQ_EQ_ARGC(push!, 2);

    if (A0.m_type != T_VEC)
        error("Can't push onto something that is not a list", A0);

    A0.m_d.vec->push(A1);
    out = A1;
END_PRIM(push!)

START_PRIM()
    REQ_EQ_ARGC(pop!, 1);

    if (A0.m_type != T_VEC)
        error("Can't pop from something that is not a list", A0);

    Atom *a = A0.m_d.vec->last();
    if (a) out = *a;
    A0.m_d.vec->pop();
END_PRIM(pop!)

START_PRIM()
    REQ_EQ_ARGC(make-vm-prog, 1);
    out = bukalisp::make_prog(m_rt->m_gc, A0);
    if (out.m_type == T_UD)
        m_rt->m_gc.reg_userdata(out.m_d.ud);
END_PRIM(make-vm-prog)

START_PRIM()
    REQ_EQ_ARGC(bkl-prog-serialize, 2);
    out = Atom(T_STR, m_rt->m_gc.new_symbol(atom2cpp(A0.to_display_str(), A1)));
END_PRIM(bkl-prog-serialize)

START_PRIM()
    REQ_EQ_ARGC(run-vm-prog, 2);

    if (A0.m_type != T_UD || A0.m_d.ud->type() != "VM-PROG")
        error("Bad input type to run-vm-prog, expected VM-PROG.", A0);

    if (A1.m_type != T_VEC)
        error("No root env given to run-vm-prog.", A1);

    if (!m_vm)
        error("Can't execute VM progs, no VM instance loaded into interpreter", A0);

    out = m_vm->eval(A0, A1.m_d.vec);
END_PRIM(run-vm-prog)

START_PRIM()
    REQ_EQ_ARGC(get-vm-modules, 0);

    if (!m_vm)
        error("Can't get VM modules, no VM instance loaded into interpreter", A0);

    if (!m_modules)
        error("Can't get VM modules, no modules defined", A0);

    out = Atom(T_MAP, m_modules);
END_PRIM(get-vm-modules);

START_PRIM()
    REQ_GT_ARGC(error, 1);

    Atom a(T_VEC);
    a.m_d.vec = m_rt->m_gc.allocate_vector(args.m_len - 1);
    for (size_t i = 1; i < args.m_len; i++)
        a.m_d.vec->m_data[i - 1] = args.m_data[i];
    a.m_d.vec->m_len = args.m_len - 1;
    error(A0.to_display_str(), a);
END_PRIM(error)

START_PRIM()
    REQ_EQ_ARGC(bkl-error, 2);
    throw
        add_stack_trace_error(
            BukaLISPException(A0.to_display_str())
        .push(A1));
END_PRIM(bkl-error)

START_PRIM()
    REQ_EQ_ARGC(bkl-set-meta, 3);
    m_rt->m_gc.set_meta_register(A0, (size_t) A1.to_int(), A2);
    out = A2;
END_PRIM(bkl-set-meta)

START_PRIM()
    REQ_EQ_ARGC(bkl-get-meta, 1);
    out = A0.meta();
END_PRIM(bkl-get-meta)

START_PRIM()
    REQ_GT_ARGC(display, 1);

    for (size_t i = 0; i < args.m_len; i++)
    {
        std::cout << args.m_data[i].to_display_str();
        if (i < (args.m_len - 1))
            std::cout << " ";
    }

    out = args.m_data[args.m_len - 1];
END_PRIM(display)

START_PRIM()
    REQ_EQ_ARGC(newline, 0);
    std::cout << std::endl;
END_PRIM(newline)

START_PRIM()
    REQ_GT_ARGC(displayln, 1);

    for (size_t i = 0; i < args.m_len; i++)
    {
        std::cout << args.m_data[i].to_display_str();
        if (i < (args.m_len - 1))
            std::cout << " ";
    }
    std::cout << std::endl;

    out = args.m_data[args.m_len - 1];
END_PRIM(displayln)

START_PRIM()
    REQ_EQ_ARGC(atom-id, 1);
    out = Atom(T_INT, A0.id());
END_PRIM(atom-id)

START_PRIM()
    REQ_GT_ARGC(str, 0);
    std::string out_str;
    for (size_t i = 0; i < args.m_len; i++)
        out_str += args.m_data[i].to_display_str();
    out = Atom(T_STR, m_rt->m_gc.new_symbol(out_str));
END_PRIM(str)

START_PRIM()
    REQ_GT_ARGC(str-join, 1);
    std::string sep = A0.to_display_str();
    std::string out_str;
    for (size_t i = 1; i < args.m_len; i++)
    {
        if (i > 1)
            out_str += sep;
        out_str += args.m_data[i].to_display_str();
    }
    out = Atom(T_STR, m_rt->m_gc.new_symbol(out_str));
END_PRIM(str-join)

START_PRIM()
    REQ_EQ_ARGC(symbol->string, 1)
    if (A0.m_type != T_SYM)
        error("'symbol->string' expected symbol as argument", A0);
    out = Atom(T_STR, A0.m_d.sym);
END_PRIM(symbol->string)

START_PRIM()
    REQ_EQ_ARGC(keyword->string, 1)
    if (A0.m_type != T_KW)
        error("'keyword->string' expected keyword as argument", A0);
    out = Atom(T_STR, A0.m_d.sym);
END_PRIM(keyword->string)

START_PRIM()
    REQ_EQ_ARGC(string->symbol, 1)
    if (A0.m_type != T_STR)
        error("'string->symbol' expected string as argument", A0);
    out = Atom(T_SYM, A0.m_d.sym);
END_PRIM(string->symbol)

START_PRIM()
    REQ_EQ_ARGC(string->keyword, 1)
    if (A0.m_type != T_STR)
        error("'string->keyword' expected string as argument", A0);
    out = Atom(T_KW, A0.m_d.sym);
END_PRIM(string->keyword)

START_PRIM()
    REQ_EQ_ARGC(last, 1);
    if (A0.m_type != T_VEC)
        error("Can't 'last' on a non-list", A0);
    AtomVec *avl = A0.m_d.vec;
    if (avl->m_len <= 0)
    {
        out = Atom();
        return;
    }
    out = avl->m_data[avl->m_len - 1];
END_PRIM(last)

START_PRIM()
    REQ_EQ_ARGC(first, 1);
    if (A0.m_type != T_VEC)
        error("Can't 'first' on a non-list", A0);
    out = A0.at(0);
END_PRIM(first)

START_PRIM()
    REQ_EQ_ARGC(take, 2);
    if (A0.m_type != T_VEC)
        error("Can't 'take' on a non-list", A0);
    size_t ti  = (size_t) A1.to_int();
    size_t len = (size_t) A0.m_d.vec->m_len;
    if (ti > len) ti = len;
    AtomVec *av = m_rt->m_gc.allocate_vector(ti);
    for (size_t i = 0; i < len && i < ti; i++)
        av->m_data[i] = A0.m_d.vec->m_data[i];
    av->m_len = ti;
    out = Atom(T_VEC, av);
END_PRIM(take)

START_PRIM()
    REQ_EQ_ARGC(drop, 2);
    if (A0.m_type != T_VEC)
        error("Can't 'drop' on a non-list", A0);
    size_t di  = (size_t) A1.to_int();
    size_t len = (size_t) A0.m_d.vec->m_len;
    if (len < di)
    {
        out = Atom(T_VEC, m_rt->m_gc.allocate_vector(0));
        return;
    }
    len -= di;
    AtomVec *av = m_rt->m_gc.allocate_vector(len);
    for (size_t i = 0; i < len; i++)
        av->m_data[i] = A0.m_d.vec->m_data[i + di];
    av->m_len = len;
    out = Atom(T_VEC, av);
END_PRIM(drop)

START_PRIM()
    REQ_GT_ARGC(append, 0);
    AtomVec *av = m_rt->m_gc.allocate_vector(args.m_len);
    for (size_t i = 0; i < args.m_len; i++)
    {
        Atom &a = args.m_data[i];
        if (a.m_type == T_VEC)
        {
            Atom *v = a.m_d.vec->m_data;
            for (size_t j = 0; j < a.m_d.vec->m_len; j++)
                av->push(v[j]);
        }
        else if (a.m_type == T_MAP)
        {
            AtomMap *map = a.m_d.map;
            for (UnordAtomMap::iterator i = map->m_map.begin();
                 i != map->m_map.end();
                 i++)
            {
                av->push(i->first);
                av->push(i->second);
            }
        }
        else
        {
            av->push(a);
        }
    }
    out = Atom(T_VEC, av);
END_PRIM(append)

START_PRIM()
    REQ_EQ_ARGC(write-str, 1);
    out = Atom(T_STR, m_rt->m_gc.new_symbol(A0.to_write_str()));
END_PRIM(write-str);

START_PRIM()
    REQ_EQ_ARGC(pp-str, 1);
    out = Atom(T_STR, m_rt->m_gc.new_symbol(A0.to_write_str(true)));
END_PRIM(pp-str);

START_PRIM()
    REQ_GT_ARGC(read-str, 1);
    if (args.m_len > 1)
        out = m_rt->read(A1.to_display_str(), A0.to_display_str());
    else
        out = m_rt->read("", A0.to_display_str());
END_PRIM(read-str)

START_PRIM()
    REQ_EQ_ARGC(size, 1);
    out = Atom(T_INT, (int64_t) A0.size());
END_PRIM(size)

START_PRIM()
    REQ_EQ_ARGC(sys-slurp-file, 1);
    if (   A0.m_type != T_STR
        && A0.m_type != T_SYM
        && A0.m_type != T_KW)
    {
        error("'bkl-slurp-file' requires a string, symbol "
              "or keyword as first argument.", A0);
    }
    out = Atom(T_STR, m_rt->m_gc.new_symbol(slurp_str(A0.m_d.sym->m_str)));
END_PRIM(sys-slurp-file)

START_PRIM()
    REQ_EQ_ARGC(sys-path-separator, 0);
#if defined(WIN32) || defined(_WIN32) 
    out = Atom(T_STR, m_rt->m_gc.new_symbol("\\"));
#else
    out = Atom(T_STR, m_rt->m_gc.new_symbol("/"));
#endif
END_PRIM(sys-path-separator)

START_PRIM()
    REQ_EQ_ARGC(sys-path-split, 1);
    std::string s = A0.to_display_str();
    size_t pos = s.find_last_of("/\\");
    AtomVec *av = m_rt->m_gc.allocate_vector(2);
    if (pos == string::npos)
    {
        av->push(Atom(T_STR, m_rt->m_gc.new_symbol(s)));
        av->push(Atom(T_STR, m_rt->m_gc.new_symbol("")));
    }
    else
    {
        av->push(Atom(T_STR, m_rt->m_gc.new_symbol(s.substr(0, pos))));
        av->push(Atom(T_STR, m_rt->m_gc.new_symbol(s.substr(pos + 1))));
    }
    out = Atom(T_VEC, av);
END_PRIM(sys-path-split)

START_PRIM()
    REQ_GT_ARGC(apply, 1);

    if (!m_vm)
        error("Can't run 'apply' without VM.", A0);

    cout << "APPLY PRIM: " << Atom(T_VEC, &args).to_write_str() << endl;

    if (args.m_len == 1)
    {
        AtomVec *av = m_rt->m_gc.allocate_vector(0);
        out = m_vm->eval(A0, nullptr, av);
    }
    else if (args.m_len == 2)
    {
        if (A1.m_type != T_VEC)
            error("'apply' No argument list given.", A1);
        out = m_vm->eval(A0, nullptr, A1.m_d.vec);
    }
    else
    {
        if (args.m_data[args.m_len - 1].m_type != T_VEC)
        {
            error("'apply' last argument needs to be a list!",
                  args.m_data[args.m_len - 1]);
        }

        AtomVec *av_last = args.m_data[args.m_len - 1].m_d.vec;
        AtomVec *av =
            m_rt->m_gc.allocate_vector(args.m_len + av_last->m_len);

        for (size_t i = 1; i < args.m_len - 1; i++)
            av->push(args.m_data[i]);

        for (size_t i = 0; i < av_last->m_len; i++)
            av->push(av_last->m_data[i]);

        out = m_vm->eval(A0, nullptr, av);
    }
END_PRIM_DOC(apply,
"@control procedure (apply _proc_ [_arg1_ ... _argN_] [...])\n"
"\n"
"Calls _proc_ with the elements of the appended lists as argument.\n"
"If there are more arguments than _proc_ then the last argument\n"
"always must be a list.\n"
"\n"
"    (apply str-join \",\" [\"X\" \"Y\"]) ;=> \"X,Y\"\n"
)

START_PRIM()
    REQ_EQ_ARGC(empty?, 1);
    out.m_type = T_BOOL;
    if (A0.m_type == T_VEC)
        out.m_d.b = A0.m_d.vec->m_len == 0;
    else if (A0.m_type == T_MAP)
        out.m_d.b = A0.m_d.map->m_map.empty();
    else
        error("Expected map or list to 'empty?'", A0);
END_PRIM_DOC(empty?,
"@lists (empty? _list_)\n"
"\n"
"Returns `#t` if _list_ is an empty list.\n"
"Same as `(null? ...)`.\n"
"\n"
"    (empty?  [1 2 3]) ;=> #f\n"
"    (empty?  '())     ;=> #t\n"
"    (empty?  [])      ;=> #t\n"
"    (empty?  "")      ;=> #t\n"
)

START_PRIM()
    REQ_EQ_ARGC(bkl-gc-statistics, 0);
    m_rt->m_gc.collect();
    out = m_rt->m_gc.get_statistics();
END_PRIM_DOC(bkl-gc-statistics,
"@internal procedure (bkl-gc-statistics)\n"
"\n"
"Performs a garbage collection and returns some statistic values\n"
"in a map.\n"
)

START_PRIM()
    REQ_EQ_ARGC(bkl-primitive-map, 0);
    out = Atom(T_MAP, m_rt->m_gc.allocate_map());
    AtomVec *prim_sym_tbl = m_vm->get_primitive_symbol_table();
    for (size_t i = 0; i < prim_sym_tbl->m_len; i++)
        out.m_d.map->set(prim_sym_tbl->m_data[i], Atom(T_INT, i));
END_PRIM_DOC(bkl-primitive-map,
"@internal procedure (bkl-primitive-map)\n"
"\n"
"Returns a map containing the names of the primitives and their\n"
"indices for the byte code.\n"
)

START_PRIM()
    REQ_EQ_ARGC(bkl-set-doc!, 2);
    m_vm->set_documentation(A0, A1);
    out = A1;
END_PRIM_DOC(bkl-set-doc!,
"@documentation procedure (bkl-set-doc! _func-name-sym-or-kw_ _doc-string_\n"
"\n"
"Stores _doc-string_ in the central documentation library.\n"
"This function is mainly useful if you want to annotate your\n"
"own code with documenation. BukaLISP usually registers everything itself.\n"
"\n"
"See also: `doc?`\n"
)

START_PRIM()
    REQ_EQ_ARGC(list-copy, 1);
    if (A0.m_type != T_VEC)
        error("'list-copy' can only copy lists", A0);
    out.set_vec(m_rt->m_gc.clone_vector(A0.m_d.vec));
END_PRIM_DOC(list-copy,
"@lists procedure (list-copy _list_)\n"
"\n"
"Makes a shallow clone of the _list_.\n"
)

START_PRIM()
    REQ_EQ_ARGC(map-copy, 1);
    if (A0.m_type != T_MAP)
        error("'map-copy' can only copy maps", A0);
    out.set_map(m_rt->m_gc.clone_map(A0.m_d.map));
END_PRIM_DOC(map-copy,
"@maps procedure (map-copy _map_)\n"
"\n"
"Makes a shallow clone of the _map_.\n"
)

START_PRIM()
    REQ_EQ_ARGC(assign, 2);

    if (   A0.m_type != T_MAP
        && A0.m_type != T_VEC)
        error("'assign' can only assign to maps or lists", A0);

    if (A1.m_type == T_MAP)
    {
        auto &src_map = A1.m_d.map->m_map;

        if (A0.m_type == T_MAP)
        {
            AtomMap *om = m_rt->m_gc.clone_map(A0.m_d.map);
            for (UnordAtomMap::iterator i = src_map.begin();
                 i != src_map.end();
                 i++)
            {
                om->set(i->first, i->second);
            }
            out.set_map(om);
        }
        else
        {
            AtomVec *av = m_rt->m_gc.clone_vector(A0.m_d.vec);
            for (UnordAtomMap::iterator i = src_map.begin();
                 i != src_map.end();
                 i++)
            {
                if (i->first.m_type != T_INT
                    && i->first.m_type != T_DBL)
                {
                    Atom e = i->first;
                    error("'assign' bad index in assignment, "
                          "expected int or dbl",
                          e);
                }
                av->set((size_t) i->first.to_int(), i->second);
            }
            out.set_vec(av);
        }
    }
    else if (A1.m_type == T_VEC)
    {
        if (A1.m_d.vec->m_len % 2 != 0)
            error("'assign' can only use an assignments list with "
                  "an even number of elements.", A0);

        AtomVec *src_vec = A1.m_d.vec;
        if (A0.m_type == T_MAP)
        {
            AtomMap *om = m_rt->m_gc.clone_map(A0.m_d.map);
            for (size_t i = 0; i < src_vec->m_len; i += 2)
                om->set(src_vec->m_data[i], src_vec->m_data[i + 1]);
            out.set_map(om);
        }
        else
        {
            AtomVec *av = m_rt->m_gc.clone_vector(A0.m_d.vec);
            for (size_t i = 0; i < src_vec->m_len; i += 2)
            {
                if (src_vec->m_data[i].m_type != T_INT
                    && src_vec->m_data[i].m_type != T_DBL)
                    error("'assign' bad index in assignment, "
                          "expected int or dbl",
                          src_vec->m_data[i]);
                av->set((size_t) src_vec->m_data[i].to_int(), src_vec->m_data[i + 1]);
            }
            out.set_vec(av);
        }
    }
END_PRIM_DOC(assign,
"@maps procedure (assign _destination-map_ _assignments-map_)\n"
"@lists procedure (assign _destination-list_ _assignments-list_)\n"
"\n"
"This procedure creates a clone of _destination-map/-list_\n"
"with the keys defined by the _assignments-map/-list_.\n"
"This is useful if you want to copy and change a list or map at the same time.\n"
"\n"
"    (assign {a: 20 b: 10} {a: 10})\n"
"    ;=> {a: 10 b: 10} where the output map is a shallow clone.\n"
"\n"
"    ; alternative:\n"
"    (assign {a: 20 b: 10} [a: 10])\n"
"\n"
"    (assign [0 1 2 3] [0 10 2 20])\n"
"    ;=> [10 1 20 3] where the output list is a shallow cone.\n"
"\n"
)

START_PRIM()
    REQ_EQ_ARGC(?doc, 1);
    const std::string search = A0.to_display_str();
    if (search.size() <= 0)
        error("'bkl-doc' can't deal with empty search strings!", A0);
    Atom doc = m_vm->get_documentation();
    AtomVec *found = m_rt->m_gc.allocate_vector(0);
    for (auto &a : doc.m_d.map->m_map)
    {
        if (   a.first.m_type != T_SYM
            && a.first.m_type != T_KW)
            continue;
        if (a.second.m_type != T_STR)
            continue;
        const std::string &funcname = a.first.m_d.sym->m_str;
        const std::string &docstr   = a.second.m_d.sym->m_str;

        if (search[0] == '*')
        {
            if (   docstr.find(search.substr(1))   != std::string::npos
                || funcname.find(search.substr(1)) != std::string::npos)
                found->push(a.second);
        }
        else if (search[0] == '?')
        {
            size_t p = docstr.find("\n");
            if (p == std::string::npos)
            {
                if (docstr.find(search.substr(1))
                    != std::string::npos)
                    found->push(a.second);
            }
            else
            {
                if (docstr.substr(0, p).find(search.substr(1))
                    != std::string::npos)
                    found->push(a.second);
            }
        }
        else
        {
            if (funcname.find(search) != std::string::npos)
                found->push(a.second);
        }
    }
    out = Atom(T_VEC, found);
END_PRIM_DOC(?doc,
"@documenation procedure (?doc \"*_search-str_\")\n"
"@documenation procedure (?doc \"?_search-str_\")\n"
"@documenation procedure (?doc \"_search-str_\")\n"
"\n"
"This procedure retrieves documentation from the internal documentation\n"
"database. It returns a list of all matching documentation strings\n"
"\n"
"If _search-str_ starts with a '*' the key (function name) and\n"
"the documentation string for that key will be checked if they contain\n"
"_search-str_.\n"
"If _search-str_ starts with a '?' only the first line in the documentation\n"
"will be checked if it contains _search-str_.\n"
"Otherwise only the keys in the database will be checked whether they\n"
"contain _search-str.\n"
"\n"
"Instead of strings you can also use symbols or keywords as search string,"
"like this:\n"
"\n"
"    (?doc ?doc:) ;=> [\"@documentation procedure (?doc ...\"];  \n"
"\n"
"\n"
"See also: `bkl-set-doc!`\n"
)

#if IN_INTERPRETER

START_PRIM()
    REQ_GT_ARGC(eval, 1);
    if (args.m_len > 1)
    {
        if (A1.m_type != T_MAP)
            error("Can invoke 'eval' only with a map as env", A1);
        out = eval(A0, A1.m_d.map);
    }
    else
        out = eval(A0);
END_PRIM(eval)

START_PRIM()
    REQ_EQ_ARGC(interaction-environment, 0);
    out = Atom(T_MAP, init_root_env());
END_PRIM(interaction-environment)

// (invoke-compiler code-name code only-compile root-env)
// (invoke-compiler [code] code-name only-compile root-env)
START_PRIM()
    REQ_GT_ARGC(invoke-compiler, 4);
    if (A3.m_type != T_VEC)
        error("invoke-compiler needs a root-env (<map>, <storage>) "
              "to compile and evaluate in", A3);

    if (A0.m_type == T_STR)
        out =
            this->call_compiler(
                A1.to_display_str(), A0.to_display_str(),
                A3.m_d.vec, !A2.is_false());
    else
        out =
            this->call_compiler(
                A0, A3.m_d.vec, A1.to_display_str(), !A2.is_false());
END_PRIM(invoke-compiler)

#else

START_PRIM()
    REQ_GT_ARGC(eval, 1);
    Atom exprs(T_VEC, m_rt->m_gc.allocate_vector(1));
    exprs.m_d.vec->push(A0);

    AtomVec *env = nullptr;
    if (args.m_len > 1)
    {
        if (A1.m_type != T_VEC)
            error("Can invoke 'eval' only with a vec as env", A1);
        env = A1.m_d.vec;
    }
    else
    {
        env = m_rt->m_gc.allocate_vector(2);
    }

    Atom prog =
        m_compiler_call(exprs, env, "eval", true);
    if (env->m_len < 2 || env->m_data[1].m_type != T_VEC)
        error("Compiler returned bad environment in 'eval'", Atom(T_VEC, env));
    out = eval(prog, env->m_data[1].m_d.vec);
END_PRIM(eval)

START_PRIM()
    REQ_EQ_ARGC(interaction-environment, 0);
    GC_ROOT_VEC(m_rt->m_gc, root_env) = m_rt->m_gc.allocate_vector(2);
    m_compiler_call(Atom(T_INT, 42), root_env, "env-gen", true);
    out = Atom(T_VEC, root_env);
END_PRIM(interaction-environment)

START_PRIM()
    REQ_GT_ARGC(invoke-compiler, 4);
    if (A3.m_type != T_VEC)
        error("invoke-compiler needs a root-env (<map>, <storage>) "
              "to compile and evaluate in", A3);

    if (A0.m_type == T_STR)
    {
        GC_ROOT(m_rt->m_gc, input) =
            m_rt->read(A1.to_display_str(), A0.to_display_str());
        out =
            m_compiler_call(
                input, A3.m_d.vec, A1.to_display_str(), !A2.is_false());
    }
    else
        out =
            m_compiler_call(
                A0, A3.m_d.vec, A1.to_display_str(), !A2.is_false());
END_PRIM(invoke-compiler)


#endif

/******************************************************************************
* Copyright (C) 2017 Weird Constructor
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
