CC = cc
CFLAGS = -static `pkg-config --static --cflags kcgi --libs kcgi kcgi-html` -O2 
#LDFLAGS = `pkg-config --static --libs kcgi kcgi-html`

EXEC = system

OBJS = system.o

all: $(EXEC)

clean:
	rm -f $(EXEC)

strip: all
	strip -s $(EXEC)

install: all strip
	install etc/doas.conf /etc
	install etc/httpd.conf /etc
	install etc/inetd.conf /etc
	install rsysd /usr/local/sbin
	install -o www -g www -m 0700 -d /var/www/tpl
	install -o www -g www -m 0500 tpl/system.xml /var/www/tpl
	install -o www -g www -m 0500 ${EXEC} /var/www/cgi-bin

