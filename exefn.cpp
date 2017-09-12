#include "data.h"

#define MAXQOP 100000L
#define INCRLEN MAXQOP*1000L

extern char tmpstore[] /* lexfn */, **logNames[4], *logPattern, 
  *logPatternCur /* xcc */;
extern FILE *logfile /* xcc */;
extern int *garr, *lvarr /* xcc */, sorted /* lexfn */;
extern Operator *pfirst, *pcur, *pbegin, *pendfile; /* xcc */
extern FuncElem **marr; /* xcc */
extern OpStack *pstack; /* xcc */
extern OutStream *sarrx, *soutx, *curoutx, pattern; /* xcc */

InStream inStream;
struct FuncElem *pftmp, faux;
int patlen, writeflag = 0;
unsigned long oporder;

void fillpat(void) {
  int i;
  for (i = 0; i < patlen; i++)
    pattern.push(inStream.pop());
}

void delpat(void) {
  pattern.reset();
}

void adjustnumber(int n, OutStream *p) {
  sprintf(tmpstore, "%d", n);
  n = 0;
  p->reset();
  while (tmpstore[n])
    p->push(tmpstore[n++]);
}

long zindec(OutStream *p) {
  long n = 0, sign = 1, i = p->len - 1;
  if ((*p)[i] == '-') {
    sign = -1;
    i--;
  } 
  else if ((*p)[i] == '+')
    i++;
  while (i >= 0) {
    if ((*p)[i] < '0' || (*p)[i] > '9')
      throw xccErr(49);
    if (n > MAXLONG10)
      throw xccErr(50);
    n = n*10 + (*p)[i--] - '0';
  }
  n *= sign;
  return n;
}

long zindec2(unsigned char *s, int len ) {
  long n = 0, sign = 1, i = 0;
  if (s[0] == '-') {
    sign = -1;
    i--;
  } 
  else if (s[0] == '+')
    i--;
  while (i < len) {
    if (s[i] < '0' || s[i] > '9')
      throw xccErr(49);
    if (n > MAXLONG10)
      throw xccErr(50);
    n = n*10 + s[i++] - '0';
  }
  n *= sign;
  return n;
}

void xDbgInfoS(const unsigned char *s, int len) {
  if (logfile) {
    int qm = 0;
    char t[17];
    for (int i = 0; i < len; i++) {
      if (s[i] > 31 && s[i] != 127) {
        if (!qm) {
          strcat(logPattern, " '");
          qm = 1;
        }
        sprintf(t, "%c", s[i]);
      } 
      else {
        if (qm) {
          strcat(logPattern, "'");
          qm = 0;
        }
        sprintf(t, " 0d%d", s[i]);
      }
      strcat(logPattern, t);
    }
    if (qm)
      strcat(logPattern, "'");
  }
}

void xDbgInfo(const char *s) {
  if (logfile) {
    strcat(logPattern, " ");
    strcat(logPattern, s);
    strcat(logPattern, "(");
    strcat(logPattern, logNames[SN][pftmp->argi]);
    strcat(logPattern, ")");
  }
}

void xDbgInfo0(const char *s) {
  if (logfile) {
    strcat(logPattern, " ");
    strcat(logPattern, s);
  }
}

void xDbgInfo5(const char *s) {
  if (logfile) {
    char t[TMPSZ];
    strcat(logPattern, " ");
    strcat(logPattern, s);
    strcat(logPattern, "(");
    sprintf(t, "%d", pftmp->argi);
    strcat(logPattern, t);
    strcat(logPattern, ")");
  }
}

int xliteral(void) {
  int n = 0;
  unsigned char c;
  long i = patlen;
  while (n < pftmp->argi) {
    c = inStream[i];
    if (inStream.end(i) || c != pftmp->string[n++])
      return 0;
    i++;
  }
  oporder += INCRLEN*n;
  patlen = i;
  xDbgInfoS(pftmp->string, pftmp->stringlength);
  return 1;
}

