#include "bukalisp/parser.h"
#include "symtbl.h"
#include <fstream>
#include <memory>
#include "strutil.h"

namespace bukalisp
{
    // [10 23.4 symabc (1 2 3) . . . . . . .]
    //
    // (let ((a 10) (b 20)) ....)
    // => [   10    20]
    //    a => 0
    //    b =>       1
    struct Environment
    {
        enum SlotType
        {
            SLT_VAR,
            SLT_PRIM
        };
        struct Slot
        {
            lilvm::Sym    *sym;
            size_t         idx;
            SlotType       type;

            Slot() : sym(0), idx(0), type(SLT_VAR) {}
            Slot(lilvm::Sym *s, size_t i) : sym(s), idx(i), type(SLT_VAR) { }
            Slot(lilvm::Sym *s, size_t i, SlotType t) : sym(s), idx(i), type(t) { }
        };
        std::shared_ptr<Environment>            m_parent;
        std::vector<Slot>                       m_env;

        Environment(std::shared_ptr<Environment> parent)
            : m_parent(parent)
        {
        }

        size_t new_env_pos_for(lilvm::Sym *sym, SlotType type = SLT_VAR)
        {
            Slot slot;
            size_t depth = 0;
            if (get_env_pos_for(sym, slot, depth))
            {
                if (depth <= 0)
                    return slot.idx;
            }

            slot = Slot(sym, m_env.size(), type);
            m_env.push_back(slot);
            return slot.idx;
        }

        bool get_env_pos_for(lilvm::Sym *sym, Slot &slot, size_t &depth)
        {
            for (auto &p : m_env)
            {
                if (p.sym == sym)
                {
                    slot = p;
                    return true;
                }
            }

            if (m_parent)
            {
                depth++;
                return m_parent->get_env_pos_for(sym, slot, depth);
            }

            return false;
        }
    };

    class OutputPad
    {
        public:
            struct OpText
            {
                size_t      m_line;
                std::string m_input_name;
                std::string m_op_text;
                size_t      m_target_lbl_nr;
                size_t      m_lbl_nr;

                OpText(ASTNode *n, const std::string &text);
            };

        private:
            ASTNode                                *m_node;
            std::unique_ptr<std::vector<OpText>>    m_ops;
            size_t                                  m_next_with_label;

        public:
            OutputPad(ASTNode *n)
                : m_node(n), m_ops(std::make_unique<std::vector<OpText>>()),
                  m_next_with_label(0)
            {
            }


        size_t c_ops() { return m_ops->size(); }
        void add_str(const std::string &op, std::string arg1 = "");
        void add(const std::string &op, std::string arg1 = "", std::string arg2 = "");
        void add_label_op(const std::string &op, size_t label_nr)
        {
            OpText ot(m_node, op);
            ot.m_target_lbl_nr = label_nr;
            m_ops->push_back(ot);
        }
        void def_label_for_next(size_t label_nr)
        {
            m_next_with_label = label_nr;
        }
        void append(OutputPad &p)
        {
            for (auto &op : *p.m_ops)
                m_ops->push_back(op);
        }

        void calc_label_addr(std::vector<int64_t> &lbls)
        {
            size_t i_max_label = 0;
            for (auto &op : *m_ops)
            {
                if (op.m_lbl_nr)
                {
                    size_t lblnr = op.m_lbl_nr;
                    if (lblnr > i_max_label)
                        i_max_label = lblnr;
                }
            }

            lbls.resize(i_max_label + 1, 0);

            int idx = 0;
            for (auto &op : *m_ops)
            {
                if (op.m_lbl_nr)
                    lbls[op.m_lbl_nr] = idx;
                idx++;
            }
        }

        void output(std::ostream &o)
        {
            std::vector<int64_t> lbls;
            calc_label_addr(lbls);

            int idx = 0;
            for (auto &op : *m_ops)
            {
                std::string op_text = op.m_op_text;

                if (op.m_target_lbl_nr > 0)
                {
                    int64_t lbl_offs = lbls[op.m_target_lbl_nr] - idx;
                    op_text =
                        "[\"" + op_text + "\", " + std::to_string(lbl_offs) + "],";
                }

                std::string info = op.m_input_name + ":" + std::to_string(op.m_line);
                info = str_replace(str_replace(info, "\\", "\\\\"), "\"", "\\\"");
                o << "       [\"#DEBUG_SYM\",\"" << info << "\"], " << std::endl;
                o << op_text;
                idx++;
            }
            o << std::endl;
        }
    };

    class ASTJSONCodeEmitter
    {
        private:
            std::ostream                   *m_out_stream;
            lilvm::SymTable                *m_symtbl;
            std::shared_ptr<Environment>    m_env;
            std::shared_ptr<Environment>    m_root;
            size_t                          m_lbl_counter;
            bool                            m_create_debug_info;
//            std::vector<std::pair<size_t, size_t>>
//                                            m_loop_stack;

            OutputPad emit_block(ASTNode *n, size_t offs);
            OutputPad emit_atom(ASTNode *n);

        public:
            ASTJSONCodeEmitter()
                : m_out_stream(nullptr),
                  m_symtbl(nullptr),
                  m_lbl_counter(0),
                  m_create_debug_info(false)
            {
                m_root = std::make_shared<Environment>(std::shared_ptr<Environment>());
            }

            size_t new_label() { return ++m_lbl_counter; }

            void push_root_primtive(const std::string &primname)
            {
                m_root->new_env_pos_for(
                    m_symtbl->str2sym(primname), Environment::SLT_PRIM);
            }

            void set_symtbl(lilvm::SymTable *symtbl)
            {
                m_symtbl = symtbl;
            }

            void set_debug(bool db) { m_create_debug_info = db; }

            void set_output(std::ostream *o) { m_out_stream = o; }
            OutputPad emit_binary_op(ASTNode *n);
            OutputPad emit_form(ASTNode *n);
            OutputPad emit_let(ASTNode *n);
            OutputPad emit_if(ASTNode *n);
            OutputPad emit_while(ASTNode *n);
            OutputPad emit_when(ASTNode *n);
            OutputPad emit_unless(ASTNode *n);
            OutputPad emit_list(ASTNode *n);
            OutputPad emit(ASTNode *n);
            void emit_json(ASTNode *n);
    };

};
