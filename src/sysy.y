%code requires {
  #include <memory>
  #include <string>
  #include <optional>
  #include <ast.h>
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <optional>
#include <ast.h>

const bool debug = false;

int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// %define parse.error verbose
%glr-parser

%parse-param { std::unique_ptr<BaseAST> &ast }

%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
}

%token INT RETURN CONST IF ELSE WHILE CONTINUE BREAK VOID EQ
%token <str_val> IDENT 
%token <int_val> INT_CONST

%type <ast_val> FuncFParam Init FuncFParams FuncRParams FuncDef Block Stmt Exp LOrExp LAndExp EqExp RelExp AddExp MulExp UnaryExp PrimaryExp Number Def Defs Decl BlockItem BlockItems SealedIF OpenIF ABracket Inits

%%

CompUnit
  : FuncDef {
    if (debug) std::cout << "CompUnit: FuncDef" << std::endl;
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_def.push_back(unique_ptr<FuncDefAST>(dynamic_cast<FuncDefAST*>($1)));
    ast = move(comp_unit);
  }
  | Decl {
    if (debug) std::cout << "CompUnit: Decl" << std::endl;
    auto comp_unit = make_unique<CompUnitAST>();
    auto stmts = unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($1));
    for (auto &stmt : stmts->stmts) {
      auto def = dynamic_cast<DefAST*>(stmt.release());
      comp_unit->var_def.push_back(std::unique_ptr<DefAST>(def));
    }
    ast = move(comp_unit);
  }
  | CompUnit FuncDef {
    if (debug) std::cout << "CompUnit: CompUnit FuncDef" << std::endl;
    auto temp = dynamic_cast<CompUnitAST*>(ast.get());
    temp->func_def.push_back(unique_ptr<FuncDefAST>(dynamic_cast<FuncDefAST*>($2)));
  }
  | CompUnit Decl {
    if (debug) std::cout << "CompUnit: CompUnit Decl" << std::endl;
    auto temp = dynamic_cast<CompUnitAST*>(ast.get());
    auto stmts = unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($2));
    for (auto &stmt : stmts->stmts) {
      auto def = dynamic_cast<DefAST*>(stmt.release());
      temp->var_def.push_back(std::unique_ptr<DefAST>(def));
    }
  }
  ;

FuncFParam
  : INT IDENT {
    if (debug) std::cout << "FuncFParam: INT IDENT" << std::endl;
    auto ast = new FuncFPAST();
    ast->name = *unique_ptr<std::string>($2);
    $$ = ast;
  }
  | INT IDENT '[' ']' {
    if (debug) std::cout << "FuncFParam: INT IDENT '[' ']'" << std::endl;
    auto ast = new FuncFPAST();
    ast->name = *unique_ptr<std::string>($2);
    auto l = List<std::optional<std::unique_ptr<ExpAST>>>();
    l.push_back(std::nullopt);
    ast->dims.merge(l);
    $$ = ast;
  }
  | INT IDENT '[' ']' ABracket {
    if (debug) std::cout << "FuncFParam: INT IDENT '[' ']' ABracket" << std::endl;
    auto ast = new FuncFPAST();
    auto dims = std::unique_ptr<DimAST>(dynamic_cast<DimAST*>($5));
    dims->dims.push_front(std::nullopt);
    ast->name = *unique_ptr<std::string>($2);
    ast->dims.merge(dims->dims);
    $$ = ast;
  }

FuncFParams
  : FuncFParam {
    if (debug) std::cout << "FuncFParams: FuncFParam" << std::endl;
    auto ast = new FuncFPsAST();
    auto ast2 = dynamic_cast<FuncFPAST*>($1);
    ast->args.push_back(std::unique_ptr<FuncFPAST>(ast2));
    $$ = ast;
  }
  | FuncFParams ',' FuncFParam {
    if (debug) std::cout << "FuncFParams: FuncFParams ',' FuncFParam" << std::endl;
    auto ast = dynamic_cast<FuncFPsAST*>($1);
    auto ast2 = dynamic_cast<FuncFPAST*>($3);
    ast->args.push_back(std::unique_ptr<FuncFPAST>(ast2));
    $$ = ast;
  }

