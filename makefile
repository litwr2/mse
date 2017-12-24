CXXFLAGS = -O3
LDFLAGS = -lstdc++
BINDIR=/usr/local/bin

mse: mse.o lexfn.o exefn.o classes.o

mse.o: mse.cpp mse.h data.h

lexfn.o: lexfn.cpp lexfn.h data.h

exefn.o: exefn.cpp data.h

classes.o: classes.cpp data.h

clean:
	-rm -f mse *.o test.out *.html *.log *.toc *.vr *.tp *.ky *.pg *.pdf *.cp *.aux *.fn

install:
	install -m 755 -s mse $(BINDIR)

wc:
	wc *.cpp *.h makefile

test:
	./mse -t test.mse -o test.out -l test.html test.in test.smp
	-diff test.out test.smp

doc: html pdf

html: mse.texi
	texi2html $< >mse.html

pdf: mse.texi
	texi2pdf $<
