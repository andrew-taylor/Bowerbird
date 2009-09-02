#include "i.h"
#include <errno.h>

char *myname;

void
set_myname(char *argv[]) {
	myname = strrchr(argv[0], '/');
	if (myname == NULL)
		myname = argv[0];
	else
		myname++;
}

//__attribute__ ((noreturn)) 
void
die(char *format, ...) {
	va_list ap;
	if (myname)
		fprintf(stderr, "%s: ", myname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	if (errno == 0)
		fprintf(stderr, "\n");
	else
		perror(": ");
	if (verbosity > 9)
		abort();
	exit(1);
}
 
int		verbosity = 0;
FILE	*debug_stream;

//__attribute__ ((format (printf, 2, 3)))
void
dprintf(int level, char *format, ...) {
	va_list ap;
	if (level > verbosity)
		return;
	va_start(ap, format);
	if (debug_stream == NULL)
		vfprintf(stderr, format, ap);
	else
		vfprintf(debug_stream, format, ap);
}

//__attribute__ ((malloc))
void *
salloc(size_t n) {
	void *m;
	if ((m = malloc(n)) == NULL)
		die("out of memory\n");
	memset(m, 0, n);
	return m;
}

//__attribute__ ((malloc))
void *
srealloc(void *m, size_t n) {
	if ((m = realloc(m, n)) == NULL)
		die("out of memory\n");
	return m;
}

//__attribute__ ((malloc))
char *
sstrdup(char *s) {
	void *new;
	if (s == NULL)
		return NULL;
	// strdup isn't ISO
	if ((new = sdup(s, strlen(s) +1)) == NULL)
		die("out of memory\n");
	return new;
}

//__attribute__ ((malloc))
void *
sdup(void *old, size_t n) {
	void *new;
	if (old == NULL || n == 0)
		return NULL;
	new = salloc(n);
	memcpy(new, old, n);
	return new;
}
