all:
	clang++ -std=c++17 -O2 LazyIterator.cc 

clean:
	rm -rf *~ a.out
