#include "data.h"
#include "lexfn.h"

#define QRWS 11
#define QRWA 43
#define WEDGE 257
#define LBRACE 258
#define RBRACE 259
#define COMMA 260
#define RW 256
#define BSIZE 16384

extern FILE *cctfile, *logfile; /* xcc */
extern Operator *pfirst, *pcur, *pbegin, *pendfile; /* xcc */
extern OpStack *pstack; /* xcc */
extern ArgElem *p1arg; /* xcc */

FuncElem *auxfp;
unsigned char lexb[BSIZE];
char tmpstore[TMPSZ], delims[] = " \n\t,)", delims2[] = "\t\n\'\"> ", quote, 
  mode6 = 0; 

int quotemode = 0 /* 0 - std, 1 - quote */, line = 0, 
  inmode = 0 /* 0 - subst, 1 - action */, litlen = 0,
  rwlen[2] = {QRWS, QRWA}, lex, rwi, hexmode = 0, 
  lexi = 0 /* lexb index */, sorted = 1;

struct RWElem {
  char *name;
  void (*fp)(void);
} rws[QRWS] = {
{"FOL", fol},       {"PRECI", preci},    {"WD", wd},          {"ANY", any},
{"CONT", cont},     {"ENDFILE", endf},   {"BEGIN", init},     {"NL", nl},
{"DEFINE", define}, {"PREVSYM", prevsym},{"PREC", prec}},
  rwa[QRWA] = {
{"ADD", add},       {"APPEND", append},  {"BACKI", backi},    {"BEGIN", begin},
{"CLEAR", clear},   {"CONT", conta},     {"DIV", div},        {"DO", doa},
{"DUP", dup},       {"ELSE", elsea},     {"ENDSTORE", ends},  {"ENDIF", endif},
{"ENDFILE", endfa}, {"END", end},        {"EXCL", excl},      {"FWD", fwd},
{"IFNEQ", ifneq},   {"IFEQ", ifeq},      {"IFGT", ifgt},      {"IFN", ifn},
{"IF", ifa},        {"INCL", incl},      {"INCR", incr},      {"MOD", mod},
{"MUL", mul},       {"NEXT", next},      {"NL", nla},         {"OMIT", omit},
{"OUTS", outs},     {"OUT", out},        {"READ", read},      {"REPEAT", rep},
{"SET", set},       {"STORE", store},    {"SUB", sub},        {"USE", use},
{"WRITE", write},   {"WRSTORE", wrstore},{"UNSORTED", unsort},{"SYMDUP", symdup}, 
{"DECR", decr},     {"BACK", back},      {"NOT", nota}}, *rwptr[2] = {rws, rwa};

void skipspc(void) {
  while (strchr("\n\t ", lexb[lexi]) != 0) 
    lexi++;
}

int inoct(void) {
  int n = 0;
  while (lexb[lexi] >= '0' && lexb[lexi] <= '7') {
    n = n*8 + lexb[lexi++] - '0';
    if (n > MAXLONG8) 
      throw xccErr(16, line);
  }
  if (strchr(delims2, lexb[lexi]) == 0) 
    throw xccErr(8, line);
  if (n > 255)
    throw xccErr(16, line);
  return n;
}

int indec(void) {
  int n = 0;
  if (lexb[lexi] < '0' || lexb[lexi] > '9') 
    throw xccErr(10, line);
  while (lexb[lexi] >= '0' && lexb[lexi] <= '9') {
    if (n > MAXLONG10) 
      throw xccErr(16, line);
    n = n*10 + lexb[lexi++] - '0';
  }
  if (strchr(delims2, lexb[lexi]) == 0) 
    throw xccErr(10, line);
  if (n > 255)
    throw xccErr(16, line);
  return n;
}

