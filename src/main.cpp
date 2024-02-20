#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <string.h>
#include <ast.h>
#include <riscv.h>

extern FILE *yyin;
extern int yyparse(std::unique_ptr<BaseAST> &ast);

int main(int argc, const char *argv[]) {
  srand(1);

  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  assert(!strcmp(mode, "-koopa") || !strcmp(mode, "-riscv") || !strcmp(mode, "-perf"));

  std::ifstream input_file(input);
  char temp_char;
  std::cout << "Input file:" << std::endl << std::endl;
  while (true)
  {
    input_file.get(temp_char);
    if (input_file.eof())
      break;
    std::cout << temp_char;
  }
  std::cout << std::endl << "end of input" << std::endl << std::endl;

  yyin = fopen(input, "r");
  assert(yyin);

  std::unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  if (ret)
  {
    std::cout << "Error parse" << std::endl << std::endl;
    return -114514;
  }
  
  std::cout << "Success parse" << std::endl << std::endl;
  // std::string ast_str;
  // ast->to_string(ast_str);
  // std::cout << "AST:" << std::endl << std::endl;
  // std::cout << ast_str << std::endl;

  std::unique_ptr<BaseIR> ir;
  std::shared_ptr<IRINFO> temp_info;
  ir = ast->to_ir(temp_info);

  std::string result;
  ir->to_string(result);
  std::cout << "IR:" << std::endl << std::endl;
  std::cout << result << std::endl;

  if (!strcmp(mode, "-riscv") || !strcmp(mode, "-perf"))
  {
    ir->gather_super();
    ir->alloc_preserve();
    ir->print_super();
    result.clear();
    RISCV riscv;
    Controller cont;
    ir->to_riscv(riscv, cont);
    riscv.to_string(result);
    // std::cout << result << std::endl;
  }

  std::ofstream output_file(output);
  output_file << result;

  return 0;
}
