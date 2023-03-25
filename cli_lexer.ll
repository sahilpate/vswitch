%option noyywrap

%{
enum token {
     ROOT, NL, EXIT, SHOW, MAC, ADDR_TBL, INTF, COUNT, NAME, UINT
};
%}

ws	[ \t]+
alpha	[A-Za-z]
digit	[0-9]
uint	[0-9]+
name	({alpha})({alpha}|{digit})*

%%
{ws}	        /* ignore whitespace */
\n		{return NL;}
exit		{return EXIT;}
show	   	{return SHOW;}
mac		{return MAC;}
address-table	{return ADDR_TBL;}
interface	{return INTF;}
counters	{return COUNT;}
{name}		{return NAME;}
{uint}		{return UINT;}
.		/* ignore anything else */
%%
