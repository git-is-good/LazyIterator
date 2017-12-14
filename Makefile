all:
	clang++ -std=c++17 LazyIterator.cc

clean:
	rm -rf *~ a.out
