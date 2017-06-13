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

                OpText(ASTNode *n, const std::string &text);
            };

        private:
            ASTNode                                *m_node;
            std::unique_ptr<std::vector<OpText>>    m_ops;

        public:
            OutputPad(ASTNode *n)
                : m_node(n), m_ops(std::make_unique<std::vector<OpText>>())
            {
            }


        void add_str(const std::string &op, std::string arg1 = "");
        void add(const std::string &op, std::string arg1 = "", std::string arg2 = "");
        void append(OutputPad &p)
        {
            for (auto &op : *p.m_ops)
                m_ops->push_back(op);
        }

        void output(std::ostream &o)
        {
            for (auto &op : *m_ops)
            {
                std::string info = op.m_input_name + ":" + std::to_string(op.m_line);
                info = str_replace(str_replace(info, "\\", "\\\\"), "\"", "\\\"");
                o << "       [\"#DEBUG_SYM\",\"" << info << "\"], " << std::endl;
                o << op.m_op_text;
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

            OutputPad emit_block(ASTNode *n, size_t offs);
            OutputPad emit_atom(ASTNode *n);

        public:
            ASTJSONCodeEmitter()
                : m_out_stream(nullptr),
                  m_symtbl(nullptr)
            {
                m_root = std::make_shared<Environment>(std::shared_ptr<Environment>());
            }

            void push_root_primtive(const std::string &primname)
            {
                m_root->new_env_pos_for(
                    m_symtbl->str2sym(primname), Environment::SLT_PRIM);
            }

            void set_symtbl(lilvm::SymTable *symtbl)
            {
                m_symtbl = symtbl;
            }

            void set_output(std::ostream *o) { m_out_stream = o; }
            OutputPad emit_binary_op(ASTNode *n);
            OutputPad emit_form(ASTNode *n);
            OutputPad emit_let(ASTNode *n);
            OutputPad emit_list(ASTNode *n);
            OutputPad emit(ASTNode *n);
            void emit_json(ASTNode *n);
    };

};
