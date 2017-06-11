#include "bukalisp/parser.h"
#include "symtbl.h"
#include <fstream>
#include <memory>

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
        struct Slot
        {
            lilvm::Sym    *sym;
            size_t  idx;

            Slot() : sym(0), idx(0) {}
            Slot(lilvm::Sym *s, size_t i) : sym(s), idx(i) { }
        };
        std::shared_ptr<Environment>            m_parent;
        std::vector<Slot>                       m_env;

        Environment(std::shared_ptr<Environment> parent)
            : m_parent(parent)
        {
        }

        size_t new_env_pos_for(lilvm::Sym *sym)
        {
            Slot slot;
            if (get_env_pos_for(sym, slot))
                return slot.idx;

            slot = Slot(sym, m_env.size());
            m_env.push_back(slot);
            return slot.idx;
        }

        bool get_env_pos_for(lilvm::Sym *sym, Slot &slot)
        {
            for (auto &p : m_env)
            {
                if (p.sym == sym)
                {
                    slot = p;
                    return true;
                }
            }

            return false;
        }
    };

    class ASTJSONCodeEmitter
    {
        private:
            std::ostream                   *m_out_stream;
            lilvm::SymTable                *m_symtbl;
            std::shared_ptr<Environment>    m_env;

            void emit_line(const std::string &line);
            void emit_json_op(const std::string &op,
                              const std::string &operands = "");
            void emit_op(const std::string &op,
                         const std::string &arg1 = "",
                         const std::string &arg2 = "");

        public:
            ASTJSONCodeEmitter()
                : m_out_stream(nullptr),
                  m_symtbl(nullptr)
            {}

            void set_symbol_table(lilvm::SymTable *symtbl)
            {
                m_symtbl = symtbl;
            }

            void set_output(std::ostream *o) { m_out_stream = o; }
            void emit_binary_op(ASTNode *n);
            void emit_form(ASTNode *n);
            void emit_let(ASTNode *n);
            void emit_list(ASTNode *n);
            void emit(ASTNode *n);
            void emit_json(ASTNode *n);
    };

};