int xfol(void) {
  int n = pftmp->argi2 + patlen;
  if (inStream.end(n))
    return 0;
  if (sarrx[pftmp->argi].search(inStream[n])) {
    oporder += MAXQOP;
    xDbgInfo("fol");
    return 1;
  }
  return 0;
}

int xprevsym(void) {
  int n = patlen - pftmp->argi;
  if (n < 0)
    return 0;
  if (inStream[n] == inStream[patlen]) {
    oporder += INCRLEN;
    patlen++;
    xDbgInfo5("prevsym");
    return 1;
  }
  return 0;
}

int xpreci(void) {
  int n = pftmp->argi2;
  if (n >= (int) inStream.forprec->len)
    return 0;
  if (sarrx[pftmp->argi].search((*inStream.forprec)[n])) {
    oporder += MAXQOP;
    xDbgInfo("preci");
    return 1;
  }
  return 0;
}

int xprec(void) {
  int n = pftmp->argi2;
  if (n >= (int) curoutx->len)
    return 0;
  if (sarrx[pftmp->argi].search((*curoutx)[n])) {
    oporder += MAXQOP;
    xDbgInfo("prec");
    return 1;
  }
  return 0;
}

int xany(void){
  if (!inStream.end(patlen) && 
      sarrx[pftmp->argi].search(inStream[patlen])) {
    patlen++;
    oporder += INCRLEN;
    xDbgInfo("any");
    return 1;
  }
  return 0;
}

int xcont(void) {
  unsigned int i = 0;
  if (inStream.end(patlen + sarrx[pftmp->argi].len))
    return 0;
  while (i != sarrx[pftmp->argi].len)
    if (!inStream.end(patlen + i) && inStream[patlen + i] == 
          sarrx[pftmp->argi][sarrx[pftmp->argi].len - i - 1]) 
      i++;
    else
      return 0;
  patlen += i;
  oporder += sarrx[pftmp->argi].len*INCRLEN;
  xDbgInfo("cont");
  return 1;
}

int xendf(void) {
  xDbgInfo0("endfile");
  return 0;
}

int xnl(void) {
  int n = 0;
  unsigned char c;
  long i = patlen;
#ifdef PCDOS  
  c = inStream[i];
  if (inStream.end(i) || c != 0xd)
    return 0;
  n++;  
  i++;
#endif  
  c = inStream[i];
  if (inStream.end(i) || c != '\n')
    return 0;
  n++;  
  i++;
  oporder += INCRLEN*n;
  patlen = i;
  xDbgInfo0("nl");
  return 1;
/*
  if (inStream.end(patlen)) 
    return 0;
  if (inStream[patlen] == '\n') {
    patlen++;
    oporder += INCRLEN;
    xDbgInfo0("nl");
    return 1;
  }
  return 0;
*/
}

int xinit(void) {
  xDbgInfo0("begin");
  return 0;
}

int xdefine(void) {
  xDbgInfo("define");
  return 0;
}

void prchHTML(char c) {
  if (c == '<')
    fprintf(logfile, "&lt;"); 
  else if (c == '>')
    fprintf(logfile, "&gt;"); 
  else if (c == '&')
    fprintf(logfile, "&amp;"); 
  else
    fprintf(logfile, "%c", c);
}

void DbgInfoS(unsigned char *s, int len) {
  if (logfile) {
    int qm = 0;
    for (int i = 0; i < len; i++)
      if (s[i] > 31 && s[i] != 127) {
        if (!qm) {
          fprintf(logfile, " '");
          qm = 1;
        }
        prchHTML(s[i]);
      } 
      else {
        if (qm) {
          fprintf(logfile, "'");
          qm = 0;
        }
        fprintf(logfile, " 0d%d", s[i]);
      }
    if (qm)
      fprintf(logfile, "'");
  }
}

char* toHTML(const char *s) {
  static char tmpstr[TMPSZ];
  tmpstr[0] = '\0';
  for (unsigned int i = 0; i < strlen(s); i++)
    if (s[i] == '<')
      strcat(tmpstr, "&lt;");
    else if (s[i] == '>')
      strcat(tmpstr, "&gt;");
    else
      strncat(tmpstr, s + i,1);
  return tmpstr;
}

