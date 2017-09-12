/* Summer Institute of Linguistics (SIL) Consistent Change extended to
  Multi Stream Editor realized by Lidovski V. in IX-1999, II-2000, 
    XI,XII-2001, I,II,VIII-2002 FREEWARE (C) Copyright */

#include <cstdlib>
#include "data.h"
#include "mse.h"

char **logNames[4], **gargv, *outfn = 0, *cctfn = 0, *logfn, 
  *logPattern, *logPatternCur, infnlist[QINFILE] = "";
int gargc, *garr, *lvarr;
FILE *outfile = 0, *logfile = 0, *cctfile;
ArgElem *p1arg = 0;
short ArgElem::argid[4] = {0,0,0,0};
Operator *pfirst = 0, *pcur, *pbegin = 0, *pendfile = 0;
int Operator::qop = 0, Operator::curgroup = 0;
OutStream *sarrx, *soutx /*standard output*/, *curoutx /*current output*/, 
  pattern;
FuncElem **marr;
OpStack *pstack;

void errormsg(int n, int line = 0) {
  if (n >= 40)
    fprintf(stderr, "mse: run-time error: ");
  else if (n != 1)
    fprintf(stderr, "mse: ");
  switch (n) {
    case 1:
      printf("Usage: mse [OPTION]... [input-file]...\n\n");
      printf("  -t change table name\tdefault is empty table\n");
      printf("  -o output file\tdefault is stdout\n");
      printf("  -l log file\t\tdefault is no logging\n");
      printf("  --help\t\tdisplay this help and exit\n");
      printf("  -V\t\t\toutput version information and exit\n\n");
      printf("Default input file is stdin. ");
      printf("You may specify - (hyphen) for stdin or stdout.\n");
      printf("\nExamples:\n");
      printf("\tmse -t table.mse -o result.txt source1.txt ... sourceN.txt\n");
      printf("\tmse -V\n");
      n = 0;
      break;
    case 2:
      fprintf(stderr, "can't open %s for read\n", (char *) line);
      break;
    case 3:
      fprintf(stderr, "can't open %s for write\n", outfn);
      break;
    case 4:
      fprintf(stderr, "only one change table allowed\n");
      break;
    case 5:
      fprintf(stderr, "unknown parameter found\n");
      break;
    case 6:
      fprintf(stderr, "only one log file allowed\n");
      break;
    case 7:
      fprintf(stderr, "only one output file allowed\n");
      break;
    case 8:
      fprintf(stderr, "error in octal number,");
      break;
    case 9:
      fprintf(stderr, "error in number,");
      break;
    case 10:
      fprintf(stderr, "error in decimal number,");
      break;
    case 11:
      fprintf(stderr, "error in hexadecimal number,");
      break;
    case 12:
      fprintf(stderr, "PREVSYM argument less than 1");
      break;
    case 14:
      fprintf(stderr, "unknown or misplaced reserved word encountered in");
      break;
    case 15:
      fprintf(stderr, "unexpected end of line encountered in");
      break;
    case 16:
      fprintf(stderr, "number too big, must be within the range 0--255,");
      break;
    case 17:
      fprintf(stderr, "duplicated > in");
      break;
    case 18:
      fprintf(stderr, "syntax error in");
      break;
    case 19:
      fprintf(stderr, "literal exceeds %d bytes in", TMPSZ);
      break;
    case 20:
      fprintf(stderr, "empty argument in");
      break;
    case 21:
      fprintf(stderr, "ELSE without IF in");
      break;
    case 22:
      fprintf(stderr, "ENDIF without IF in");
      break;
    case 23:
      fprintf(stderr, "END without BEGIN in");
      break;
    case 24:
      fprintf(stderr, "REPEAT without BEGIN in");
      break;
    case 25:
      fprintf(stderr, "dublicated ELSE in");
      break;
    case 26:
      fprintf(stderr, "dublicated ENDIF in");
      break;
    case 27:
      fprintf(stderr, "error in DEFINE statement,");
      break;
    case 28:
      fprintf(stderr, "second argument is missing or bad");
      break;
    case 29:
      fprintf(stderr, "duplicated BEGIN in");
      break;
    case 30:
      fprintf(stderr, "BEGIN is not alone in search entry in");
      break;
    case 31:
      fprintf(stderr, "duplicated ENDFILE in");
      break;
    case 32:
      fprintf(stderr, "ENDFILE is not alone in search entry in");
      break;
    case 33:
      fprintf(stderr, "misplaced UNSORTED statement,");
      break;
    case 34:
      fprintf(stderr, "missing open parenthesis,");
      break;
    case 35:
      fprintf(stderr, "missing close parenthesis,");
      break;
    case 36:
      fprintf(stderr, "bad numerical argument in");
      break;
    case 37:
      break;
    case 40:
      fprintf(stderr, "BACK exceeds buffer\n");
      break;
    case 41:
      break;
    case 42:
      fprintf(stderr, "INCR buffer is too big\n");
      break;
    case 43:
      fprintf(stderr, "DECR buffer is too big\n");
      break;
    case 44:
      break;
    case 45:
      fprintf(stderr, "NEXT in the last line of change table\n");
      break;
    case 46:
      break;
    case 47:
      fprintf(stderr, "unsufficient memory\n");
      break;
    case 48:
      fprintf(stderr, "arithmetic operation with empty store\n");
      break;
    case 49:
      fprintf(stderr, "arithmetic operation with non-number store\n");
      break;
    case 50:
      fprintf(stderr, "arithmetic overflow\n");
      break;
    case 51:
      fprintf(stderr, "division by zero\n");
      break;
    case 52:
      fprintf(stderr, "\n");
      break;
    case 53:
      fprintf(stderr, "\n");
      break;
    case 54:
      fprintf(stderr, "can\'t open temporary file\n");
      break;
    case 55:
      fprintf(stderr, "can\'t write to temporary file\n");
      break;
    case 56:
      fprintf(stderr, "SYMDUP exceeds buffer\n");
      break;
    case 57:
      fprintf(stderr, "can\'t seek position in temporary file\n");
      break;
    case 58:
      fprintf(stderr, "can\'t read from temporary file\n");
      break;
    case 59:
      fprintf(stderr, "OutStream::operator[] index too big\n");
      break;
    case 60:
      fprintf(stderr, "InStream::operator[] index too big\n");
      break;
    default:
      fprintf(stderr, "unknown error\n");
  }
  if (line != 0 && n > 7) 
    fprintf(stderr, " line %d\n", line);
  exit(n);
}

