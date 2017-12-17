all:
	clang++ -std=c++17 -O2 LazyIterator.cc 

parser_test: nothing
	clang++ -o $@ -g -O0 -std=c++14 parser_test.cc

clean:
	rm -rf *~ a.out *.dSYM parser_test

nothing: 
