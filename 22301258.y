%{

#include "symbol_table.h"

#define YYSTYPE symbol_info*

extern FILE *yyin;
int yyparse(void);
int yylex(void);
extern YYSTYPE yylval;

symbol_table *table;

int lines = 1;

ofstream outlog;

string current_var_type = "";
vector<pair<string, int>> var_list; // (name, array_size) -1 for non-array
string current_func_name = "";
string current_func_return_type = "";
vector<pair<string, string>> current_func_params; // (type, name)

void yyerror(char *s)
{
	outlog << "At line " << lines << " " << s << endl << endl;
	
	// Reinitialize variables
	current_var_type = "";
	var_list.clear();
	current_func_name = "";
	current_func_return_type = "";
	current_func_params.clear();
}

%}

%token IF ELSE FOR WHILE DO BREAK INT CHAR FLOAT DOUBLE VOID RETURN SWITCH CASE DEFAULT CONTINUE PRINTLN ADDOP MULOP INCOP DECOP RELOP ASSIGNOP LOGICOP NOT LPAREN RPAREN LCURL RCURL LTHIRD RTHIRD COMMA SEMICOLON CONST_INT CONST_FLOAT ID

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

start : program
	{
		outlog << "At line no: " << lines << " start : program " << endl << endl;
		outlog << "Symbol Table" << endl << endl;
		
		table->print_all_scopes(outlog);
	}
	;

program : program unit
	{
		outlog << "At line no: " << lines << " program : program unit " << endl << endl;
		outlog << $1->getname() + "\n" + $2->getname() << endl << endl;
		
		$$ = new symbol_info($1->getname() + "\n" + $2->getname(), "program");
	}
	| unit
	{
		outlog << "At line no: " << lines << " program : unit " << endl << endl;
		outlog << $1->getname() << endl << endl;
		
		$$ = new symbol_info($1->getname(), "program");
	}
	;

unit : var_declaration
	 {
		outlog << "At line no: " << lines << " unit : var_declaration " << endl << endl;
		outlog << $1->getname() << endl << endl;
		
		$$ = new symbol_info($1->getname(), "unit");
	 }
     | func_definition
     {
		outlog << "At line no: " << lines << " unit : func_definition " << endl << endl;
		outlog << $1->getname() << endl << endl;
		
		$$ = new symbol_info($1->getname(), "unit");
	 }
     ;

func_definition : type_specifier ID LPAREN parameter_list RPAREN 
		{
			current_func_name = $2->get_name();
			current_func_return_type = $1->get_name();
			
			// Insert function into symbol table
			symbol_info *func = new symbol_info($2->get_name(), "ID");
			func->set_symbol_type("function");
			func->set_return_type(current_func_return_type);
			func->set_parameters(current_func_params);
			
			if (!table->insert(func))
			{
				outlog << "Error at line " << lines << ": Multiple declaration of function " << $2->get_name() << endl << endl;
				delete func;
			}
			
			// Enter new scope for function body
			table->enter_scope();
			outlog << "New ScopeTable # " << table->get_current_scope()->get_unique_id() << " created" << endl << endl;
			
			// Insert parameters into function scope
			for (auto param : current_func_params)
			{
				if (!param.second.empty())
				{
					symbol_info *p = new symbol_info(param.second, "ID");
					p->set_symbol_type("variable");
					p->set_data_type(param.first);
					table->insert(p);
				}
			}
		}
		compound_statement
		{	
			outlog << "At line no: " << lines << " func_definition : type_specifier ID LPAREN parameter_list RPAREN compound_statement " << endl << endl;
			outlog << $1->getname() << " " << $2->getname() << "(" + $4->getname() + ")\n" << $7->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + " " + $2->getname() + "(" + $4->getname() + ")\n" + $7->getname(), "func_def");	
			
			// Print and exit scope
			table->print_all_scopes(outlog);
			outlog << "ScopeTable # " << table->get_current_scope()->get_unique_id() << " removed" << endl << endl;
			table->exit_scope();
			
			current_func_params.clear();
			current_func_name = "";
			current_func_return_type = "";
		}
		| type_specifier ID LPAREN RPAREN 
		{
			current_func_name = $2->get_name();
			current_func_return_type = $1->get_name();
			
			// Insert function into symbol table
			symbol_info *func = new symbol_info($2->get_name(), "ID");
			func->set_symbol_type("function");
			func->set_return_type(current_func_return_type);
			
			if (!table->insert(func))
			{
				outlog << "Error at line " << lines << ": Multiple declaration of function " << $2->get_name() << endl << endl;
				delete func;
			}
			
			// Enter new scope for function body
			table->enter_scope();
			outlog << "New ScopeTable # " << table->get_current_scope()->get_unique_id() << " created" << endl << endl;
		}
		compound_statement
		{
			outlog << "At line no: " << lines << " func_definition : type_specifier ID LPAREN RPAREN compound_statement " << endl << endl;
			outlog << $1->getname() << " " << $2->getname() << "()\n" << $6->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + " " + $2->getname() + "()\n" + $6->getname(), "func_def");	
			
			// Print and exit scope
			table->print_all_scopes(outlog);
			outlog << "ScopeTable # " << table->get_current_scope()->get_unique_id() << " removed" << endl << endl;
			table->exit_scope();
			
			current_func_name = "";
			current_func_return_type = "";
		}
 		;