void freedmem() {
  FuncElem *auxfp;
  delete [] sarrx;
  delete [] garr;
  delete [] lvarr;
  if (logfile) {
    delete [] logPattern;
    delete [] logPatternCur;
    for (ArgMode m = SN; m < Num; m = ArgMode(m + 1)) {
      for (int k = 0; k < ArgElem::argid[m]; k++)
        delete [] logNames[m][k];
      delete [] logNames[m];
    }
    fprintf(logfile, "</pre>\n");
    if (logfile != stdout)
      fclose(logfile);
  }
  while (pfirst != 0) {
    while (pfirst->s1 != 0) {
      auxfp = pfirst->s1;
      pfirst->s1 = pfirst->s1->next;
      delete auxfp;
    }
    while (pfirst->a1 != 0) {
      auxfp = pfirst->a1;
      pfirst->a1 = pfirst->a1->next;
      delete auxfp;
    }
    pcur = pfirst;
    pfirst = pfirst->next;
    delete pcur;
  }
  delete [] marr;
  delete pstack;
}

void setvars(void) {
  if (cctfn) {
    if (cctfn[0] == '-' && cctfn[1] == '\0')
      cctfile = stdin;
    else
      if ((cctfile = fopen(cctfn, "r")) == 0)
        throw xccErr(2, (long) cctfn);
    inputcct();
    if (cctfile != stdin)
      fclose(cctfile);
  }
  if (logfn) {
    if (logfn[0] == '-' && logfn[1] == '\0')
      logfile = stdout;
    else if ((logfile = fopen(logfn, "w")) == 0)
      throw xccErr(3);
    fprintf(logfile, "<pre>");
    logPattern = new char [TMPSZ];
    logPatternCur = new char [TMPSZ];
    logNames[SN] = new char* [ArgElem::argid[SN]];
    logNames[MN] = new char* [ArgElem::argid[MN]];
    logNames[LVN] = new char* [ArgElem::argid[LVN]];
    logNames[GN] = new char* [ArgElem::argid[GN]];
  }
  marr = new FuncElem* [ArgElem::argid[MN]];
  for (int k = 0; k < ArgElem::argid[MN]; k++)
    marr[k] = 0;
  while (p1arg != 0) {
    ArgElem *pcarg;
    if (p1arg->type == MN)
      marr[p1arg->id] = p1arg->pf1;
    if (logfile) {
      logNames[p1arg->type][p1arg->id] = new char [strlen(p1arg->name) + 1];
      strcpy(logNames[p1arg->type][p1arg->id], p1arg->name);
    }
    delete [] p1arg->name;
    pcarg = p1arg->next;
    delete p1arg;
    p1arg = pcarg;
  }
  lvarr = new int[ArgElem::argid[LVN]];
  for (int k = 0; k < ArgElem::argid[LVN]; k++)
    lvarr[k] = 0;
  garr = new int[ArgElem::argid[GN]];
  for (int k = 1; k < ArgElem::argid[GN]; k++)
    garr[k] = 0;
  garr[0] = 1;
  sarrx = new OutStream[ArgElem::argid[SN] + 1];
  curoutx = soutx = sarrx + ArgElem::argid[SN];
}