int inhex(void) {
  int n = 0, k = 2;
  if ((lexb[lexi] < '0' || lexb[lexi] > '9') && 
      (lexb[lexi] < 'A' || lexb[lexi] > 'F'))
    throw xccErr(11, line);
  while (lexb[lexi] >= '0' && lexb[lexi] <= '9' || 
      lexb[lexi] >= 'A' && lexb[lexi] <= 'F' ) {
    n = n*16 + lexb[lexi] - (lexb[lexi] > '9' ? 55 : '0');
    if (n > MAXLONG16)
      throw xccErr(16, line);
    ++lexi;
    if (!(--k)) 
      break;
  }
  return n;
}

ArgElem *argsearch(unsigned char *s, ArgMode argmode) {
  ArgElem *ptmp = p1arg;
  int k = 0;
  while (strchr(delims, s[k]) == 0) 
    k++;
  if (!k) 
    throw xccErr(20, line);
  lexi += k;
  if (ptmp != 0)
    while (1) {
      if (argmode != ptmp->type || strncmp(ptmp->name, (char *)s, k))
        if (ptmp->next == 0)
          break;
        else
          ptmp = ptmp->next;
      else
	return ptmp;
    }
  ptmp = ptmp->next = new ArgElem(argmode);
  ptmp->name = new char[k + 1];
  strncpy(ptmp->name, (char *) s, k);
  ptmp->name[k] = '\0';
  return ptmp;
}

int xindec(void) {
  int n = 0;
  if (lexb[lexi] < '0' || lexb[lexi] > '9') 
    throw xccErr(36, line);
  while (lexb[lexi] >= '0' && lexb[lexi] <= '9') {
    if (n > MAXLONG10) 
      throw xccErr(36, line);
    n = n*10 + lexb[lexi++] - '0';
  }
  if (strchr(delims, lexb[lexi]) == 0) 
    throw xccErr(36, line);
  return n;
}

int toupcase(void) {
  int k = 0, operend = 0;
  while (lexb[k] != '\0') {
    if (lexb[k] == '\"')
      while (lexb[++k] != '\"');
    else if (lexb[k] == '\'')
      while (lexb[++k] != '\'');
    else if (lexb[k] >= 'a' && lexb[k] <= 'z' || lexb[k] == '%') {
      lexb[k] -= 32;
      if (lexb[k] == 5 || lexb[k] == 'C'
&& (lexb[k + 1] == ' ' || lexb[k + 1] == '\t' || lexb[k + 1] == '\n')) {
        lexb[k] = '\0';
        continue;
      }
    }
    else if (lexb[k] == '>') 
      operend++;
    k++;
  }
  if (operend > 1) 
    throw xccErr(17, line);
  return operend;
}

void syntaxok(void) {
  while (pstack->ptop != 0) {
    if (pstack->ptop->type != 2)
      throw xccErr(23, line);
    pstack->ptop->label->label2 = auxfp;
    if (pstack->ptop->label->label1 != 0)
      pstack->ptop->label->label1->label1 = auxfp;
    pstack->pop();
  }
}

int getline(void) {
  if (fgets((char *)lexb, BSIZE, cctfile) == 0)
    return 0;
  while (lexb[strlen((char *)lexb) - 2] == '\\' 
          && lexb[strlen((char *)lexb) - 1] == '\n') {
    fgets((char *)lexb + strlen((char *)lexb) - 2, BSIZE, cctfile);
    line++;
  }
  lexi = 0;
  line++;
  if (toupcase() && inmode) {
    syntaxok();
    inmode--;
  }
  return 1;
}

int rwsearch(unsigned char *s, RWElem *a, int size) {
  int k;
  for (k = 0; k < size; k++)
    if (strstr((char *)s, a[k].name) == (char *)s)
      break;
  if (k == size || strchr("\n\t\'\" (>", s[strlen(a[k].name)]) == 0)
    throw xccErr(14, line);
  return k;
}

void endlit(void) {
  if (litlen) {
    auxfp->string = new unsigned char[litlen];
    auxfp->argi = auxfp->stringlength = litlen;
    memcpy(auxfp->string, tmpstore, litlen);
    litlen = 0;
  }
}