void DbgInfo6(const char *s) {
  if (logfile) {
    fprintf(logfile, " %s", toHTML(s));
    fprintf(logfile, "(%s)", toHTML(logNames[SN][pftmp->argi]));
    if (pftmp->argi2 < 0)
      DbgInfoS(pftmp->string, pftmp->stringlength); 
    else
      fprintf(logfile, " cont(%s)", toHTML(logNames[SN][pftmp->argi2]));
  }
}

void DbgInfo5(const char *s) {
  if (logfile)
    fprintf(logfile, " %s(%d)", toHTML(s), pftmp->argi);
}

void DbgInfo1234(ArgMode m, const char *s) {
  if (logfile) {
    fprintf(logfile, " %s", toHTML(s));
    fprintf(logfile, "(%s)", toHTML(logNames[m][pftmp->argi]));
  }
}

void DbgInfo0(const char *s) {
  if (logfile)
    fprintf(logfile, " %s", toHTML(s));
}

void yliteral(void) {
  int n = 0;
  while (n < pftmp->argi)
    if (writeflag)
      printf("%c", pftmp->string[n++]);
    else 
      curoutx->push(pftmp->string[n++]);
  DbgInfoS(pftmp->string, pftmp->stringlength);
}

void yappend(void) {
  writeflag = 0;
  curoutx = sarrx + pftmp->argi;
  DbgInfo1234(SN, "append");
}

void yadd(void) {
  writeflag = 0;
  adjustnumber(zindec(&sarrx[pftmp->argi]) +
      (pftmp->argi2 < 0 ?
        zindec2(pftmp->string, pftmp->stringlength) : 
        zindec(&sarrx[pftmp->argi2])),
      sarrx + pftmp->argi);
  DbgInfo6("add");
}

void ynla(void) {
  if (writeflag)
    printf("\n");
  else {
#ifdef PCDOS 
    curoutx->push(0xd);
#endif
    curoutx->push('\n');
  }  
  DbgInfo0("nl");
}

void yback(void) {
  int k = pftmp->argi;
  writeflag = 0;
  while (k--) 
    inStream.push(curoutx->pop());
  DbgInfo5("back");
}

void ybacki(void) {
  int k = pftmp->argi;
  writeflag = 0;
  while (k--) 
    inStream.push(inStream.forprec->pop());
  DbgInfo5("backi");
}

void ybegin(void) {
  writeflag = 0;
  DbgInfo0("begin");
}

void yclear(void) {
  writeflag = 0;
  lvarr[pftmp->argi] = 0;
  DbgInfo1234(LVN, "clear");
}

void ydiv(void) {
  long n = pftmp->argi2 < 0 ? zindec2(pftmp->string, pftmp->stringlength) :
    zindec(&sarrx[pftmp->argi2]);
  writeflag = 0;
  if (n == 0)
    throw xccErr(51);
  adjustnumber(zindec(&sarrx[pftmp->argi])/n, sarrx + pftmp->argi);
  DbgInfo6("div");
}

void ydoa(void) {
  writeflag = 0;
  pstack->push(0, pftmp->next);
  faux.next = marr[pftmp->argi];
  pftmp = &faux;
  DbgInfo1234(MN, "do");
}

void ydup(void) {
  int k = pattern.len;
  writeflag = 0;
  while (k > 0) 
    curoutx->push(pattern[--k]);
  DbgInfo0("dup");
}

void yelsea(void) {
  writeflag = 0;
  pftmp = pftmp->label1;
  DbgInfo0("else");
}

void yend(void) {
  writeflag = 0;
  DbgInfo0("end");
}

void yendif(void) {
  writeflag = 0;
  DbgInfo0("endif");
}

void yends(void) {
  writeflag = 0;
  curoutx = soutx;
  DbgInfo0("endstore");
}