parameter_list : parameter_list COMMA type_specifier ID
		{
			outlog << "At line no: " << lines << " parameter_list : parameter_list COMMA type_specifier ID " << endl << endl;
			outlog << $1->getname() << "," << $3->getname() << " " << $4->getname() << endl << endl;
					
			$$ = new symbol_info($1->getname() + "," + $3->getname() + " " + $4->getname(), "param_list");
			
			current_func_params.push_back(make_pair($3->get_name(), $4->get_name()));
		}
		| parameter_list COMMA type_specifier
		{
			outlog << "At line no: " << lines << " parameter_list : parameter_list COMMA type_specifier " << endl << endl;
			outlog << $1->getname() << "," << $3->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + "," + $3->getname(), "param_list");
			
			current_func_params.push_back(make_pair($3->get_name(), ""));
		}
 		| type_specifier ID
 		{
			outlog << "At line no: " << lines << " parameter_list : type_specifier ID " << endl << endl;
			outlog << $1->getname() << " " << $2->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + " " + $2->getname(), "param_list");
			
			current_func_params.push_back(make_pair($1->get_name(), $2->get_name()));
		}
		| type_specifier
		{
			outlog << "At line no: " << lines << " parameter_list : type_specifier " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "param_list");
			
			current_func_params.push_back(make_pair($1->get_name(), ""));
		}
 		;

compound_statement : LCURL 
			{
				// Enter new scope only if not already in function scope
				if (current_func_name.empty())
				{
					table->enter_scope();
					outlog << "New ScopeTable # " << table->get_current_scope()->get_unique_id() << " created" << endl << endl;
				}
				else
				{
					current_func_name = ""; // Reset for nested scopes
				}
			}
			statements RCURL
			{ 
 		    	outlog << "At line no: " << lines << " compound_statement : LCURL statements RCURL " << endl << endl;
				outlog << "{\n" + $3->getname() + "\n}" << endl << endl;
				
				$$ = new symbol_info("{\n" + $3->getname() + "\n}", "comp_stmnt");
				
				table->print_all_scopes(outlog);
				outlog << "ScopeTable # " << table->get_current_scope()->get_unique_id() << " removed" << endl << endl;
				table->exit_scope();
 		    }
 		    | LCURL 
 		    {
				// Enter new scope only if not already in function scope
				if (current_func_name.empty())
				{
					table->enter_scope();
					outlog << "New ScopeTable # " << table->get_current_scope()->get_unique_id() << " created" << endl << endl;
				}
				else
				{
					current_func_name = ""; // Reset
				}
			}
			RCURL
 		    { 
 		    	outlog << "At line no: " << lines << " compound_statement : LCURL RCURL " << endl << endl;
				outlog << "{\n}" << endl << endl;
				
				$$ = new symbol_info("{\n}", "comp_stmnt");
				
				table->print_all_scopes(outlog);
				outlog << "ScopeTable # " << table->get_current_scope()->get_unique_id() << " removed" << endl << endl;
				table->exit_scope();
 		    }
 		    ;
 		    
var_declaration : type_specifier 
		{
			current_var_type = $1->get_name();
		}
		declaration_list SEMICOLON
		 {
			outlog << "At line no: " << lines << " var_declaration : type_specifier declaration_list SEMICOLON " << endl << endl;
			outlog << $1->getname() << " " << $3->getname() << ";" << endl << endl;
			
			$$ = new symbol_info($1->getname() + " " + $3->getname() + ";", "var_dec");
			
			// Insert variables into symbol table
			for (auto var : var_list)
			{
				symbol_info *s = new symbol_info(var.first, "ID");
				if (var.second == -1) // Normal variable
				{
					s->set_symbol_type("variable");
					s->set_data_type(current_var_type);
				}
				else // Array
				{
					s->set_symbol_type("array");
					s->set_data_type(current_var_type);
					s->set_array_size(var.second);
				}
				
				if (!table->insert(s))
				{
					outlog << "Error at line " << lines << ": Multiple declaration of " << var.first << endl << endl;
					delete s;
				}
			}
			
			var_list.clear();
			current_var_type = "";
		 }
 		 ;