void rearrange(void) {  // set sort priority for fol, prec, preci
  FuncElem *ptmp = pcur->s1;  // and handle define command
  int pc = 0, fc = 0, pdc = 0;
  endlit();
  while (ptmp != 0) {
    if (ptmp->pfi == xfol)
      ptmp->argi2 = fc++;
    else if (ptmp->pfi == xprec)
      pc++;
    else if (ptmp->pfi == xpreci)
      pdc++;
    else if (ptmp->pfi == xdefine) {
      if (ptmp->next != 0)
        xccErr(27, line);
      ((ArgElem *)ptmp->label2)->pf1 = pcur->a1;
      return;
    }  
    ptmp = ptmp->next;
  }
  ptmp = pcur->s1;
  while (pc) {
    if (ptmp->pfi == xprec)
      ptmp->argi2 = --pc;
    if (ptmp->pfi == xpreci)
      ptmp->argi2 = --pdc;
    ptmp = ptmp->next;
  }
}

int getlex(void) {
  int c;
  while (1) {
    c = lexb[lexi];
    if (quotemode)
      if (quote == c) {
	c = lexb[++lexi];
	quotemode--;
      }
      else if (c == '\n')
        throw xccErr(15, line);
      else
	return lexb[lexi++];
    if (hexmode)
      if (strchr(delims2, c) != 0)
        hexmode = 0;
      else
        return inhex();
    switch (c) {
      case '\'':
      case '\"':
	if (lexb[++lexi] != c) {
	  quote = c;
	  quotemode++;
	  return lexb[lexi++];
	}
      case ' ':
      case '\n':
      case '\t':
	lexi++;
	break;
      case '\0':
	if (getline())
	  break;
	return -1;
      case '>':
	lexi++;
	return WEDGE;
      case '0':
	if (lexb[++lexi] == 'X') {
      case 'X':
	  lexi++;
          hexmode = 1;
	  return inhex();
	}
	else if (lexb[lexi] == 'D') {
      case 'D':
          if (lexb[lexi + 1] < '0' || lexb[lexi + 1] > '9') 
            goto L1;
	  lexi++;
	  return indec();
	}
	else if (lexb[lexi] <= '7' && lexb[lexi] >= '0')
	  return inoct();
	else
	  throw xccErr(9, line);
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
	return inoct();
      case '(':
	lexi++;
	return LBRACE;
      case ')':
	lexi++;
	return RBRACE;
      case ',':
	lexi++;
	return COMMA;
      case 'G':
        if (lexb[lexi + 1] == 'R' && lexb[lexi + 2] == 'O' && 
            lexb[lexi + 3] == 'U' && lexb[lexi + 4] == 'P') {
          lexi += 5;
          group();
          break;
	}
      default:
L1:
        rwi = rwsearch(lexb + lexi, rwptr[inmode], rwlen[inmode]);
        lexi += strlen(rwptr[inmode][rwi].name);
        return RW;
    }
  }
}

void addyopp5(void (*fp)(void)) {
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  auxfp = (pcur->a1 == 0 ? pcur->a1 : auxfp->next) = new FuncElem;
  auxfp->pfv = fp;
  auxfp->argi = xindec();
  while ((lex = getlex()) == COMMA) {
    skipspc();
    auxfp = auxfp->next = new FuncElem;
    auxfp->pfv = fp;
    auxfp->argi = xindec();
  }
  if (lex != RBRACE) 
    throw xccErr(35, line);
}

