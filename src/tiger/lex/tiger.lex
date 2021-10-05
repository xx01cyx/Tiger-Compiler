%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 letter [A-Za-z]
 digits [0-9]+
 printable \\[0-9]{3}
 control \\\^[A-Z]

%x COMMENT STR IGNORE

%%

 /* operators */
 "," {adjust(); return Parser::COMMA;}
 ":" {adjust(); return Parser::COLON;}
 ";" {adjust(); return Parser::SEMICOLON;}
 "(" {adjust(); return Parser::LPAREN;}
 ")" {adjust(); return Parser::RPAREN;}
 "[" {adjust(); return Parser::LBRACK;}
 "]" {adjust(); return Parser::RBRACK;}
 "{" {adjust(); return Parser::LBRACE;}
 "}" {adjust(); return Parser::RBRACE;}
 "." {adjust(); return Parser::DOT;}
 "+" {adjust(); return Parser::PLUS;}
 "-" {adjust(); return Parser::MINUS;}
 "*" {adjust(); return Parser::TIMES;}
 "/" {adjust(); return Parser::DIVIDE;}
 "=" {adjust(); return Parser::EQ;}
 "<" {adjust(); return Parser::LT;}
 ">" {adjust(); return Parser::GT;}
 "&" {adjust(); return Parser::AND;}
 "|" {adjust(); return Parser::OR;}
 ":=" {adjust(); return Parser::ASSIGN;}
 "<>" {adjust(); return Parser::NEQ;}
 "<=" {adjust(); return Parser::LE;}
 ">=" {adjust(); return Parser::GE;}

 /* reserved words */
 "array" {adjust(); return Parser::ARRAY;}
 "if" {adjust(); return Parser::IF;}
 "then" {adjust(); return Parser::THEN;}
 "else" {adjust(); return Parser::ELSE;}
 "while" {adjust(); return Parser::WHILE;}
 "for" {adjust(); return Parser::FOR;}
 "to" {adjust(); return Parser::TO;}
 "do" {adjust(); return Parser::DO;}
 "let" {adjust(); return Parser::LET;}
 "in" {adjust(); return Parser::IN;}
 "end" {adjust(); return Parser::END;}
 "of" {adjust(); return Parser::OF;}
 "break" {adjust(); return Parser::BREAK;}
 "nil" {adjust(); return Parser::NIL;}
 "function" {adjust(); return Parser::FUNCTION;}
 "var" {adjust(); return Parser::VAR;}
 "type" {adjust(); return Parser::TYPE;}

 /* literals */
 {letter}[A-Za-z0-9_]* {adjust(); return Parser::ID;}
 {digits} {adjust(); return Parser::INT;}

 /* strings */
 \" {adjust(); begin(StartCondition__::STR);}
 <STR>\" {adjustStr(); begin(StartCondition__::INITIAL); return Parser::STRING;}
 <STR>\\n|\\t|\\\"|\\\\|{control}|{printable} {adjustStr();}
 <STR>\\ {adjust(); begin(StartCondition__::IGNORE);}
 <STR><<EOF>> {errormsg_->Error(errormsg_->tok_pos_, "unterminated string");}
 <STR>. {adjustStr();}
 <IGNORE>[\n\t ] {adjust();}
 <IGNORE>\\ {adjust(); begin(StartCondition__::STR);}

 /* comments */
 <INITIAL,COMMENT>"/*" {adjust(); begin(StartCondition__::COMMENT);}
 <COMMENT>"*/" {
    adjust();
    if (comment_level_ == 1)
        begin(StartCondition__::INITIAL);
 }
 <COMMENT>\n {adjust();}
 <COMMENT>. {adjust();}

 /*
  * skip white space chars.
  * space, tabs and LF
  */
 [ \t]+ {adjust();}
 \n {adjust(); errormsg_->Newline();}

 /* illegal input */
 . {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
