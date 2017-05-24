#include "bukalisp/parser.h"
#include <fstream>

namespace bukalisp
{
    class ASTJSONCodeEmitter
    {
        private:
            std::fstream    *m_out_file_stream;

            void emit_line(const std::string &line);
            void emit_json_op(const std::string &op,
                              const std::string &operands = "");

        public:
            ASTJSONCodeEmitter() : m_out_file_stream(nullptr) {}
            void open_output_file(const std::string &path);
            void emit_binary_op(ASTNode *n);
            void emit_form(ASTNode *n);
            void emit(ASTNode *n);
            void emit_json(ASTNode *n);
    };

};