void yendfa(void) { /*endfile*/
  pftmp = &faux;
  pftmp->next = 0;
  while (pstack->ptop != 0)
    pstack->pop();
  inStream.FinishFlag = inStream.xFinishFlag = 1;
  DbgInfo0("endfile");
}

void yexcl(void) {
  writeflag = 0;
  garr[pftmp->argi] = 0;
  DbgInfo1234(GN, "excl");
}

void yfwd(void) {
  int k = pftmp->argi;
  writeflag = 0;
  while (k--)
    curoutx->push(inStream.pop());
  DbgInfo5("fwd");
}

void yifneq(void) {
  writeflag = 0;
  DbgInfo6("ifneq");
  if (pftmp->argi2 < 0) {
    unsigned int l = pftmp->stringlength, k = 0;
    if (l == sarrx[pftmp->argi].len) {
      while (k < l)
        if (pftmp->string[l - k - 1] != sarrx[pftmp->argi][k])
          break;
        else 
          k++;
      if (k != l)
        return;
    }
    else
      return;
  }
  else {
    OutStream *p = &sarrx[pftmp->argi2];
    unsigned int l = p->len, k = 0;
    if (l == sarrx[pftmp->argi].len) {
      while (k < l)
        if ((*p)[k] != sarrx[pftmp->argi][k])
          break;
        else 
          k++;
      if (k != l)
        return;
    }
    else
      return;
  }
  if (pftmp->label1 == 0)
    pftmp = pftmp->label2;
  else
    pftmp = pftmp->label1;
}

void yifeq(void) {
  writeflag = 0;
  DbgInfo6("ifeq");
  if (pftmp->argi2 < 0) {
    unsigned int l = pftmp->stringlength, k = 0;
    if (l == sarrx[pftmp->argi].len) {
      while (k < l)
       if (pftmp->string[l - k - 1] != sarrx[pftmp->argi][k])
          break;
        else 
          k++;
      if (k == l)
        return;
    }
  }
  else {
    OutStream *p = &sarrx[pftmp->argi2];
    unsigned int l = p->len, k = 0;
    if (l == sarrx[pftmp->argi].len) {
      while (k < l)
       if ((*p)[k] != sarrx[pftmp->argi][k])
          break;
        else 
          k++;
      if (k == l)
        return;
    }
  }
  if (pftmp->label1 == 0)
    pftmp = pftmp->label2;
  else
    pftmp = pftmp->label1;
}

void yifgt(void) {
  writeflag = 0;
  DbgInfo6("ifgt");
  if (pftmp->argi2 < 0) {
    unsigned char *s = pftmp->string;
    unsigned int l = pftmp->stringlength, k = 0;
    if (l == sarrx[pftmp->argi].len) {
      while (k < l) {
        if (s[k] > sarrx[pftmp->argi][l - k - 1])
          goto L1;
        if (s[k] < sarrx[pftmp->argi][l - k - 1])
          break;
        else 
          k++;
      }
      if (k != l)
        return;
    }
    else if (l < sarrx[pftmp->argi].len)
      return;
  }
  else {
    OutStream *p = &sarrx[pftmp->argi2];
    unsigned int l = p->len, k = l - 1;
    if (l == sarrx[pftmp->argi].len) {
      while (k >= 0) {
        if ((*p)[k] > sarrx[pftmp->argi][k])
          goto L1;
        if ((*p)[k] < sarrx[pftmp->argi][k])
          break;
        else 
          k--;
      }
      if (k >= 0)
        return;
    }
    else if (l < sarrx[pftmp->argi].len)
      return;
  }
L1:
  if (pftmp->label1 == 0)
    pftmp = pftmp->label2;
  else
    pftmp = pftmp->label1;
}

void yifn(void) {
  writeflag = 0;
  if (lvarr[pftmp->argi])
    if (pftmp->label1 == 0)
      pftmp = pftmp->label2;
    else
      pftmp = pftmp->label1;
  DbgInfo1234(LVN, "ifn");
}