FuncDef
  : INT IDENT '(' ')' Block {
    if (debug) std::cout << "FuncDef: FuncType IDENT '(' ')' Block" << std::endl;
    auto ast = new FuncDefAST();
    ast->func_type = "int";
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BlockAST>(dynamic_cast<BlockAST*>($5));
    $$ = ast;
  }
  | INT IDENT '(' FuncFParams ')' Block {
    if (debug) std::cout << "FuncDef: FuncType IDENT '(' FuncFParams ')' Block" << std::endl;
    auto ast = new FuncDefAST();
    ast->func_type = "int";
    ast->ident = *unique_ptr<string>($2);
    auto args = std::unique_ptr<FuncFPsAST>(dynamic_cast<FuncFPsAST*>($4));
    for (auto &arg : args->args) {
      auto temp = arg.release();
      ast->args.push_back({temp->name, std::move(temp->dims)});
    }
    ast->block = unique_ptr<BlockAST>(dynamic_cast<BlockAST*>($6));
    $$ = ast;
  }
  | VOID IDENT '(' ')' Block {
    if (debug) std::cout << "FuncDef: FuncType IDENT '(' ')' Block" << std::endl;
    auto ast = new FuncDefAST();
    ast->func_type = "void";
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BlockAST>(dynamic_cast<BlockAST*>($5));
    $$ = ast;
  }
  | VOID IDENT '(' FuncFParams ')' Block {
    if (debug) std::cout << "FuncDef: FuncType IDENT '(' FuncFParams ')' Block" << std::endl;
    auto ast = new FuncDefAST();
    ast->func_type = "void";
    ast->ident = *unique_ptr<string>($2);
    auto args = std::unique_ptr<FuncFPsAST>(dynamic_cast<FuncFPsAST*>($4));
    for (auto &arg : args->args) {
      auto temp = arg.release();
      ast->args.push_back({temp->name, std::move(temp->dims)});
    }
    ast->block = unique_ptr<BlockAST>(dynamic_cast<BlockAST*>($6));
    $$ = ast;
  }
  ;

ABracket
  : '[' Exp ']' {
    if (debug) std::cout << "ABracket: '[' Exp ']'" << std::endl;
    auto ast = new DimAST();
    ast->dims.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($2)));
    $$ = ast;
  }
  | ABracket '[' Exp ']' {
    if (debug) std::cout << "ABracket: ABracket '[' Exp ']'" << std::endl;
    auto ast = dynamic_cast<DimAST*>($1);
    ast->dims.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }

Inits
  : Init {
    if (debug) std::cout << "Inits: Init" << std::endl;
    auto ast = new InitAST();
    ast->inits.push_back(unique_ptr<InitAST>(dynamic_cast<InitAST*>($1)));
    $$ = ast;
  }
  | Exp {
    if (debug) std::cout << "Inits: Exp" << std::endl;
    auto ast = new InitAST();
    auto ast2 = new InitAST();
    ast2->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1));
    ast->inits.push_back(unique_ptr<InitAST>(ast2));
    $$ = ast;
  }
  | Inits ',' Init {
    if (debug) std::cout << "Inits: Inits ',' Init" << std::endl;
    auto ast = dynamic_cast<InitAST*>($1);
    ast->inits.push_back(unique_ptr<InitAST>(dynamic_cast<InitAST*>($3)));
    $$ = ast;
  }
  | Inits ',' Exp {
    if (debug) std::cout << "Inits: Inits ',' Exp" << std::endl;
    auto ast = dynamic_cast<InitAST*>($1);
    auto ast2 = new InitAST();
    ast2->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    ast->inits.push_back(unique_ptr<InitAST>(ast2));
    $$ = ast;
  }

