#include <cstdio>
#include <cstring>
#include <new>

using namespace std;

//#define PCDOS
#define MAXLONG16 134217727L
#define MAXLONG10 214748363L
#define MAXLONG8 268435455L
#define TMPSZ 2048
#define OBUFSZ 32768 /*32768*/
#define IBUFSZ 32768
#define QINFILE 256

struct OutStream {
  FILE *otmpfile;
  unsigned char obuf[OBUFSZ];
  unsigned long ptr, len;
  OutStream() {
    otmpfile = 0;
    len = ptr = 0;
  }
  ~OutStream() {
    if (otmpfile != 0)
      fclose(otmpfile);
  }
  void reset(void) {
    if (otmpfile != 0)
      fseek(otmpfile, 0, SEEK_SET);
    len = ptr = 0;
  }
  unsigned char push(unsigned char);
  unsigned char pop(void);
  unsigned char &operator[](unsigned int);
  int search(int);
};

struct InStream {
  FILE *infile;
  OutStream *forback, *forprec;
  unsigned char ibuf[IBUFSZ], Nfn, FinishFlag, xFinishFlag;
  unsigned long buflen, curp;
  InStream() {
    infile = 0;
    curp = buflen = Nfn = FinishFlag = xFinishFlag = 0;
    forback = new OutStream;
    forprec = new OutStream;
  }
  ~InStream() {
    delete forback;
    delete forprec;
    if (infile != stdin && infile != 0) 
      fclose(infile);
  }
  unsigned char operator[](unsigned int);
  unsigned char pop(void);
  void push(unsigned char);
  int end(int);
private:
  int xopen(void);
  void bufread(void);
};

struct FuncElem {
  union {
    int (*pfi)(void);
    void (*pfv)(void);
  };
  short argi /*length of literal in substitute, # of arg1name in action*/, 
    argi2 /*# (-1 for literal) of arg1name in action for opp6
    # in rule for fol (count from 0) or prec (count from # of precs)*/;
  int stringlength;
  unsigned char *string;
  FuncElem *label1, *label2, *next;
  ~FuncElem() {
    delete [] string;
  }
  FuncElem() {
    stringlength = argi2 = 0;
    string = 0;
    label1 = label2 = next = 0;
  }
};

enum ArgMode {SN, LVN, GN, MN, Num};

struct ArgElem {
  ArgElem *next;
  char *name; 
  ArgMode type;
  static short argid[4];
  short id;
  FuncElem *pf1;
  ArgElem(ArgMode argmode) {
    next = 0;
    pf1 = 0;
    type = argmode; /* 0-SN, 1-LVN, 2-GN, 3-MN, 4-Num */
    id = argid[argmode]++;
  }
};

struct Operator {
  short group, order;
  Operator *next;
  FuncElem *s1, *a1;
  static int qop, curgroup;
  Operator() {
    next = 0;
    a1 = s1 = 0;
    order = qop++;
    group = curgroup;
  }
};

struct StackElem {
  char type; /* 0-begin, 2-if */
  FuncElem *label;
  StackElem *prev;
};

struct OpStack {
  StackElem *ptop;
  void push(char, FuncElem *);
  void pop(void);
  OpStack() {
    ptop = 0;
  }
  ~OpStack() {
    while (ptop != 0)
      pop();
  }
};

struct xccErr {
  long n;
  union {
    long line;
    char *errstr;
  };
  xccErr(int i) {n = i; line = 0;}
  xccErr(int i, int l) {n = i; line = l;}
};