void yifa(void) {
  writeflag = 0;
  if (!lvarr[pftmp->argi])
    if (pftmp->label1 == 0)
      pftmp = pftmp->label2;
    else
      pftmp = pftmp->label1;
  DbgInfo1234(LVN, "if");
}

void yincl(void) {
  writeflag = 0;
  garr[pftmp->argi] = 1;
  DbgInfo5("incl");
}

void yincr(void) {
  unsigned int k = 0;
  writeflag = 0;
  if (sarrx[pftmp->argi].len == 0)
    throw xccErr(48);
  else
    while (sarrx[pftmp->argi][k]++ == '9') {
      sarrx[pftmp->argi][k] = '0';
      if (sarrx[pftmp->argi].ptr == ++k)
        if (sarrx[pftmp->argi].len != sarrx[pftmp->argi].ptr)
          throw xccErr(42);
        else 
          break;
    }
  DbgInfo1234(SN, "incr");
}

void ydecr(void) {
  unsigned int k = 0;
  writeflag = 0;
  if (sarrx[pftmp->argi].len == 0)
    throw xccErr(48);
  else
    while (sarrx[pftmp->argi][k]-- == '0') {
      sarrx[pftmp->argi][k] = '9';
      if (sarrx[pftmp->argi].len == ++k)
        if (sarrx[pftmp->argi].len != sarrx[pftmp->argi].ptr)
          throw xccErr(43);
        else
          break;
    }
  DbgInfo1234(SN, "decr");
}

void ymod(void) {
  long n = pftmp->argi2 < 0 ? zindec2(pftmp->string, pftmp->stringlength) :
    zindec(&sarrx[pftmp->argi2]);
  writeflag = 0;
  if (n == 0)
    throw xccErr(51);
  adjustnumber(zindec(&sarrx[pftmp->argi])%n, sarrx + pftmp->argi);
  DbgInfo6("mod");
}

void ymul(void) {
  writeflag = 0;
  adjustnumber(zindec(&sarrx[pftmp->argi])*
    (pftmp->argi2 < 0 ? 
      zindec2(pftmp->string, pftmp->stringlength) :
      zindec(&sarrx[pftmp->argi2])),
    sarrx + pftmp->argi);
  DbgInfo6("mul");
}

void ynext(void) {
  writeflag = 0;
  if ((pcur = pcur->next) != 0) {
    pftmp = &faux;
    pftmp->next = pcur->a1;
  }
  else
    throw xccErr(45);
  DbgInfo0("next");
}

void ynot(void) {
  writeflag = 0;
  lvarr[pftmp->argi] ^= 1;
  DbgInfo1234(LVN, "not");
}

void yomit(void) {
  int k = pftmp->argi;
  writeflag = 0;
  while (k--) 
    inStream.pop();
  DbgInfo5("omit");
}

void youts(void) {
  int k = sarrx[pftmp->argi].len, l = k - 1;
  writeflag = 0;
  while (k--) 
    if (curoutx == sarrx + pftmp->argi)
      curoutx->push(sarrx[pftmp->argi][l]);
    else
      curoutx->push(sarrx[pftmp->argi][k]);
  DbgInfo1234(SN, "outs");
}

void yout(void) {
  int k = sarrx[pftmp->argi].len, l = k - 1;
  curoutx = soutx;
  writeflag = 0;
  while (k--) 
    if (curoutx == sarrx + pftmp->argi)
      curoutx->push(sarrx[pftmp->argi][l]);
    else
      curoutx->push(sarrx[pftmp->argi][k]);
  DbgInfo1234(SN, "out");
}

void yread(void) {
  writeflag = 0;
  fgets(tmpstore, TMPSZ, stdin);
  int k = 0, l = strlen(tmpstore) - 1;
  while (k < l)
    curoutx->push(tmpstore[k++]);
  DbgInfo0("read");
}

void yrep(void) {
  writeflag = 0;
  if (pftmp->label1 == 0) {
    pftmp = &faux;
    pftmp->next = pcur->a1;
  }
  else
    pftmp = pftmp->label1;
  DbgInfo0("repeat");
}

