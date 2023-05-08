all: monitor rt_ps rt_async

monitor: monitor.c
	gcc -o monitor monitor.c -lrt
	
rt_ps: rt_ps.c
	gcc -o rt_ps rt_ps.c -lpthread -lrt
	
rt_async: rt_async.c
	gcc -o rt_async rt_async.c -lrt
	
clean:
	rm monitor rt_ps rt_async