void addyopp6(void (*fp)(void)) {
  int k = 0, si, sl;
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  auxfp = (pcur->a1 == 0 ? pcur->a1 : auxfp->next) = new FuncElem;
  auxfp->pfv = fp;
  auxfp->argi = argsearch(lexb + lexi, SN)->id;
  if (getlex() != RBRACE) 
    throw xccErr(35, line);
  if ((lex = getlex()) == RW) {
    if (rwa[rwi].fp != conta) 
      throw xccErr(14, line);
    if (getlex() != LBRACE) 
      throw xccErr(34, line);
    skipspc();
    auxfp->argi2 = argsearch(lexb + lexi, SN)->id;
    if (getlex() != RBRACE) 
      throw xccErr(35, line);
  }
  else if (lex < 256) {
    while (lex < 256 && inmode && lex >= 0) {
      tmpstore[k++] = lex;
      si = lexi;
      sl = line;
      lex = getlex();
    }
    if (sl == line)
      lexi = si + 1;
    else {
      lexi = 0;
      if (!inmode)
        mode6 = 1;
    }
    if (k > TMPSZ) 
      throw xccErr(19, line);
    auxfp->string = new unsigned char [k];
    memcpy(auxfp->string, tmpstore, k);
    auxfp->stringlength = k;
    auxfp->argi2 = -1;
  }
  else
    throw xccErr(28, line);
}

void addyopp1234(void (*fp)(void), ArgMode argtype) {
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  auxfp = (pcur->a1 == 0 ? pcur->a1 : auxfp->next) = new FuncElem;
  auxfp->pfv = fp;
  auxfp->argi = argsearch(lexb + lexi, argtype)->id;
  while ((lex = getlex()) == COMMA) {
    skipspc();
    auxfp = auxfp->next = new FuncElem;
    auxfp->pfv = fp;
    auxfp->argi = argsearch(lexb + lexi, argtype)->id;
  }
  if (lex != RBRACE) 
    throw xccErr(35, line);
}

void addyopp0(void (*fp)(void)) {
  auxfp = (pcur->a1 == 0 ? pcur->a1 : auxfp->next) = new FuncElem;
  auxfp->pfv = fp;
}

void addxfunc(void) {
  if (pcur == pbegin)
    throw xccErr(30, line);
  if (pcur == pendfile)
    throw xccErr(32, line);
  auxfp = (pcur->s1 == 0 ? pcur->s1 : auxfp->next) = new FuncElem;
}

void literal(void) {
  if (!litlen)
    if (inmode) {
      auxfp = (pcur->a1 == 0 ? pcur->a1 : auxfp->next) = new FuncElem;
      auxfp->pfv = yliteral;
    }
    else {
      addxfunc();
      auxfp->pfi = xliteral;
    }
  tmpstore[litlen++] = lex;
}

void addxopp(int (*pf)(void)) {
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  addxfunc();
  auxfp->pfi = pf;
  auxfp->argi = argsearch(lexb + lexi, SN)->id;
  while ((lex = getlex()) == COMMA) {
    skipspc();
    auxfp = auxfp->next = new FuncElem;
    auxfp->pfi = pf;
    auxfp->argi = argsearch(lexb + lexi, SN)->id;
  }
  if (lex != RBRACE) 
    throw xccErr(35, line);
}

void fol(void) {
  addxopp(xfol);
}

void prevsym(void) {
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  addxfunc();
  auxfp->pfi = xprevsym;
  if ((auxfp->argi = xindec()) < 1)
    throw xccErr(12, line);
  while ((lex = getlex()) == COMMA) {
    skipspc();
    auxfp = auxfp->next = new FuncElem;
    auxfp->pfi = xprevsym;
    if ((auxfp->argi = xindec()) < 1)
      throw xccErr(12, line);
  }
  if (lex != RBRACE) 
    throw xccErr(35, line);
}

void preci(void) {
  addxopp(xpreci);
}

void prec(void) {
  addxopp(xprec);
}

void wd(void) {
  int k;
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  addxfunc();
  auxfp->pfi = xprec;
  k = auxfp->argi = argsearch(lexb + lexi, SN)->id;
  auxfp = auxfp->next = new FuncElem;
  auxfp->pfi = xfol;
  auxfp->argi = k;
  while ((lex = getlex()) == COMMA) {
    skipspc();
    auxfp = auxfp->next = new FuncElem;
    auxfp->pfi = xprec;
    k = auxfp->argi = argsearch(lexb + lexi, SN)->id;
    auxfp = auxfp->next = new FuncElem;
    auxfp->pfi = xfol;
    auxfp->argi = k;
  }
  if (lex != RBRACE) 
    throw xccErr(35, line);
}

