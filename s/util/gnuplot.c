#include "i.h"
//
// gnuplotf provides a convenient C interface to gnuplot using a printf-like format string
//
// These variables can optionally be specified in the format string:
// width     - width of plot in pixels (default=1000)
// height    - heightof plot in pixels (default=1000)
// length    - number of data points in plot (default=1)
// columns   - columns of multiplot (default=1)
// rows      - rows of multiplot (default=1)
// title     - title of next data (default none)
// with      - plot style for next data (default none)
//
// like this:
//
//	gnuplotf("width=200 height=100 length=10 title='my title' with='points' %d", array);
//
// or equivalently
//
//	gnuplotf("width=%d height=%d length=%d title=%s with=%s %d", 200,100,10, "my title", "points", array);
//
// Outside variable assignments % specifiers indicate arrays to be plotted
//
// %d = int array, %lf = double array, %lf = double array
// %t indicates x,y data and 2 more %specifiers must follow containing the data
//
// a ';' in the format string means move to the next plot in multi-plot
// 
// whitespace is ignored in format strings
//

//	int n = 100;
//	double a[n], b[n], c[n], d[n], e[n];
//	int g[n];
//	int i;
//	for (i = 0; i < n; i++) {
//		a[i] = sin(2*M_PI*i/30);
//		b[i] = cos(2*M_PI*i/30);
//		c[i] = sin(2*M_PI*i/20);
//		d[i] = cos(2*M_PI*i/20);
//		e[i] = i;
//		g[i] = i;
//	}
//	gnuplotf("width=2000 height=1000 rows=4 length=%d %lf;%lf;%t%lf%lf; with='points' title=%s %t%d%lf",  n, a, b, e, a, "using points", g, b);