void getparams(void) {
  int count = 1;
  char *pstr;
  while (count < gargc) {
    pstr = gargv[count];
    if (pstr[0] == '-' && pstr[1] != '\0')
      switch (pstr[1]) {
        case '-':
          if (!strcmp(pstr, "--help"))
        case 'h':
            throw xccErr(1);
          throw xccErr(5);
        case 'V':
          printf("mse version 2.03\n\n");
          printf("Copyright (C) VI-2007, VII-2013 V. Lidovski\n");
          printf("This is free software\n");
          exit(0); 
	case 't':
	  count++;
          if (cctfn == 0) 
            cctfn = gargv[count++];
          else
            throw xccErr(4);
          p1arg = new ArgElem(GN);
          p1arg->name = new char[2];
          p1arg->name[0] = '1'; // starts from group 1!
          p1arg->name[1] = '\0';
	  break;
        case 'l':
          count++;
          if (logfn == 0)
            logfn = gargv[count++];
	  else
	    throw xccErr(6);
          break;
	case 'o':
	  count++;
          if (outfn == 0)
            outfn = gargv[count++];
	  else
	    throw xccErr(7);
	  break;
	default:
          throw xccErr(5);
      }
    else if (pstr[0] != '-' || strchr(infnlist, '-') == 0) {
      infnlist[strlen(infnlist) + 1] = 0; 
      infnlist[strlen(infnlist)] = count++; 
    }
  }
}

void setres(void) {
  unsigned char t[OBUFSZ];
  unsigned q = soutx->len/OBUFSZ;
  if (outfn == 0 || outfn[0] == '-' && outfn[1] == '\0')
    outfile = stdout;
  else if ((outfile = fopen(outfn, "wb")) == 0)
    throw xccErr(3);
  if (soutx->otmpfile != 0) {
    fseek(soutx->otmpfile, 0, SEEK_SET);
    while (q--) {
      fread(t, 1, OBUFSZ, soutx->otmpfile);
      fwrite(t, 1, OBUFSZ, outfile);
    }
  }
  fwrite(soutx->obuf, 1, soutx->ptr, outfile);
  if (outfile != stdout)
    fclose(outfile);
}

int main(int argc, char *argv[]) {
  gargv = argv;
  gargc = argc;
  try {
    pstack = new OpStack;
    getparams();
    setvars();
    CC();
    setres();
    freedmem();
  }
  catch (bad_alloc) {
    errormsg(47);
  }
  catch (xccErr e) {
    errormsg(e.n, e.line);
  }
  catch (...) {
    errormsg(127);
  }
  return 0;
}
