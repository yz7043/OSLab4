all: nyufile.cpp util.cpp
	g++ -g -o nyufile nyufile.cpp util.cpp -lcrypto