void any(void) {
  addxopp(xany);
}

void cont(void) {
  addxopp(xcont);
}

void nl(void) {
  addxfunc();
  auxfp->pfi = xnl;
}

void endf(void) {
  if (pendfile != 0)
    throw xccErr(31, line);
  if (pcur->s1 != 0)
    throw xccErr(32, line);
  pendfile = pcur;
  auxfp = pcur->s1 = new FuncElem;
  auxfp->pfi = xendf;
}

void init(void) {
  if (pbegin != 0)
    throw xccErr(29, line);
  if (pcur->s1 != 0)
    throw xccErr(30, line);
  pbegin = pcur;
  auxfp = pcur->s1 = new FuncElem;
  auxfp->pfi = xinit;
}

void group(void) {
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  Operator::curgroup = argsearch(lexb + lexi, GN)->id;
  while ((lex = getlex()) == COMMA) {
    skipspc();
    Operator::curgroup = argsearch(lexb + lexi, GN)->id;
  }
  if (lex != RBRACE) 
    throw xccErr(35, line);
  if (inmode) {
    syntaxok();
    inmode--;
  }
  else if (pcur != pfirst)
    throw xccErr(14, line);
}

void define(void) {
  if (pcur->s1 != 0)
    throw xccErr(27, line);
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  addxfunc();
  auxfp->pfi = xdefine;
  auxfp->label2 = (FuncElem *) argsearch(lexb + lexi, MN);
  if (getlex() != RBRACE) 
    throw xccErr(35, line);
}

void add(void) {
  addyopp6(yadd);
}

void append(void) {
  addyopp1234(yappend, SN);
}

void back(void) {
  addyopp5(yback);
}

void backi(void) {
  addyopp5(ybacki);
}

void begin(void) {
  if (logfile)
    addyopp0(ybegin);
  if (pcur->a1 == 0)
    auxfp = 0;
  pstack->push(0, auxfp);
}

void clear(void) {
  addyopp1234(yclear, LVN);
}

void conta(void) {
  throw xccErr(14, line);
}

void div(void) {
  addyopp6(ydiv);
}

void doa(void) {
  addyopp1234(ydoa, MN);
}

void dup(void) {
  addyopp0(ydup);
}

void symdup(void) {
  addyopp5(ysymdup);
}

void elsea(void) {
  if (pstack->ptop == 0 || pstack->ptop->type != 2)
    throw xccErr(21, line);
  addyopp0(yelsea);
  if (pstack->ptop->label->label1 == 0)
    pstack->ptop->label->label1 = auxfp;
  else
    throw xccErr(25, line);
}

void end(void) {
  if (logfile)
    addyopp0(yend);
  if (pstack->ptop == 0)
    throw xccErr(23, line);
  while (pstack->ptop != 0 && pstack->ptop->type == 2) {
    pstack->ptop->label->label2 = auxfp;
    if (pstack->ptop->label->label1 != 0)
      pstack->ptop->label->label1->label1 = auxfp;
    pstack->pop();
  }
  if (pstack->ptop == 0 || pstack->ptop->type != 0)
    throw xccErr(23, line);
  pstack->pop();
}

void endif(void) {
  if (logfile)
    addyopp0(yendif);
  if (pstack->ptop == 0 || pstack->ptop->type != 2)
    throw xccErr(22, line);
  if (pstack->ptop->label->label2 == 0) {
    pstack->ptop->label->label2 = auxfp;
    if (pstack->ptop->label->label1 != 0)
      pstack->ptop->label->label1->label1 = auxfp;
  }
  else
    throw xccErr(26, line);
  pstack->pop();
}

