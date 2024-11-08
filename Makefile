all: chatty server send

clean:
	rm -f server chatty send tags *.log _*

chatty:
	gcc -ggdb -Wall -pedantic -std=c99 -o chatty chatty.c
server:
	gcc -ggdb -Wall -pedantic -std=c99 -o server server.c
send:
	gcc -ggdb -Wall -pedantic -std=c99 -o send send.c
