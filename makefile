#CXXFLAGS = -gstabs
CXXFLAGS = -O5
LDFLAGS = -lstdc++
BINDIR=/usr/local/bin

mse: mse.o lexfn.o exefn.o classes.o

mse.o: mse.cpp mse.h data.h

lexfn.o: lexfn.cpp lexfn.h data.h

exefn.o: exefn.cpp data.h

classes.o: classes.cpp data.h

clean:
	-rm mse *.o test.out test.html

install:
	install -m 755 -s mse $(BINDIR)

wc:
	wc *.cpp *.h makefile

test:
	./mse -t test.mse -o test.out -l test.html test.in test.smp
	-diff test.out test.smp