void
gnuplotf(char *format, ...) {
	int multiplot_columns = 1, multiplot_rows = 1;
	int pixels_width = 1000, pixels_height = 1000; 
	int array_length = 1;
	int column = 0, row = 0;
	int n_skip_headers = 0;
	char format_copy[strlen(format)+20];
	memset(format_copy, 0, sizeof format_copy);
	int max_args = 2*strlen(format);
	struct saved_array {
		int type;
		void *a;
		int length;
	} saved_arrays[max_args];
	int n_saved_arrays = 0;
	strcpy(format_copy, format);
	char *title_string = "", *with_string = "lines";
	FILE *f = NULL;
	va_list ap;
	va_start(ap, format);
	for (char *format_p = format_copy; ; format_p++) {
		char format_char = *format_p;
		dp(11, "format_p=%p format_char='%d' n_saved_arrays='%d'\n", format_p, format_char, n_saved_arrays);
		if (isspace(format_char))
			continue;
		if (isalpha(format_char)) {
			char *name = format_p;
			char *s = strchr(format_p, '=');
			if (s == NULL) die("gnuplotf: expected '=' in format");
			*s = '\0';
			format_p = s + 1;
			struct {char *n; void *p; int type;} t[] = {{"width",&pixels_width,'i'},{"height",&pixels_height,'i'},{"length",&array_length,'i'},{"columns",&multiplot_columns,'i'},{"rows",&multiplot_rows,'i'},{"title",&title_string,'s'},{"with",&with_string,'s'}, {0}};
			int v;
			for (v = 0; t[v].n && strncmp(name, t[v].n, strlen(name)); v++)
				;
			if (!t[v].n) die("gnuplotf: unexpected name '%s' in format", name);
			if (*format_p == '%') {
				format_p++;
				switch (*format_p) {
				case 'd':
					*((int *)t[v].p) = va_arg(ap, int); break;
				case 's':
					*((char **)t[v].p) = va_arg(ap, char *); break;
				default:
					die("Unimplemented character in gnuplotf %% specification '%c'", *format_p);
				}
				dp(11, "length=%d t[v].p=%p &array_length=%p\n", array_length, t[v].p, &array_length);
			} else if (*format_p == '\'') {
				char *p = strchr(format_p + 1, '\'');
				if (p == NULL)
					die("Unmatched quote in gnuplotf format");
				*p = '\0';
				*((char **)t[v].p) = format_p + 1;
				format_p = p;
				dp(11, "title_string='%s' t[v].p=%p &title_string=%p\n", title_string, t[v].p, &title_string);
			} else if (*format_p == '-' || *format_p == '+' || isdigit(*format_p)) {
				*((int *)t[v].p) = strtod(format_p, &format_p);
				format_p--;
				dp(11, "pixels_width=%d t[v].p=%p &pixels_width=%p\n", pixels_width, t[v].p, &pixels_width);
			} else 
				die("Unknown character in gnuplotf format '%c'", *format_p);
			continue;
		}
		if (f == NULL) {
			f = gnuplot_open(pixels_width, pixels_height);
			gnuplot(f, "set multiplot;set size %f,%f;", 1.0/multiplot_columns, 1.0/multiplot_rows);
		}
		if (format_char == '\'') {
			char *p = strchr(format_p + 1, '\'');
			if (p == NULL)
				die("Unmatched quote in gnuplotf format");
			*p = '\0';
			gnuplot(f, format_p + 1);
			gnuplot(f, "\n");
			dp(11, "'%s'\n", format_p + 1);
			format_p = p;
			continue;
		}
		if (format_char == '\0' || format_char == ';') {
			gnuplot(f, "\n");
			for (int array = 0; array < n_saved_arrays; array++) {
				int length = saved_arrays[array].length;
				int dimension = 1;
				if (saved_arrays[array].type == 't') { // 2d points
					dimension = 2;
					array++;
				}
//				fprintf(stderr, "dimension=%d array=%d length='%d' type='%c' a=%p\n", dimension, array, length, saved_arrays[array].type,  saved_arrays[array].a);
				for (int i = 0; i < length; i++) {
					for (int j = 0; j < dimension; j++) {
						struct saved_array *a = &saved_arrays[array+j];
						switch (a->type) {
						case 'f':
						case 'l':
						{
							double d = a->type == 'f' ? ((float *)a->a)[i] : ((double *)a->a)[i];
							assert(isfinite(d));
							gnuplot(f, "%g", d);
							break;
						}
						case 'd': gnuplot(f, "%d", ((int *)a->a)[i]); break;
						default: die("Unknown array type '%c'", a->type);
						}
						gnuplot(f, " ");
				}
					gnuplot(f, "\n");
				}
				gnuplot(f, "e\n");
				array += dimension - 1;
			}
			n_saved_arrays = 0;
			if (format_char == '\0') break;
			if (++row == multiplot_rows) {
				row = 0;
				column++;
			}
			continue;
		}
		if (n_skip_headers) {
			n_skip_headers--;
		} else {
			if (n_saved_arrays == 0) {
				gnuplot(f, "set origin %f,%f;", (multiplot_columns-1.0-column)/multiplot_columns, (multiplot_rows-1.0-row)/multiplot_rows);
				gnuplot(f, "plot \"-\"");
			} else
				gnuplot(f, ", \"-\"");
			if (title_string) gnuplot(f, " title \"%s\"", title_string);
			if (with_string && strlen(with_string)) gnuplot(f, " with %s", with_string);
		}
		
		saved_arrays[n_saved_arrays].length = array_length;
		saved_arrays[n_saved_arrays].type = format_p[1];
		if (!strncmp(format_p, "%t", 2)) {
			n_skip_headers = 2;
		} else if (!strncmp(format_p, "%d", 2)) {
			saved_arrays[n_saved_arrays].a = va_arg(ap, int *);
		} else if (!strncmp(format_p, "%f", 2)) {
			saved_arrays[n_saved_arrays].a = va_arg(ap, float *);
		} else if (!strncmp(format_p, "%lf", 3)) {
			saved_arrays[n_saved_arrays].a = va_arg(ap, double *);
			format_p++;
		} else 
			die("Unexpected character in gnuplotf format ");
		format_p++;
		n_saved_arrays++;
	}
 	gnuplot_close(f);
	va_end(ap);
}

// popen/pclose is not c99
extern FILE *popen(const char *command, const char *type); 
extern int pclose(FILE *stream);

FILE *
gnuplot_open(int width, int height) {
	char buffer[1000];
	FILE *f;
	sprintf(buffer, "gnuplot -persist -geometry %dx%d", width, height);
	f = popen(buffer, "w");
	if (f == NULL)
		die("Can not popen gnuplot");
	setbuf(f, NULL);
	return f;
}

void
gnuplot(FILE *f, char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(f, format, ap);
//	vfprintf(stdout, format, ap);
}

void
gnuplot_close(FILE *f) {
//	dp(11, "pclose\n");
//    pclose(f);
//	dp(11, "pclose returns\n");
}
