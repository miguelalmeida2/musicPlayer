all: ex1 ex2

ex1:
	gcc ex1.c ./utils/dlist.c -L./wavelib -lwave -lasound -o ex1 -g -Wall

debugex1:
	insight ./ex1 -args wave_capture1.wav

ex2:
	gcc ex2.c -L./utils -lutils -ldlist -L./wavelib -lwave -lasound -lpthread -o ex2 -g -Wall

debugex2:
	insight ./ex2 .

clean:
	rm -rf *.o *.i
