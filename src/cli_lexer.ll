/*
 * cli_lexer.ll - Flex file which scans for various special tokens used in the vswitch CLI.
 */

%option noyywrap

%{
enum token {
     ROOT, NL, EXIT, SHOW, MAC, ADDR_TBL, INTF, COUNT, NAME, UINT, VLAN, NO, CLEAR, AGE_TIME
};
%}

ws	[ \t]+
alpha	[A-Za-z]
digit	[0-9]
uint	[0-9]+
name	({alpha})({alpha}|{digit}|-)*

%%
{ws}	        /* ignore whitespace */
\n		{return NL;}
exit		{return EXIT;}
show	   	{return SHOW;}
mac		{return MAC;}
address-table	{return ADDR_TBL;}
interfaces	{return INTF;}
counters	{return COUNT;}
vlan		{return VLAN;}
no		{return NO;}
clear		{return CLEAR;}
aging-time	{return AGE_TIME;}
{name}		{return NAME;}
{uint}		{return UINT;}
.		/* ignore anything else */
%%
