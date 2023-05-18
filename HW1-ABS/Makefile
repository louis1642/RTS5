all: plants controller reference diag

plants: plantR plantL

plantR: plantR.o rt-lib.o
	gcc -o plantR plantR.o rt-lib.o -lrt -pthread

plantL: plantL.o rt-lib.o
	gcc -o plantL plantL.o rt-lib.o -lrt -pthread

controller: controller.o rt-lib.o diag_data.o
	gcc -o controller controller.o rt-lib.o diag_data.o -lrt -pthread

reference: reference.c parameters.h
	gcc -o reference reference.c -lrt

diag: diag.o rt-lib.o diag_data.o
	gcc -o diag diag.o rt-lib.o diag_data.o -pthread -lrt

plantR.o: plantR.c rt-lib.h parameters.h
	gcc -c plantR.c

plantL.o: plantL.c rt-lib.h parameters.h
	gcc -c plantL.c

controller.o: controller.c rt-lib.h parameters.h diag_data.h
	gcc -c controller.c

rt-lib.o: rt-lib.c rt-lib.h
	gcc -c rt-lib.c

diag.o: diag.c diag_data.h rt-lib.h parameters.h
	gcc -c diag.c

diag_data.o: diag_data.h diag_data.c parameters.h
	gcc -c diag_data.c

clean:
	rm *.o plantR plantL controller reference diag