type_specifier : INT
		{
			outlog << "At line no: " << lines << " type_specifier : INT " << endl << endl;
			outlog << "int" << endl << endl;
			
			$$ = new symbol_info("int", "type");
	    }
 		| FLOAT
 		{
			outlog << "At line no: " << lines << " type_specifier : FLOAT " << endl << endl;
			outlog << "float" << endl << endl;
			
			$$ = new symbol_info("float", "type");
	    }
 		| VOID
 		{
			outlog << "At line no: " << lines << " type_specifier : VOID " << endl << endl;
			outlog << "void" << endl << endl;
			
			$$ = new symbol_info("void", "type");
	    }
 		;

declaration_list : declaration_list COMMA ID
		  {
 		  	outlog << "At line no: " << lines << " declaration_list : declaration_list COMMA ID " << endl << endl;
 		  	outlog << $1->getname() + "," << $3->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + "," + $3->getname(), "dec_list");
			
			var_list.push_back(make_pair($3->get_name(), -1));
 		  }
 		  | declaration_list COMMA ID LTHIRD CONST_INT RTHIRD
 		  {
 		  	outlog << "At line no: " << lines << " declaration_list : declaration_list COMMA ID LTHIRD CONST_INT RTHIRD " << endl << endl;
 		  	outlog << $1->getname() + "," << $3->getname() << "[" << $5->getname() << "]" << endl << endl;
			
			$$ = new symbol_info($1->getname() + "," + $3->getname() + "[" + $5->getname() + "]", "dec_list");
			
			var_list.push_back(make_pair($3->get_name(), stoi($5->get_name())));
 		  }
 		  |ID
 		  {
 		  	outlog << "At line no: " << lines << " declaration_list : ID " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "dec_list");
			
			var_list.push_back(make_pair($1->get_name(), -1));
 		  }
 		  | ID LTHIRD CONST_INT RTHIRD
 		  {
 		  	outlog << "At line no: " << lines << " declaration_list : ID LTHIRD CONST_INT RTHIRD " << endl << endl;
			outlog << $1->getname() << "[" << $3->getname() << "]" << endl << endl;
			
			$$ = new symbol_info($1->getname() + "[" + $3->getname() + "]", "dec_list");
			
			var_list.push_back(make_pair($1->get_name(), stoi($3->get_name())));
 		  }
 		  ;
 		  

statements : statement
	   {
	    	outlog << "At line no: " << lines << " statements : statement " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "stmnts");
	   }
	   | statements statement
	   {
	    	outlog << "At line no: " << lines << " statements : statements statement " << endl << endl;
			outlog << $1->getname() << "\n" << $2->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + "\n" + $2->getname(), "stmnts");
	   }
	   ;
	   
statement : var_declaration
	  {
	    	outlog << "At line no: " << lines << " statement : var_declaration " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "stmnt");
	  }
	  | expression_statement
	  {
	    	outlog << "At line no: " << lines << " statement : expression_statement " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "stmnt");
	  }
	  | compound_statement
	  {
	    	outlog << "At line no: " << lines << " statement : compound_statement " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "stmnt");
	  }
	  | FOR LPAREN expression_statement expression_statement expression RPAREN statement
	  {
	    	outlog << "At line no: " << lines << " statement : FOR LPAREN expression_statement expression_statement expression RPAREN statement " << endl << endl;
			outlog << "for(" << $3->getname() << $4->getname() << $5->getname() << ")\n" << $7->getname() << endl << endl;
			
			$$ = new symbol_info("for(" + $3->getname() + $4->getname() + $5->getname() + ")\n" + $7->getname(), "stmnt");
	  }
	  | IF LPAREN expression RPAREN statement %prec LOWER_THAN_ELSE
	  {
	    	outlog << "At line no: " << lines << " statement : IF LPAREN expression RPAREN statement " << endl << endl;
			outlog << "if(" << $3->getname() << ")\n" << $5->getname() << endl << endl;
			
			$$ = new symbol_info("if(" + $3->getname() + ")\n" + $5->getname(), "stmnt");
	  }
	  | IF LPAREN expression RPAREN statement ELSE statement
	  {
	    	outlog << "At line no: " << lines << " statement : IF LPAREN expression RPAREN statement ELSE statement " << endl << endl;
			outlog << "if(" << $3->getname() << ")\n" << $5->getname() << "\nelse\n" << $7->getname() << endl << endl;
			
			$$ = new symbol_info("if(" + $3->getname() + ")\n" + $5->getname() + "\nelse\n" + $7->getname(), "stmnt");
	  }
	  | WHILE LPAREN expression RPAREN statement
	  {
	    	outlog << "At line no: " << lines << " statement : WHILE LPAREN expression RPAREN statement " << endl << endl;
			outlog << "while(" << $3->getname() << ")\n" << $5->getname() << endl << endl;
			
			$$ = new symbol_info("while(" + $3->getname() + ")\n" + $5->getname(), "stmnt");
	  }
	  | PRINTLN LPAREN ID RPAREN SEMICOLON
	  {
	    	outlog << "At line no: " << lines << " statement : PRINTLN LPAREN ID RPAREN SEMICOLON " << endl << endl;
			outlog << "printf(" << $3->getname() << ");" << endl << endl; 
			
			$$ = new symbol_info("printf(" + $3->getname() + ");", "stmnt");
	  }
	  | RETURN expression SEMICOLON
	  {
	    	outlog << "At line no: " << lines << " statement : RETURN expression SEMICOLON " << endl << endl;
			outlog << "return " << $2->getname() << ";" << endl << endl;
			
			$$ = new symbol_info("return " + $2->getname() + ";", "stmnt");
	  }
	  ;
	  