Init
  : '{' '}' {
    if (debug) std::cout << "Inits: '{' '}'" << std::endl;
    auto ast = new InitAST();
    $$ = ast;
  }
  | '{' Inits '}' {
    if (debug) std::cout << "Inits: '{' Inits '}'" << std::endl;
    $$ = $2;
  }

Def 
  : IDENT {
    if (debug) std::cout << "Def: IDENT" << std::endl;
    auto ast = new DefAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '=' Exp {
    if (debug) std::cout << "Def: IDENT '=' Exp" << std::endl;
    auto ast = new DefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    $$ = ast;
  }
  | IDENT ABracket {
    if (debug) std::cout << "Def: IDENT ABracket" << std::endl;
    auto ast = new DefAST();
    ast->ident = *unique_ptr<string>($1);
    auto dims = std::unique_ptr<DimAST>(dynamic_cast<DimAST*>($2));
    for (auto &dim : dims->dims) {
      auto temp = dim.value().release();
      ast->dims.push_back(unique_ptr<ExpAST>(temp));
    }
    $$ = ast;
  }
  | IDENT ABracket '=' Init {
    if (debug) std::cout << "Def: IDENT ABracket '=' Init" << std::endl;
    auto ast = new DefAST();
    ast->ident = *unique_ptr<string>($1);
    auto dims = std::unique_ptr<DimAST>(dynamic_cast<DimAST*>($2));
    for (auto &dim : dims->dims) {
      auto temp = dim.value().release();
      ast->dims.push_back(unique_ptr<ExpAST>(temp));
    }
    ast->init = unique_ptr<InitAST>(dynamic_cast<InitAST*>($4));
    $$ = ast;
  }

Defs
  : Def {
    if (debug) std::cout << "Defs: Def" << std::endl;
    auto ast = new StmtsAST();
    ast->stmts.push_back(unique_ptr<StmtAST>(dynamic_cast<StmtAST*>($1)));
    $$ = ast;
  }
  | Defs ',' Def {
    if (debug) std::cout << "Defs: Defs ',' Def" << std::endl;
    auto ast = dynamic_cast<StmtsAST*>($1);
    ast->stmts.push_back(unique_ptr<StmtAST>(dynamic_cast<StmtAST*>($3)));
    $$ = ast;
  }

Decl
  : CONST INT Defs ';' {
    if (debug) std::cout << "Decl: CONST INT Defs ';'" << std::endl;
    auto ast = dynamic_cast<StmtsAST*>($3);
    for (auto &stmt : ast->stmts) {
      auto def = dynamic_cast<DefAST*>(stmt.get());
      def->is_const = true;
    }
    $$ = dynamic_cast<BaseAST*>(ast);
  }
  | INT Defs ';' {
    if (debug) std::cout << "Decl: INT Defs ';'" << std::endl;
    $$ = $2;
  }

BlockItem
  : Decl {
    if (debug) std::cout << "BlockItem: Decl" << std::endl;
    $$ = $1;
  }
  | Stmt {
    if (debug) std::cout << "BlockItem: Stmt" << std::endl;
    $$ = $1;
  }

BlockItems
  : BlockItem {
    if (debug) std::cout << "BlockItems: BlockItem" << std::endl;
    $$ = $1;
  }
  | BlockItems BlockItem {
    if (debug) std::cout << "BlockItems: BlockItem BlockItems" << std::endl;
    auto ast = dynamic_cast<StmtsAST*>($1);
    auto ast2 = std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($2));
    ast->merge(ast2);
    $$ = ast;
  }

Block
  : '{' BlockItems '}' {
    if (debug) std::cout << "Block: '{' Stmt '}'" << std::endl;
    auto ast = new BlockAST();
    ast->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($2))->stmts);
    $$ = ast;
  }
  | '{' '}' {
    if (debug) std::cout << "Block: '{' '}'" << std::endl;
    auto ast = new BlockAST();
    $$ = ast;
  }
  ;

