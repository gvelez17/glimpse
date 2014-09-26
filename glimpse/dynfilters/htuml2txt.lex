/********
 * $Id: htuml2txt.lex,v 1.1 2000/05/25 18:07:05 golda Exp $
 * $Log: htuml2txt.lex,v $
 * Revision 1.1  2000/05/25 18:07:05  golda
 * Added Christian's changes to allow dynamic filters.  I believe this has only been tested on Linux
 * systems.  --GV
 *
 * Revision 1.5  1999/11/06 21:25:07  cvogler
 * - Fixed bug that did not recognize the end of a comment correctly.
 *
 * Revision 1.4  1999/11/06 06:55:08  cvogler
 * - Added support for &gt; and &lt; (greather than, and less than).
 * -  Fixed problems with the matching rules for non-spacing tags that
 *    caused linefeeds to be incorrectly suppressed. As a result, jumping
 *    to line numbers from webglimpse searches did not work.
 *
 *
 * htuml2text.lex
 *
 * A faster HTML filter for WebGlimpse than htuml2txt.pl. I found that
 * the spawning of all the perl processes by glimpse was way too expensive
 * to be practical. In particular, searching 2000 files for a frequently
 * occuring term took more than 30 seconds on a PII-400/Linux 2.2.5
 * machine. Rewriting the filter as a set of lex rules reduced the search
 * time to 5 seconds, which is on par with the simple html2txt filter.
 *
 * Suggested options for compiling on i386/Linux with egcs 1.1.2/flex 2.5.4:
 * flex -F -8 htuml2txt.lex
 * gcc -O3 -fomit-frame-pointer -o htuml2txt lex.yy.c -lfl
 *
 * Note:    For a smaller, slightly slower executable, omit the -F switch in
 *          the call to flex.
 *
 * Caution: The -8 switch MUST be specified if -f or -F is specified!
 * 
 * Note:    It is also necessary to edit .glimpse_filters in the
 *          WebGlimpse database directories.
 *
 * Suggested options for compiling with AT&T-style lex:
 * lex htuml2txt.lex
 * cc -O -o htuml2txt lex.yy.c -ll
 * 
 * Written  on 5/16/1999 by Christian Vogler
 * Send bugreports and suggestions to cvogler@gradient.cis.upenn.edu.
 ******/


STRING           \"([^\"\n\\]|\\\")*\"
WHITE            [\ \t]

/* HTML tags that are to be eliminated altogether, without even a */
/* substitution with a space */
A                [aA]
B                [bB]
I                [iI]
EM               [eE][mM]
FONT             [fF][oO][nN][tT]
STRONG           [sS][tT][rR][oO][nN][gG]
BIG              [bB][iI][gG]
SUP              [sS][uU][pP]
SUB              [sS][uU][bB]
U                [uU]
STRIKE           [sS][tT][rR][iI][kK][eE]
STYLE            [sS][tT][yY][lL][eE]
NSPTAGS          ({A}|{B}|{I}|{EM}|{FONT}|{STRONG}|{BIG}|{SUP}|{SUB}|{U}|{STRIKE}|{STYLE})


/* These allocate the necessary space to make AT&T lex work. */
/* flex ignores them. */
%e 4000
%p 10000
%n 2000

/* treat inside of HTML comments and tags specially, to ensure that */
/* everything inside them is eliminated, even if they contain quotes */
%s COMMENT
%s TAG
%s BEGINTAG

%%

