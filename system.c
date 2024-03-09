#include <sys/types.h> /* size_t, ssize_t */
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h> /* va_list */
#include <stddef.h> /* NULL */
#include <stdint.h> /* int64_t */
#include <stdlib.h>
#include <string.h> /* memset */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>


#include "kcgi.h"
#include "kcgihtml.h"

/* Recognised page requests.  See pages[]. */
enum	page {
	PAGE_INDEX,
	PAGE_HOSTNAME,
	PAGE__MAX
};

/*
 * All of the keys (input field names) we accept. 
 * The key names are in the "keys" array.
 * See sendindex() for how these are used.
 */
enum	key {
	KEY_HOSTNAME,
	KEY__MAX
};

/*
 * The elements in our template file.
 * The element key names are in the "templs" array.
 * See sendtemplate() for how this is used.
 */

enum	hostname_tpl {
	TPL_TITLE,
	TPL_HEADER,
	TPL_HOSTNAME,
	TPL_NOTIFY,
	TPL__MAX
};

/*
 * We need a structure because we can't get the "r" from the request.
 * This is used by our template callback.
 */
struct	tstrct {
	struct khtmlreq	 req;
	struct kreq	*r;
};

/*
 * We'll use this to route pages by creating an array indexed by our
 * page.
 * Then when the page is parsed, we'll route directly into it.
 */
typedef	void (*disp)(struct kreq *);

static void send_index(struct kreq *);
static void send_hostname(struct kreq *);

static const disp disps[PAGE__MAX] = {
	send_index, /* PAGE_INDEX */
	send_hostname, /* PAGE_HOSTNAME */
};

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_string, "hostname" }, /* KEY_HOSTNAME */
	//{ NULL, "file" }, /* KEY_FILE */
	//{ kvalid_uint, "count" }, /* KEY_PAGECOUNT */
	//{ kvalid_uint, "size" }, /* KEY_PAGESIZE */
};
/*
 * Template key names (as in @@TITLE@@ in the file).
 */
static const char *const templs[TPL__MAX] = {
	"title", /* TPL_TITLE */
	"header", /* TPL_HEADER */
	"hostname", /* TPL_HOSTNAME */
	"notify", /* TPL_NOTOFY */
};

/* 
 * Page names (as in the URL component) mapped from the first name part
 * of requests, e.g., /sample.cgi/index.html -> index -> PAGE_INDEX.
 */
static const char *const pages[PAGE__MAX] = {
	"index", /* PAGE_INDEX */
	"hostname" /* PAGE_HOSTNAME */
};

/*
 * Open an HTTP response with a status code and a particular
 * content-type, then open the HTTP content body.
 * You can call khttp_head(3) before this: CGI doesn't dictate any
 * particular header order.
 */
static void
resp_open(struct kreq *req, enum khttp http)
{
	enum kmime	 mime;

	/*
	 * If we've been sent an unknown suffix like '.foo', we won't
	 * know what it is.
	 * Default to an octet-stream response.
	 */
	if (KMIME__MAX == (mime = req->mime))
		mime = KMIME_APP_OCTET_STREAM;

	khttp_head(req, kresps[KRESP_STATUS], 
		"%s", khttps[http]);
	khttp_head(req, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[mime]);
	khttp_body(req);
}

#define SOCK_PATH "/run/rsysd.sock"
#define BUF_SIZE 256

char
*trim(const char *str)
{
  const char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0) { // All spaces?
	char* out = malloc(1);
	*out = '\0';
    return out;
  }

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  const size_t size = end-str+2;
  char* out = malloc(size);
  strncpy(out, str, size-1);
  out[size-1] = '\0';
  return out;
}

int
call_rsysd(const char* command, char* answer, size_t ans_size) {

    if(command == 0 || ans_size < 30) return -1;
//TODO: проверить правильность строки command
    int sockfd;
    struct sockaddr_un server_addr;

    // Создаем сокет
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	strcpy(answer, "create socket error");
        return -1;
    }

    // Таймауты
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    int opt_ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    opt_ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    // Настраиваем адрес сервера
    server_addr.sun_family = AF_UNIX;
    strlcpy(server_addr.sun_path, SOCK_PATH, sizeof(SOCK_PATH));

    // Присоединяемся к серверу
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
	strcpy(answer, "connect socket error");
	close(sockfd);
        return -1;
    }

    // Отправляем сообщение на сервер
    if (send(sockfd, command, strlen(command), 0) == -1) {
	strcpy(answer, "send socket error");
	close(sockfd);
        return -1;
    }

    // Читаем ответ от сервера
    int bytes_received = recv(sockfd, answer, ans_size-1, 0);

    if (bytes_received > 0) {
        answer[bytes_received] = '\0';
    } else if (bytes_received == 0) {
	strcpy(answer, "empty or socket close");
    } else {
	strcpy(answer, "received socket error");
	close(sockfd);
        return -1;
    }

    // Закрываем соединение
    close(sockfd);

    return 0;
}


char buf[BUF_SIZE];
char
*_get_hostname(void) {
    int sockfd;
    struct sockaddr_un server_addr;

    // Создаем сокет
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Настраиваем адрес сервера
    server_addr.sun_family = AF_UNIX;
    strlcpy(server_addr.sun_path, SOCK_PATH, sizeof(SOCK_PATH));

    // Присоединяемся к серверу
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        exit(1);
    }

    // Отправляем сообщение на сервер
    char* message = "get_hostname\n";
    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
        exit(1);
    }

    // Читаем ответ от сервера
    int bytes_received = recv(sockfd, buf, BUF_SIZE-1, 0);

    if (bytes_received > 0) {
        buf[bytes_received] = '\0';
        //printf("Received message: %s\n", trim(buf));
    } else if (bytes_received == 0) {
        printf("Server disconnected\n");
    } else {
        perror("recv");
        exit(1);
    }

    // Закрываем соединение
    close(sockfd);

    return buf;
}