SealedIF
  : RETURN Exp ';' {
    if (debug) std::cout << "Stmt: RETURN Exp ';'" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new ReturnAST();
    ast2->type = "ret";
    ast2->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($2));
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | RETURN ';' {
    if (debug) std::cout << "Stmt: RETURN ';'" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new ReturnAST();
    ast2->type = "ret";
    ast2->exp = std::nullopt;
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | IDENT '=' Exp ';' {
    if (debug) std::cout << "Stmt: IDENT '=' Exp ';'" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new AssignAST();
    ast2->type = "assign";
    ast2->ident = *unique_ptr<string>($1);
    ast2->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | IDENT ABracket '=' Exp ';' {
    if (debug) std::cout << "Stmt: IDENT ABracket '=' Exp ';'" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new AssignAST();
    ast2->type = "assign";
    ast2->ident = *unique_ptr<string>($1);
    auto dim = std::unique_ptr<DimAST>(dynamic_cast<DimAST*>($2));
    for (auto &d : dim->dims) {
      auto temp = d.value().release();
      ast2->dims.push_back(unique_ptr<ExpAST>(temp));
    }
    ast2->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($4));
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | ';' {
    if (debug) std::cout << "Stmt: ';'" << std::endl;
    auto ast = new StmtsAST();
    $$ = ast;
  }
  | Exp ';' {
    if (debug) std::cout << "Stmt: Exp ';'" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new StmtExpAST();
    ast2->exp = std::unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1));
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | Block {
    if (debug) std::cout << "Stmt: Block" << std::endl;
    auto ast = new StmtsAST();
    ast->stmts.push_back(unique_ptr<BlockAST>(dynamic_cast<BlockAST*>($1)));
    $$ = ast;
  }
  | IF '(' Exp ')' SealedIF ELSE SealedIF {
    if (debug) std::cout << "Stmt: IF '(' Exp ')' SealedIF ELSE SealedIF" << std::endl;
    auto ast = new StmtsAST();
    auto ast1 = new IfAST();
    ast1->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    auto ast2 = new BlockAST();
    ast2->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($5))->stmts);
    ast1->then_stmt = std::unique_ptr<BlockAST>(ast2);
    auto ast3 = new BlockAST();
    ast3->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($7))->stmts);
    ast1->else_stmt = std::unique_ptr<BlockAST>(ast3);
    ast->stmts.push_back(unique_ptr<StmtAST>(ast1));
    $$ = ast;
  }
  | BREAK ';' {
    if (debug) std::cout << "Stmt: BREAK ';'" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new ControlAST();
    ast2->type = "break";
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | CONTINUE ';' {
    if (debug) std::cout << "Stmt: CONTINUE ';'" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new ControlAST();
    ast2->type = "continue";
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | WHILE '(' Exp ')' SealedIF {
    if (debug) std::cout << "Stmt: WHILE '(' Exp ')' Stmt" << std::endl;
    auto ast = new StmtsAST();
    auto ast1 = new WhileAST();
    ast1->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    auto ast2 = new BlockAST();
    ast2->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($5))->stmts);
    ast1->stmt = std::unique_ptr<BlockAST>(ast2);
    ast->stmts.push_back(unique_ptr<StmtAST>(ast1));
    $$ = ast;
  }
  ;