<COMMENT>[^\-\"\n\r]+                   {/* This ruleset eats up all */}
<COMMENT>-+[^\-\>\"\n\r]+               {/* HTML comments */}
<COMMENT>-\>                            {/* none */}
<COMMENT>{STRING}                       {/* none */}
<COMMENT>-{2,}\>                        BEGIN(INITIAL);

<TAG>[^\"\>\r\n]+                       {/* This ruleset discards all */}
<TAG>{STRING}                           {/* HTML tags */}
<TAG>\>                                 BEGIN(INITIAL);

<BEGINTAG>{WHITE}+                      {/* eat whitespace to find tag name */}
<BEGINTAG>!--                           BEGIN(COMMENT); /* HTML comment */
<BEGINTAG>\/                            {/* eat slash in tags */}
<BEGINTAG>{NSPTAGS}                     BEGIN(TAG); /* tag to be eliminated altogether */
<BEGINTAG>\>                            { fputc(' ', yyout); BEGIN(INITIAL);  /* whoa. Empty tag?!? Replace with space */ };
<BEGINTAG>[A-Za-z0-9]+                  |
<BEGINTAG>[^\r\n]                       { fputc(' ', yyout); BEGIN(TAG); /* all else is a tag to be replaced with a space */ }                    

<INITIAL>\<                             BEGIN(BEGINTAG); /* tag that must be analyzed further (comment, spacing tag, non-spacing tag) */



<INITIAL>&nbsp;                         fputc(' ', yyout); /* replace special */
<INITIAL>&#161;                         fputc('¡', yyout); /* HTML odes with */
<INITIAL>&iexcl;                        fputc('¡', yyout); /* corresponding ISO */
<INITIAL>&#162;                         fputc('¢', yyout); /* codes */
<INITIAL>&cent;                         fputc('¢', yyout);
<INITIAL>&#163;                         fputc('£', yyout);
<INITIAL>&pound;                        fputc('£', yyout);
<INITIAL>&#164;                         fputc('¤', yyout);
<INITIAL>&curren;                       fputc('¤', yyout);
<INITIAL>&#165;                         fputc('¥', yyout);
<INITIAL>&yen;                          fputc('¥', yyout);
<INITIAL>&#166;                         fputc('¦', yyout);
<INITIAL>&brvbar;                       fputc('¦', yyout);
<INITIAL>&#167;                         fputc('§', yyout);
<INITIAL>&sect;                         fputc('§', yyout);
<INITIAL>&#168;                         fputc('¨', yyout);
<INITIAL>&uml;                          fputc('¨', yyout);
<INITIAL>&#169;                         fputc('©', yyout);
<INITIAL>&copy;                         fputc('©', yyout);
<INITIAL>&#170;                         fputc('ª', yyout);
<INITIAL>&ordf;                         fputc('ª', yyout);
<INITIAL>&#171;                         fputc('«', yyout);
<INITIAL>&laquo;                        fputc('«', yyout);
<INITIAL>&#172;                         fputc('¬', yyout);
<INITIAL>&not;                          fputc('¬', yyout);
<INITIAL>&#173;                         fputc('\\', yyout);
<INITIAL>&shy;                          fputc('\\', yyout);
<INITIAL>&#174;                         fputc('®', yyout);
<INITIAL>&reg;                          fputc('®', yyout);
<INITIAL>&#175;                         fputc('¯', yyout);
<INITIAL>&macr;                         fputc('¯', yyout);
<INITIAL>&#176;                         fputc('°', yyout);
<INITIAL>&deg;                          fputc('°', yyout);
<INITIAL>&#177;                         fputc('±', yyout);
<INITIAL>&plusmn;                       fputc('±', yyout);
<INITIAL>&#178;                         fputc('²', yyout);
<INITIAL>&sup2;                         fputc('²', yyout);
<INITIAL>&#179;                         fputc('³', yyout);
<INITIAL>&sup3;                         fputc('³', yyout);
<INITIAL>&#180;                         fputc('´', yyout);
<INITIAL>&acute;                        fputc('´', yyout);
<INITIAL>&#181;                         fputc('µ', yyout);
<INITIAL>&micro;                        fputc('µ', yyout);
<INITIAL>&#182;                         fputc('¶', yyout);
<INITIAL>&para;                         fputc('¶', yyout);
<INITIAL>&#183;                         fputc('·', yyout);
<INITIAL>&middot;                       fputc('·', yyout);
<INITIAL>&#184;                         fputc('¸', yyout);
<INITIAL>&cedil;                        fputc('¸', yyout);
<INITIAL>&#185;                         fputc('¹', yyout);
<INITIAL>&sup1;                         fputc('¹', yyout);
<INITIAL>&#186;                         fputc('º', yyout);
<INITIAL>&ordm;                         fputc('º', yyout);
<INITIAL>&#187;                         fputc('»', yyout);
<INITIAL>&raquo;                        fputc('»', yyout);
<INITIAL>&#188;                         fputc('¼', yyout);
<INITIAL>&frac14;                       fputc('¼', yyout);
<INITIAL>&#189;                         fputc('½', yyout);
<INITIAL>&frac12;                       fputc('½', yyout);
<INITIAL>&#190;                         fputc('¾', yyout);
<INITIAL>&frac34;                       fputc('¾', yyout);
<INITIAL>&#191;                         fputc('¿', yyout);
<INITIAL>&iquest;                       fputc('¿', yyout);
<INITIAL>&#192;                         fputc('À', yyout);
<INITIAL>&Agrave;                       fputc('À', yyout);
<INITIAL>&#193;                         fputc('Á', yyout);
<INITIAL>&Aacute;                       fputc('Á', yyout);
<INITIAL>&#194;                         fputc('Â', yyout);
<INITIAL>&circ;                         fputc('Â', yyout);
<INITIAL>&#195;                         fputc('Ã', yyout);
<INITIAL>&Atilde;                       fputc('Ã', yyout);
<INITIAL>&#196;                         fputc('Ä', yyout);
<INITIAL>&Auml;                         fputc('Ä', yyout);
<INITIAL>&#197;                         fputc('Å', yyout);
<INITIAL>&ring;                         fputc('Å', yyout);
<INITIAL>&#198;                         fputc('Æ', yyout);
<INITIAL>&AElig;                        fputc('Æ', yyout);
<INITIAL>&#199;                         fputc('Ç', yyout);
<INITIAL>&Ccedil;                       fputc('Ç', yyout);
<INITIAL>&#200;                         fputc('È', yyout);
<INITIAL>&Egrave;                       fputc('È', yyout);
<INITIAL>&#201;                         fputc('É', yyout);
<INITIAL>&Eacute;                       fputc('É', yyout);
<INITIAL>&#202;                         fputc('Ê', yyout);
<INITIAL>&Ecirc;                        fputc('Ê', yyout);
<INITIAL>&#203;                         fputc('Ë', yyout);
<INITIAL>&Euml;                         fputc('Ë', yyout);
<INITIAL>&#204;                         fputc('Ì', yyout);
<INITIAL>&Igrave;                       fputc('Ì', yyout);
<INITIAL>&#205;                         fputc('Í', yyout);
<INITIAL>&Iacute;                       fputc('Í', yyout);
<INITIAL>&#206;                         fputc('Î', yyout);
<INITIAL>&Icirc;                        fputc('Î', yyout);
<INITIAL>&#207;                         fputc('Ï', yyout);
<INITIAL>&Iuml;                         fputc('Ï', yyout);
<INITIAL>&#208;                         fputc('Ð', yyout);
<INITIAL>&ETH;                          fputc('Ð', yyout);
<INITIAL>&#209;                         fputc('Ñ', yyout);
<INITIAL>&Ntilde;                       fputc('Ñ', yyout);
<INITIAL>&#210;                         fputc('Ò', yyout);
<INITIAL>&Ograve;                       fputc('Ò', yyout);
<INITIAL>&#211;                         fputc('Ó', yyout);
<INITIAL>&Oacute;                       fputc('Ó', yyout);
<INITIAL>&#212;                         fputc('Ô', yyout);
<INITIAL>&Ocirc;                        fputc('Ô', yyout);
<INITIAL>&#213;                         fputc('Õ', yyout);
<INITIAL>&Otilde;                       fputc('Õ', yyout);
<INITIAL>&#214;                         fputc('Ö', yyout);
<INITIAL>&Ouml;                         fputc('Ö', yyout);
<INITIAL>&#215;                         fputc('×', yyout);
<INITIAL>&times;                        fputc('×', yyout);
<INITIAL>&#216;                         fputc('Ø', yyout);
<INITIAL>&Oslash;                       fputc('Ø', yyout);
<INITIAL>&#217;                         fputc('Ù', yyout);
<INITIAL>&Ugrave;                       fputc('Ù', yyout);
<INITIAL>&#218;                         fputc('Ú', yyout);
<INITIAL>&Uacute;                       fputc('Ú', yyout);
<INITIAL>&#219;                         fputc('Û', yyout);
<INITIAL>&Ucirc;                        fputc('Û', yyout);
<INITIAL>&#220;                         fputc('Ü', yyout);
<INITIAL>&Uuml;                         fputc('Ü', yyout);
<INITIAL>&#221;                         fputc('Ý', yyout);
<INITIAL>&Yacute;                       fputc('Ý', yyout);
<INITIAL>&#222;                         fputc('Þ', yyout);
<INITIAL>&THORN;                        fputc('Þ', yyout);
<INITIAL>&#223;                         fputc('ß', yyout);
<INITIAL>&szlig;                        fputc('ß', yyout);
<INITIAL>&#224;                         fputc('à', yyout);
<INITIAL>&agrave;                       fputc('à', yyout);
<INITIAL>&#225;                         fputc('á', yyout);
<INITIAL>&aacute;                       fputc('á', yyout);
<INITIAL>&#226;                         fputc('â', yyout);
<INITIAL>&acirc;                        fputc('â', yyout);
<INITIAL>&#227;                         fputc('ã', yyout);
<INITIAL>&atilde;                       fputc('ã', yyout);
<INITIAL>&#228;                         fputc('ä', yyout);
<INITIAL>&auml;                         fputc('ä', yyout);
<INITIAL>&#229;                         fputc('å', yyout);
<INITIAL>&aring;                        fputc('å', yyout);
<INITIAL>&#230;                         fputc('æ', yyout);
<INITIAL>&aelig;                        fputc('æ', yyout);
<INITIAL>&#231;                         fputc('ç', yyout);
<INITIAL>&ccedil;                       fputc('ç', yyout);
<INITIAL>&#232;                         fputc('è', yyout);
<INITIAL>&egrave;                       fputc('è', yyout);
<INITIAL>&#233;                         fputc('é', yyout);
<INITIAL>&eacute;                       fputc('é', yyout);
<INITIAL>&#234;                         fputc('ê', yyout);
<INITIAL>&ecirc;                        fputc('ê', yyout);
<INITIAL>&#235;                         fputc('ë', yyout);
<INITIAL>&euml;                         fputc('ë', yyout);
<INITIAL>&#236;                         fputc('ì', yyout);
<INITIAL>&igrave;                       fputc('ì', yyout);
<INITIAL>&#237;                         fputc('í', yyout);
<INITIAL>&iacute;                       fputc('í', yyout);
<INITIAL>&#238;                         fputc('î', yyout);
<INITIAL>&icirc;                        fputc('î', yyout);
<INITIAL>&#239;                         fputc('ï', yyout);
<INITIAL>&iuml;                         fputc('ï', yyout);
<INITIAL>&#240;                         fputc('ð', yyout);
<INITIAL>&ieth;                         fputc('ð', yyout);
<INITIAL>&#241;                         fputc('ñ', yyout);
<INITIAL>&ntilde;                       fputc('ñ', yyout);
<INITIAL>&#242;                         fputc('ò', yyout);
<INITIAL>&ograve;                       fputc('ò', yyout);
<INITIAL>&#243;                         fputc('ó', yyout);
<INITIAL>&oacute;                       fputc('ó', yyout);
<INITIAL>&#244;                         fputc('ô', yyout);
<INITIAL>&ocirc;                        fputc('ô', yyout);
<INITIAL>&#245;                         fputc('õ', yyout);
<INITIAL>&otilde;                       fputc('õ', yyout);
<INITIAL>&#246;                         fputc('ö', yyout);
<INITIAL>&ouml;                         fputc('ö', yyout);
<INITIAL>&#247;                         fputc('÷', yyout);
<INITIAL>&divide;                       fputc('÷', yyout);
<INITIAL>&#248;                         fputc('ø', yyout);
<INITIAL>&oslash;                       fputc('ø', yyout);
<INITIAL>&#249;                         fputc('ù', yyout);
<INITIAL>&ugrave;                       fputc('ù', yyout);
<INITIAL>&#250;                         fputc('ú', yyout);
<INITIAL>&uacute;                       fputc('ú', yyout);
<INITIAL>&#251;                         fputc('û', yyout);
<INITIAL>&ucirc;                        fputc('û', yyout);
<INITIAL>&#252;                         fputc('ü', yyout);
<INITIAL>&uuml;                         fputc('ü', yyout);
<INITIAL>&#253;                         fputc('ý', yyout);
<INITIAL>&yacute;                       fputc('ý', yyout);
<INITIAL>&#254;                         fputc('þ', yyout);
<INITIAL>&thorn;                        fputc('þ', yyout);
<INITIAL>&#255;                         fputc('ÿ', yyout);
<INITIAL>&yuml;                         fputc('ÿ', yyout);
<INITIAL>&#34;                          fputc('\"', yyout);
<INITIAL>&quot;                         fputc('\"', yyout);
<INITIAL>&#38;                          fputc('&', yyout);
<INITIAL>&amp;                          fputc('&', yyout);
<INITIAL>&#62;                          fputc('>', yyout);
<INITIAL>&gt;                           fputc('>', yyout);
<INITIAL>&#60;                          fputc('<', yyout);
<INITIAL>&lt;                           fputc('<', yyout); 

%%


/* Define this if the filter is to be loaded as a shared library. This
   is an experimental option and requires patches to glimpse, at least
   up to version 4.12.6. The resulting speedup in searches is impressive
   and well worth the hassle. 

   For the patch and instructions, contact cvogler@gradient.cis.upenn.edu.

   These patches might be merged into the main glimpse source tree in the
   future.
*/

#ifdef SHARED_OBJECT 

int filter_func(FILE *in, FILE *out)
{
    yyout = out;
    yyrestart(in);
    BEGIN(INITIAL); /* necessary to put scanner in known state if previous
		       file contained syntax errors, or unbalanced <, >, " */
    while (yylex())
	;

    return 0;       /* all o.k. */
}



#else 

/* filter is loaded as standalone external glimpse filter process. This
   is the default.
*/

int main(void)
{
    while (yylex())
	;
    return 1;
}

#endif

