CFLAGS	= -O
OBJS	= main.o esc.o

all: vttest quote

vttest:	$(OBJS)
	cc $(CFLAGS) -o vttest $(OBJS)

install: vttest
	cp vttest.1 /usr/man/man1/vttest.1
	cp vttest /usr/local/bin/vttest
	strip /usr/local/bin/vttest

clean:
	rm -f $(OBJS) vttest foo core lint tags a.out
