all: libwave.a libwavelib.so

# Creating static library for wave that depends only on wave.o
libwave.a: wave.o wave.h
	ar rcs libwave.a wave.o

# Creating a dynamic library that depends only on wave.o
libwavelib.so: wave.o
	gcc -fPIC wave.o -shared -o libwavelib.so -Wall

wave.o: wave.c wave.h
	gcc -L./utils -lutils -ldlist -fPIC -c wave.c -g -Wall

clean:
	rm -rf *.o *.is
