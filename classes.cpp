#include "data.h"
extern char **gargv, infnlist[];

int InStream::end(int i) {
  if (!FinishFlag && forback->len + buflen <= curp + i)
    bufread();
  return FinishFlag && forback->len + buflen <= curp + i || xFinishFlag;
}

unsigned char InStream::operator[](unsigned int i) {
  if (forback->len > 0)
    if (forback->len > i)
      return (*forback)[i];
    else
      i -= forback->len;
  if (i >= IBUFSZ)
    throw xccErr(60);
  while (curp + i >= buflen && !FinishFlag) 
    bufread();
  if (curp + i < buflen)
    return ibuf[curp + i];
  return 0; /* dummy */
}

unsigned char InStream::pop(void) {
  if (forback->len > 0)
    return forprec->push(forback->pop());
  while (curp >= buflen && !FinishFlag)
    bufread();
  if (curp < buflen) 
    return forprec->push(ibuf[curp++]);
  return 0; /* dummy */
}

void InStream::push(unsigned char c) {
  forback->push(c);
}

void InStream::bufread(void) {
  int n = buflen/2, m = 0;
  memmove(ibuf, ibuf + n, buflen - n);
  if (Nfn == 0 || feof(infile)) 
    if (xopen()) 
      m = fread(ibuf + buflen - n, 1, IBUFSZ - buflen + n, infile);
    else 
      FinishFlag = 1;
  else
    m = fread(ibuf + buflen - n, 1, IBUFSZ - buflen + n, infile);
  curp -= n;
  buflen += m - n;
}

int InStream::xopen(void) {
  char *tstr = gargv[infnlist[Nfn]];
  if (Nfn > 0 && infnlist[Nfn] == '\0')
    return 0;
  if (infnlist[0] == '\0' || !strcmp(tstr, "-")) {
    if (Nfn != 0 && infile != stdin)
      fclose(infile);
    infile = stdin;
  }
  else {
    if (Nfn != 0 && infile != stdin)
      fclose(infile);
    if ((infile = fopen(tstr, "rb")) == 0)
      throw xccErr(2, (long) tstr);
  }
  Nfn++;
  return 1;
}

unsigned char OutStream::push(unsigned char c) {
  obuf[ptr++] = c;
  len++;
  if (ptr == OBUFSZ) {
    if (otmpfile == 0)
      if ((otmpfile = tmpfile()) == 0)
        throw xccErr(54);
    if (fwrite(obuf, 1, OBUFSZ, otmpfile) != OBUFSZ)
      throw xccErr(55);
    ptr = 0;
  }
  return c;
}

unsigned char OutStream::pop(void) {
  if (ptr == 0)
    if (len == 0)
      throw xccErr(40);
    else {
      if (fseek(otmpfile, -OBUFSZ, SEEK_CUR) != 0)
        throw xccErr(57);
      if (fread(obuf, 1, OBUFSZ, otmpfile) != OBUFSZ)
        throw xccErr(58);
      if (fseek(otmpfile, -OBUFSZ, SEEK_CUR) != 0)
        throw xccErr(57);
      ptr = OBUFSZ;
    }
  len--;
  return obuf[--ptr];
}

unsigned char &OutStream::operator[](unsigned int i) {
  unsigned char t[OBUFSZ];
  unsigned long savefpos;
  if (i >= len)
    throw xccErr(59);
  else if (i < ptr)
    return obuf[ptr - i - 1];
  else {
    int q, r;
    i = len - i - 1;
    q = i/OBUFSZ;
    r = i%OBUFSZ;
    savefpos = ftell(otmpfile);
    if (fseek(otmpfile, q*OBUFSZ, SEEK_SET) != 0)
      throw xccErr(57);
    if (fread(t, 1, OBUFSZ, otmpfile) != OBUFSZ)
      throw xccErr(58);
    if (fseek(otmpfile, savefpos, SEEK_SET) != 0)
      throw xccErr(57);
    return t[r];
  }
}

int OutStream::search(int c) {
  for (unsigned int i = 0; i < len; i++)
    if ((*this)[i] == c)
      return 1;
  return 0;  
}

void OpStack::push(char t, FuncElem *p) {
  StackElem *ptmp = ptop;
  ptop = new StackElem;
  ptop->prev = ptmp;
  ptop->type = t;
  ptop->label = p;
}

void OpStack::pop(void) {
  StackElem *ptmp = ptop;
  ptop = ptop->prev;
  delete ptmp;
}
