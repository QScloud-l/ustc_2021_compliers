%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "syntax_analyzer.h"
/*enum{
     ELSE, IF, INT, RETURN, VOID, WHILE, FLOAT,\
     ADD, SUB, MUL, DIV, LT, LET, GT, GET, EQ, NE, ASSIGN, SEMI,\
     COMMA, LL, LR, ML, MR, BL, BR,\
     IDENT, NUMBER, BLANK, ERROR, ENTER
};*/
int lines;
int pos_start;
int pos_end;
void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}
void locationwork(){
     pos_start = pos_end;
     for(int i = 0; yytext[i] != '\0'; i++){
	     if(yytext[i] == '\n') {
	          lines++;
	          pos_end = 0;
	          }
	     else {
	          pos_end++;
	     }
    }
}

/*****************声明和选项设置  end*****************/

%}

LETTER [a-zA-Z]
DIGIT [0-9]
ID {LETTER}+
INTEGER {DIGIT}+
FLOATPOINT ({DIGIT}+"."|{DIGIT}*"."{DIGIT}+)
COMMENTS "/*"(\*|[^*/](\/)*)*"*/"|"/*/"
%%
 /* to do for students */
 /* two cases for you, pass_node will send flex's token to bison */
 /* \+ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ADD;} */
{COMMENTS}     {locationwork();}
\n        {locationwork();}
else      {locationwork(); pass_node(yytext); return ELSE;}
if        {locationwork(); pass_node(yytext); return IF;}
int       {locationwork(); pass_node(yytext); return INT;}
return    {locationwork(); pass_node(yytext); return RETURN;}
void      {locationwork(); pass_node(yytext); return VOID;}
while     {locationwork(); pass_node(yytext); return WHILE;}
float     {locationwork(); pass_node(yytext); return FLOAT;}
\+   {locationwork(); pass_node(yytext); return ADD;}
\-   {locationwork(); pass_node(yytext); return SUB;}
\*   {locationwork(); pass_node(yytext); return MUL;}
\/   {locationwork(); pass_node(yytext); return DIV;}
\>        {locationwork(); pass_node(yytext); return GT;}
\>=       {locationwork(); pass_node(yytext); return GET;}
\<        {locationwork(); pass_node(yytext); return LT;}
\<=       {locationwork(); pass_node(yytext); return LET;}
==        {locationwork(); pass_node(yytext); return EQ;}
!=        {locationwork(); pass_node(yytext); return NE;}
=         {locationwork(); pass_node(yytext); return ASSIGN;}
;         {locationwork(); pass_node(yytext); return SEMI;}
,         {locationwork(); pass_node(yytext); return COMMA;}
\(        {locationwork(); pass_node(yytext); return LL;}
\)        {locationwork(); pass_node(yytext); return LR;}
\[        {locationwork(); pass_node(yytext); return ML;}
\]        {locationwork(); pass_node(yytext); return MR;}
\{        {locationwork(); pass_node(yytext); return BL;}
\}        {locationwork(); pass_node(yytext); return BR;}
{ID}           {locationwork(); pass_node(yytext); return IDENT;}
{FLOATPOINT}   {locationwork(); pass_node(yytext); return FLOATPOINT;}
{INTEGER}      {locationwork(); pass_node(yytext); return INTEGER;}
[ \t]          {locationwork();}
.              {locationwork(); return ERROR;}

 /****请在此补全所有flex的模式与动作  end******/
%%