expression_statement : SEMICOLON
			{
				outlog << "At line no: " << lines << " expression_statement : SEMICOLON " << endl << endl;
				outlog << ";" << endl << endl;
				
				$$ = new symbol_info(";", "expr_stmt");
	        }			
			| expression SEMICOLON 
			{
				outlog << "At line no: " << lines << " expression_statement : expression SEMICOLON " << endl << endl;
				outlog << $1->getname() << ";" << endl << endl;
				
				$$ = new symbol_info($1->getname() + ";", "expr_stmt");
	        }
			;
	  
variable : ID 	
      {
	    outlog << "At line no: " << lines << " variable : ID " << endl << endl;
		outlog << $1->getname() << endl << endl;
			
		$$ = new symbol_info($1->getname(), "varbl");
	 }	
	 | ID LTHIRD expression RTHIRD 
	 {
	 	outlog << "At line no: " << lines << " variable : ID LTHIRD expression RTHIRD " << endl << endl;
		outlog << $1->getname() << "[" << $3->getname() << "]" << endl << endl;
		
		$$ = new symbol_info($1->getname() + "[" + $3->getname() + "]", "varbl");
	 }
	 ;
	 
expression : logic_expression
	   {
	    	outlog << "At line no: " << lines << " expression : logic_expression " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "expr");
	   }
	   | variable ASSIGNOP logic_expression 	
	   {
	    	outlog << "At line no: " << lines << " expression : variable ASSIGNOP logic_expression " << endl << endl;
			outlog << $1->getname() << "=" << $3->getname() << endl << endl;

			$$ = new symbol_info($1->getname() + "=" + $3->getname(), "expr");
	   }
	   ;
			
logic_expression : rel_expression
	     {
	    	outlog << "At line no: " << lines << " logic_expression : rel_expression " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "lgc_expr");
	     }	
		 | rel_expression LOGICOP rel_expression 
		 {
	    	outlog << "At line no: " << lines << " logic_expression : rel_expression LOGICOP rel_expression " << endl << endl;
			outlog << $1->getname() << $2->getname() << $3->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + $2->getname() + $3->getname(), "lgc_expr");
	     }	
		 ;
			
rel_expression	: simple_expression
		{
	    	outlog << "At line no: " << lines << " rel_expression : simple_expression " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "rel_expr");
	    }
		| simple_expression RELOP simple_expression
		{
	    	outlog << "At line no: " << lines << " rel_expression : simple_expression RELOP simple_expression " << endl << endl;
			outlog << $1->getname() << $2->getname() << $3->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + $2->getname() + $3->getname(), "rel_expr");
	    }
		;
				
simple_expression : term
          {
	    	outlog << "At line no: " << lines << " simple_expression : term " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "simp_expr");
	      }
		  | simple_expression ADDOP term 
		  {
	    	outlog << "At line no: " << lines << " simple_expression : simple_expression ADDOP term " << endl << endl;
			outlog << $1->getname() << $2->getname() << $3->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + $2->getname() + $3->getname(), "simp_expr");
	      }
		  ;
					
term :	unary_expression
     {
	    	outlog << "At line no: " << lines << " term : unary_expression " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "term");
	 }
     |  term MULOP unary_expression
     {
	    	outlog << "At line no: " << lines << " term : term MULOP unary_expression " << endl << endl;
			outlog << $1->getname() << $2->getname() << $3->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + $2->getname() + $3->getname(), "term");
	 }
     ;