char*
get_hostname(void) {
	char* answer = malloc(250);
	int error = call_rsysd("get_hostname\n", answer, 250);
	char* trim_answer = trim(answer);
	free(answer); 
	if(answer[0] == '\0') return "ERROR!";
	if(error != 0) {
		char* err_ans = malloc(300);
		strcpy(err_ans, "ERROR: ");
		strlcat(err_ans, trim_answer, 300);
		free(trim_answer);
		return err_ans;
	}
	return trim_answer;
}

char*
set_hostname(char* hostname) {
	char* answer = malloc(300);
	char* command = malloc(300);
	//trim?
	snprintf(command, 300, "set_hostname %s\n", hostname);
	//strcpy(command, "set_hostname");
	//strlcat(command, hostname, 300);

	int error = call_rsysd(command, answer, 300);
	char* trim_answer = trim(answer);
	free(answer); 
	if(answer[0] == '\0') return "ERROR!";
	if(error != 0) {
		char* err_ans = malloc(300);
		strcpy(err_ans, "ERROR: ");
		strlcat(err_ans, trim_answer, 300);
		free(trim_answer);
		return err_ans;
	}
	return trim_answer;
}


/*
 * Callback for filling in a particular template part.
 * Let's just be simple for simplicity's sake.
 */

static int
hostname(size_t key, void *arg)
{
	struct tstrct	*p = arg;
	char* answer = 0;
	struct kpair *pa;
	char* notify = "";
	if((pa = p->r->fieldmap[KEY_HOSTNAME])) {
		notify = set_hostname(pa->val);
	}
	switch (key) {
	case (TPL_TITLE):
		khtml_puts(&p->req, "System - hostname");
		break;
	case (TPL_HEADER):
		khtml_puts(&p->req, "Hostname settings");
		break;
	case (TPL_HOSTNAME):
		answer = get_hostname();
		khtml_puts(&p->req, answer);
		break;
	case (TPL_NOTIFY):
		khtml_puts(&p->req, notify);
		break;
	default:
		return(0);
	}
//	if(answer != 0) free(answer);
	return(1);
}

/*
 * Demonstrates how to use templates.
 * Returns HTTP 200 and the template content.
 */
static void
send_hostname(struct kreq *req)
{
	struct ktemplate t;
	struct tstrct	 p;

	memset(&t, 0, sizeof(struct ktemplate));
	memset(&p, 0, sizeof(struct tstrct));

	p.r = req;
	t.key = templs;
	t.keysz = TPL__MAX;
	t.arg = &p;
	t.cb = hostname;

	resp_open(req, KHTTP_200);
	khtml_open(&p.req, req, 0);
	khttp_template(req, &t, "../tpl/system.xml");
	khtml_close(&p.req);
}


/*
 * Demonstrates how to use GET and POST forms and building with the HTML
 * builder functions.
 * Returns HTTP 200 and HTML content.
 */
static void
send_index(struct kreq *req)
{
	char		*page;
	struct khtmlreq	 r;
	const char	*cp;
	
	kasprintf(&page, "%s/%s", req->pname, pages[PAGE_INDEX]);

	resp_open(req, KHTTP_200);
	khtml_open(&r, req, 0);
	khtml_elem(&r, KELEM_DOCTYPE);
	khtml_elem(&r, KELEM_HTML);
	khtml_elem(&r, KELEM_HEAD);
	khtml_elem(&r, KELEM_TITLE);
	khtml_puts(&r, "Welcome!");
	khtml_closeelem(&r, 2);
	khtml_elem(&r, KELEM_BODY);
	khtml_puts(&r, "Welcome!");
	khtml_elem(&r, KELEM_BR);
	khtml_attrx(&r, KELEM_A, KATTR_HREF, KATTRX_STRING, "/cgi-bin/system/hostname", KATTR__MAX);
	khtml_puts(&r, "hostname");
	khtml_closeelem(&r, 1);
	khtml_closeelem(&r, 1);
	khtml_closeelem(&r, 0);
	khtml_close(&r);
	free(page);
}

int
main(void)
{
	struct kreq	 r;
	enum kcgi_err	 er;

	/* Set up our main HTTP context. */

	er = khttp_parse(&r, keys, KEY__MAX, 
		pages, PAGE__MAX, PAGE_INDEX);

	if (KCGI_OK != er)
		return(EXIT_FAILURE);

	/* 
	 * Accept only GET, POST, and OPTIONS.
	 * Restrict to text/html and a valid page.
	 * If all of our parameters are valid, use a dispatch array to
	 * send us to the page handlers.
	 */

	if (KMETHOD_OPTIONS == r.method) {
		khttp_head(&r, kresps[KRESP_ALLOW], 
			"OPTIONS GET POST");
		resp_open(&r, KHTTP_200);
	} else if (KMETHOD_GET != r.method && 
		   KMETHOD_POST != r.method) {
		resp_open(&r, KHTTP_405);
	} else if (PAGE__MAX == r.page || 
		   KMIME_TEXT_HTML != r.mime) {
		resp_open(&r, KHTTP_404);
	} else
		(*disps[r.page])(&r);

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

