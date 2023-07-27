all: tables.h gentable cparse

cparse:
	g++ --std=c++11 cparse.cpp -o cparse

gentable:
	g++ --std=c++11 gentable.cpp Follows.cpp Grammar.cpp Item.cpp LRSet.cpp Production.cpp State.cpp TableGenerator.cpp -o gentable

tables.h: gentable items.txt
	./gentable < items.txt > tables.h
clean:
	rm -f tables.h