void yset(void) {
  writeflag = 0;
  lvarr[pftmp->argi] = 1;
  DbgInfo1234(LVN, "set");
}

void ystore(void) {
  writeflag = 0;
  curoutx = &sarrx[pftmp->argi];
  curoutx->reset();
  DbgInfo1234(SN, "store");
}

void ysub(void){
  writeflag = 0;
  adjustnumber(zindec(&sarrx[pftmp->argi]) - 
      (pftmp->argi2 < 0 ? 
        zindec2(pftmp->string, pftmp->stringlength) :
        zindec(&sarrx[pftmp->argi2])),
      sarrx + pftmp->argi);
  DbgInfo6("sub");
}

void ysymdup(void) {
  int n = pattern.len - pftmp->argi - 1;
  writeflag = 0;
  if (n < 0)
    throw xccErr(56);
  curoutx->push(pattern[n]);
  DbgInfo5("symdup");
}

void yuse(void) {
  writeflag = 0;
  for(int k = 0; k < ArgElem::argid[GN]; k++)
    garr[k] = 0;
  garr[pftmp->argi] = 1;
  DbgInfo1234(GN, "use");
}

void ywrite(void) {
  writeflag = 1;
  DbgInfo0("write");
}

void ywrstore(void) {
  int k = sarrx[pftmp->argi].len;
  writeflag = 0;
  while (k--)
    printf("%c", sarrx[pftmp->argi][k]);
  DbgInfo1234(GN, "wrstore");
}

void yunsort(void) {
  DbgInfo0("unsort");
}

int execs(void) {
  if (logfile)
    logPattern[0] = '\0';
  pftmp = pcur->s1;
  while (pftmp != 0)
    if (pftmp->pfi())
      pftmp = pftmp->next;
    else
      return 0;
  return 1;
}

void execa(void) {
  pftmp = pcur->a1;
  if (logfile) {
    int i = pattern.len;
    while (i > 0)
      prchHTML(pattern[--i]);
    if (logPatternCur == 0)
      fprintf(logfile, "<em>'' >");
    else
      fprintf(logfile, "<em>%s >", toHTML(logPatternCur + 1));
  }
  while (1) {
    while (pftmp != 0) {
      pftmp->pfv();
      pftmp = pftmp->next;
    }
    if (pstack->ptop != 0) {
      pftmp = pstack->ptop->label;
      pstack->pop();
    }
    else
      break;
  }
  if (logfile)
    fprintf(logfile, "</em>");
}

void execcct(void) {
  Operator *savepcur;
  int savepatlen;
  unsigned long maxorder = 0;
  pcur = pfirst;
  writeflag = 0;
  if (sorted) {
    while (pcur != 0) {
      oporder = Operator::qop - pcur->order;
      patlen = 0;
      if (garr[pcur->group] && execs() && oporder >= maxorder) {
        if (logfile)
          strcpy(logPatternCur, logPattern);
        savepcur = pcur;
        savepatlen = patlen;
        maxorder = oporder;
      }
      pcur = pcur->next;
    }
    if (maxorder) {
      pcur = savepcur;
      patlen = savepatlen;
      fillpat();
      execa();
      delpat();
      return;
    }
  }
  else
    while (pcur != 0) {
      patlen = 0;
      if (garr[pcur->group] && execs()) {
        if (logfile)
          strcpy(logPatternCur, logPattern);
	fillpat();		
        execa();
        delpat();
        return;
      }
      else
        pcur = pcur->next;
    }
  if (logfile)
    prchHTML(inStream[0]);
  curoutx->push(inStream.pop());
}

void CC(void) {
  if (pbegin != 0) {
    patlen = 0;
    pcur = pbegin;
    if (logfile)
      strcpy(logPatternCur, " begin");
    execa();
  }
  while (!inStream.end(0))
    execcct();
  if (pendfile != 0) {
    patlen = 0;
    pcur = pendfile;
    if (logfile)
      strcpy(logPatternCur, " endfile");
    execa();
  }
}
