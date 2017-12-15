all:
	clang++ -std=c++17 -O2 LazyIterator.cc 

parser:
	clang++ -g -std=c++14 parser.cc

clean:
	rm -rf *~ a.out *.dSYM