void ends(void) {
  addyopp0(yends);
}

void endfa(void) {
  addyopp0(yendfa);
}

void excl(void) {
  addyopp1234(yexcl, GN);
}

void fwd(void) {
  addyopp5(yfwd);
}

void ifneq(void) {
  addyopp6(yifneq);
  pstack->push(2, auxfp);
}

void ifeq(void) {
  addyopp6(yifeq);
  pstack->push(2, auxfp);
}

void ifgt(void) {
  addyopp6(yifgt);
  pstack->push(2, auxfp);
}

void ifn(void) {
  addyopp1234(yifn, LVN);
  pstack->push(2, auxfp);
}

void ifa(void) {
  addyopp1234(yifa, LVN);
  pstack->push(2, auxfp);
}

void incl(void) {
  addyopp1234(yincl, GN);
}

void incr(void) {
  addyopp1234(yincr, SN);
}

void decr(void) {
  addyopp1234(ydecr, SN);
}

void mod(void) {
  addyopp6(ymod);
}

void mul(void) {
  addyopp6(ymul);
}

void nota(void) {
  addyopp1234(ynot, LVN);
}

void next(void) {
  addyopp0(ynext);
}

void nla(void) {
  addyopp0(ynla);
}

void omit(void) {
  addyopp5(yomit);
}

void outs(void) {
  addyopp1234(youts, SN);
}

void out(void) {
  addyopp1234(yout, SN);
}

void read(void) {
  addyopp0(yread);
}

void rep(void) {
  StackElem *ptmp = pstack->ptop;
  while (ptmp != 0)
    if (ptmp->type == 0)
      break;
    else
      ptmp = ptmp->prev;
  if (ptmp == 0)
    throw xccErr(24, line);
  addyopp0(yrep);
  auxfp->label1 = ptmp->label;
}

void set(void) {
  addyopp1234(yset, LVN);
}

void store(void) {
  addyopp1234(ystore, SN);
}

void sub(void) {
  addyopp6(ysub);
}

void use(void) {
  if (getlex() != LBRACE) 
    throw xccErr(34, line);
  skipspc();
  auxfp = (pcur->a1 == 0 ? pcur->a1 : auxfp->next) = new FuncElem;
  auxfp->pfv = yuse;
  auxfp->argi = argsearch(lexb + lexi, GN)->id;
  while ((lex = getlex()) == COMMA) {
    skipspc();
    auxfp = auxfp->next = new FuncElem;
    auxfp->pfv = yincl;
    auxfp->argi = argsearch(lexb + lexi, GN)->id;
  }
  if (lex != RBRACE) 
    throw xccErr(35, line);
}

void write(void) {
  auxfp = ((pcur->a1 == 0)? pcur->a1: auxfp->next) = new FuncElem;
  auxfp->pfv = ywrite;
  auxfp->argi2 = -1;
}

void wrstore(void) {
  addyopp1234(ywrstore, SN);
}

void unsort(void) {
  if (pcur != pbegin)
    throw xccErr(33, line);
  if (logfile)
    addyopp0(yunsort);
  sorted = 0;
}

void inputcct(void) {
  int savedm;
  pcur = pfirst = new Operator;
  lexb[0] = '\0';
  do {
    savedm = inmode;
    if ((lex = getlex()) < 0)
      break;
    if (inmode < savedm) {
      rearrange();
      pcur = pcur->next = new Operator;
    }
    if (lex < RW)
      literal();
    else {
      endlit();
      if (lex == RW) {
        rwptr[inmode][rwi].fp();
        if (mode6) {
          rearrange();
          pcur = pcur->next = new Operator;
          mode6 = 0;
          quotemode = 0;
          hexmode = 0;
          continue;
        }
      }
      else if (lex == WEDGE)
        inmode++;
      else
        throw xccErr(18, line);
    }
  } while (1);
  rearrange();
  syntaxok();
}
