#include "i.h"

#ifdef USE_GLIB_MALLOC
//__attribute__ ((malloc))
void *
salloc(gsize n) {
	void *m;
	m = g_malloc(n);
	memset(m, 0, n);
	return m;
}
#endif // USE_GLIB_MALLOC

//#ifdef OLD
#ifndef USE_GLIB_MALLOC
//__attribute__ ((malloc))
void *
salloc(size_t n) {
	void *m;
	if ((m = malloc(n)) == NULL)
		die("out of memory\n");
	memset(m, 0, n);
	return m;
}
#endif // USE_GLIB_MALLOC

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
//#endif