OpenIF
  : IF '(' Exp ')' Stmt {
    if (debug) std::cout << "Stmt: IF '(' Exp ')' Stmt" << std::endl;
    auto ast = new StmtsAST();
    auto ast1 = new IfAST();
    ast1->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    auto ast2 = new BlockAST();
    ast2->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($5))->stmts);
    ast1->then_stmt = std::unique_ptr<BlockAST>(ast2);
    ast->stmts.push_back(unique_ptr<StmtAST>(ast1));
    $$ = ast;
  }
  | WHILE '(' Exp ')' OpenIF {
    if (debug) std::cout << "Stmt: WHILE '(' Exp ')' Stmt" << std::endl;
    auto ast = new StmtsAST();
    auto ast1 = new WhileAST();
    ast1->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    auto ast2 = new BlockAST();
    ast2->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($5))->stmts);
    ast1->stmt = std::unique_ptr<BlockAST>(ast2);
    ast->stmts.push_back(unique_ptr<StmtAST>(ast1));
    $$ = ast;
  }
  | IF '(' Exp ')' SealedIF ELSE OpenIF {
    if (debug) std::cout << "Stmt: IF '(' Exp ')' SealedIF ELSE OpenIF" << std::endl;
    auto ast = new StmtsAST();
    auto ast1 = new IfAST();
    ast1->exp = unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    auto ast2 = new BlockAST();
    ast2->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($5))->stmts);
    ast1->then_stmt = std::unique_ptr<BlockAST>(ast2);
    auto ast3 = new BlockAST();
    ast3->stmts.merge(std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($7))->stmts);
    ast1->else_stmt = std::unique_ptr<BlockAST>(ast3);
    ast->stmts.push_back(unique_ptr<StmtAST>(ast1));
    $$ = ast;
  }

Stmt
  : SealedIF {
    if (debug) std::cout << "Stmt: SealedIF" << std::endl;
    $$ = $1;
  }
  | OpenIF {
    if (debug) std::cout << "Stmt: OpenIF" << std::endl;
    $$ = $1;
  }

Exp
  : LOrExp {
    if (debug) std::cout << "Exp: LOrExp" << std::endl;
    $$ = $1;
  }

LOrExp
  : LAndExp {
    if (debug) std::cout << "LOrExp: LAndExp" << std::endl;
    $$ = $1;
  }
  | LOrExp '|' '|' LAndExp {
    if (debug) std::cout << "LOrExp: LOrExp '||' LAndExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "or";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($4)));
    $$ = ast;
  }

LAndExp
  : EqExp {
    if (debug) std::cout << "LAndExp: EqExp" << std::endl;
    $$ = $1;
  }
  | LAndExp '&' '&' EqExp {
    if (debug) std::cout << "LAndExp: LAndExp '&&' EqExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "and";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($4)));
    $$ = ast;
  }