unary_expression : ADDOP unary_expression
		 {
	    	outlog << "At line no: " << lines << " unary_expression : ADDOP unary_expression " << endl << endl;
			outlog << $1->getname() << $2->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname() + $2->getname(), "un_expr");
	     }
		 | NOT unary_expression 
		 {
	    	outlog << "At line no: " << lines << " unary_expression : NOT unary_expression " << endl << endl;
			outlog << "!" << $2->getname() << endl << endl;
			
			$$ = new symbol_info("!" + $2->getname(), "un_expr");
	     }
		 | factor 
		 {
	    	outlog << "At line no: " << lines << " unary_expression : factor " << endl << endl;
			outlog << $1->getname() << endl << endl;
			
			$$ = new symbol_info($1->getname(), "un_expr");
	     }
		 ;
	
factor	: variable
    {
	    outlog << "At line no: " << lines << " factor : variable " << endl << endl;
		outlog << $1->getname() << endl << endl;
			
		$$ = new symbol_info($1->getname(), "fctr");
	}
	| ID LPAREN argument_list RPAREN
	{
	    outlog << "At line no: " << lines << " factor : ID LPAREN argument_list RPAREN " << endl << endl;
		outlog << $1->getname() << "(" << $3->getname() << ")" << endl << endl;

		$$ = new symbol_info($1->getname() + "(" + $3->getname() + ")", "fctr");
	}
	| LPAREN expression RPAREN
	{
	   	outlog << "At line no: " << lines << " factor : LPAREN expression RPAREN " << endl << endl;
		outlog << "(" << $2->getname() << ")" << endl << endl;
		
		$$ = new symbol_info("(" + $2->getname() + ")", "fctr");
	}
	| CONST_INT 
	{
	    outlog << "At line no: " << lines << " factor : CONST_INT " << endl << endl;
		outlog << $1->getname() << endl << endl;
			
		$$ = new symbol_info($1->getname(), "fctr");
	}
	| CONST_FLOAT
	{
	    outlog << "At line no: " << lines << " factor : CONST_FLOAT " << endl << endl;
		outlog << $1->getname() << endl << endl;
			
		$$ = new symbol_info($1->getname(), "fctr");
	}
	| variable INCOP 
	{
	    outlog << "At line no: " << lines << " factor : variable INCOP " << endl << endl;
		outlog << $1->getname() << "++" << endl << endl;
			
		$$ = new symbol_info($1->getname() + "++", "fctr");
	}
	| variable DECOP
	{
	    outlog << "At line no: " << lines << " factor : variable DECOP " << endl << endl;
		outlog << $1->getname() << "--" << endl << endl;
			
		$$ = new symbol_info($1->getname() + "--", "fctr");
	}
	;
	
argument_list : arguments
			  {
					outlog << "At line no: " << lines << " argument_list : arguments " << endl << endl;
					outlog << $1->getname() << endl << endl;
						
					$$ = new symbol_info($1->getname(), "arg_list");
			  }
			  |
			  {
					outlog << "At line no: " << lines << " argument_list :  " << endl << endl;
					outlog << "" << endl << endl;
						
					$$ = new symbol_info("", "arg_list");
			  }
			  ;
	
arguments : arguments COMMA logic_expression
		  {
				outlog << "At line no: " << lines << " arguments : arguments COMMA logic_expression " << endl << endl;
				outlog << $1->getname() << "," << $3->getname() << endl << endl;
						
				$$ = new symbol_info($1->getname() + "," + $3->getname(), "arg");
		  }
	      | logic_expression
	      {
				outlog << "At line no: " << lines << " arguments : logic_expression " << endl << endl;
				outlog << $1->getname() << endl << endl;
						
				$$ = new symbol_info($1->getname(), "arg");
		  }
	      ;
 

%%

int main(int argc, char *argv[])
{
	if(argc != 2) 
	{
		cout << "input1.c" << endl;
		return 0;
	}
	yyin = fopen(argv[1], "r");
	outlog.open("output.txt", ios::trunc);
	
	if(yyin == NULL)
	{
		cout << "Couldn't open file" << endl;
		return 0;
	}
	
	// Create symbol table with bucket size 10
	table = new symbol_table(10);
	outlog << "ScopeTable # " << table->get_current_scope()->get_unique_id() << " created" << endl << endl;

	yyparse();
	
	outlog << endl << "Total lines: " << lines << endl;
	
	delete table;
	
	outlog.close();
	
	fclose(yyin);
	
	return 0;
}