dvr: main
	g++ main.o -o dvr -lpthread -std=c++11

main: main.cpp
	g++ -c main.cpp -lpthread -std=c++1
clean:
	$(RM) dvr main.o