EqExp 
  : RelExp {
    if (debug) std::cout << "EqExp: RelExp" << std::endl;
    $$ = $1;
  }
  | EqExp EQ RelExp {
    if (debug) std::cout << "EqExp: EqExp '==' RelExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "eq";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }
  | EqExp '!' '=' RelExp {
    if (debug) std::cout << "EqExp: EqExp '!=' RelExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "ne";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($4)));
    $$ = ast;
  }

RelExp
  : AddExp {
    if (debug) std::cout << "RelExp: AddExp" << std::endl;
    $$ = $1;
  }
  | RelExp '<' AddExp {
    if (debug) std::cout << "RelExp: RelExp '<' AddExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "lt";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }
  | RelExp '>' AddExp {
    if (debug) std::cout << "RelExp: RelExp '>' AddExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "gt";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }
  | RelExp '<' '=' AddExp {
    if (debug) std::cout << "RelExp: RelExp '<=' AddExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "le";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($4)));
    $$ = ast;
  }
  | RelExp '>' '=' AddExp {
    if (debug) std::cout << "RelExp: RelExp '>=' AddExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "ge";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($4)));
    $$ = ast;
  }

AddExp
  : MulExp {
    if (debug) std::cout << "AddExp: MulExp" << std::endl;
    $$ = $1;
  }
  | AddExp '+' MulExp {
    if (debug) std::cout << "AddExp: AddExp '+' MulExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "add";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }
  | AddExp '-' MulExp {
    if (debug) std::cout << "AddExp: AddExp '-' MulExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "sub";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }

MulExp
  : UnaryExp {
    if (debug) std::cout << "MulExp: UnaryExp" << std::endl;
    $$ = $1;
  }
  | MulExp '*' UnaryExp {
    if (debug) std::cout << "MulExp: MulExp '*' UnaryExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "mul";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }
  | MulExp '/' UnaryExp {
    if (debug) std::cout << "MulExp: MulExp '/' UnaryExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "div";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }
  | MulExp '%' UnaryExp {
    if (debug) std::cout << "MulExp: MulExp '\%' UnaryExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "mod";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1)));
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3)));
    $$ = ast;
  }

UnaryExp
  : PrimaryExp {
    if (debug) std::cout << "UnaryExp: PrimaryExp" << std::endl;
    $$ = $1;
  }
  | '-' UnaryExp {
    if (debug) std::cout << "UnaryExp: '-' UnaryExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "sub";
    ast->args.push_back(std::make_unique<ExpAST>());
    dynamic_cast<ExpAST*>(ast->args.begin()->get())->value = "0";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($2)));
    $$ = ast;
  }
  | '!' UnaryExp {
    if (debug) std::cout << "UnaryExp: '!' UnaryExp" << std::endl;
    auto ast = new ExpAST();
    ast->op = "eq";
    ast->args.push_back(std::make_unique<ExpAST>());
    dynamic_cast<ExpAST*>(ast->args.begin()->get())->value = "0";
    ast->args.push_back(unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($2)));
    $$ = ast;
  }
  | '+' UnaryExp {
    if (debug) std::cout << "UnaryExp: '+' UnaryExp" << std::endl;
    $$ = $2;
  }
  | IDENT '(' ')' {
    if (debug) std::cout << "UnaryExp: IDENT '(' ')'" << std::endl;
    auto ast = new ExpAST();
    ast->op = "func_" + *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '(' FuncRParams ')' {
    if (debug) std::cout << "UnaryExp: IDENT '(' FuncRParams ')'" << std::endl;
    auto ast = new ExpAST();
    ast->op = "func_" + *unique_ptr<string>($1);
    auto args = std::unique_ptr<StmtsAST>(dynamic_cast<StmtsAST*>($3));
    for (auto &stmt : args->stmts) {
      auto sexp = dynamic_cast<StmtExpAST*>(stmt.get());
      ast->args.push_back(std::move(sexp->exp));
    }
    $$ = ast;
  }

FuncRParams
  : Exp {
    if (debug) std::cout << "FuncRParams: Exp" << std::endl;
    auto ast = new StmtsAST();
    auto ast2 = new StmtExpAST();
    ast2->exp = std::unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($1));
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }
  | FuncRParams ',' Exp {
    if (debug) std::cout << "FuncRParams: FuncRParams ',' Exp" << std::endl;
    auto ast = dynamic_cast<StmtsAST*>($1);
    auto ast2 = new StmtExpAST();
    ast2->exp = std::unique_ptr<ExpAST>(dynamic_cast<ExpAST*>($3));
    ast->stmts.push_back(unique_ptr<StmtAST>(ast2));
    $$ = ast;
  }

PrimaryExp
  : '(' Exp ')' {
    if (debug) std::cout << "PrimaryExp: '(' Exp ')'" << std::endl;
    $$ = $2;
  }
  | Number {
    if (debug) std::cout << "PrimaryExp: Number" << std::endl;
    $$ = $1;
  }
  | IDENT {
    if (debug) std::cout << "PrimaryExp: IDENT" << std::endl;
    auto ast = new ExpAST();
    ast->value = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT ABracket {
    if (debug) std::cout << "PrimaryExp: IDENT ABracket" << std::endl;
    auto ast = new ExpAST();
    ast->op = "at";
    ast->arr_name = *unique_ptr<string>($1);
    auto dim = std::unique_ptr<DimAST>(dynamic_cast<DimAST*>($2));
    for (auto &d : dim->dims) {
      auto temp = d.value().release();
      ast->args.push_back(unique_ptr<ExpAST>(temp));
    }
    $$ = ast;
  }

Number
  : INT_CONST {
    if (debug) std::cout << "Number: INT_CONST" << std::endl;
    auto ast = new ExpAST();
    ast->value = std::to_string($1);
    $$ = ast;
  }
  ;

%%

void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
    std::string current;
    if (ast)
      ast->to_string(current);
    cerr << "error: " << s << std::endl << current << std::endl;
}