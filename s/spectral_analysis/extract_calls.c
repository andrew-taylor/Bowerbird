#include "i.h"

#define VERSION "0.1.1"

int
main(int argc, char *argv[]) 
{
	int optind = initialize(argc, argv, VERSION, "<sound files>");
	char *output_directory = param_get_string("call", "output_directory");
	char *prefix = param_sprintf("call", "pathname_prefix", output_directory);
	g_free(output_directory);
	FILE *index_file = NULL;
	char *index_filename = param_sprintf("call", "index_filename", prefix);
	g_free(prefix);
	if (index_filename) {
		index_file = fopen(index_filename, "w");
		g_free(index_filename);
		assert(index_file);
		setbuf(index_file, NULL);
		fprintf(index_file, "<html>\n");
	}
	int call_count = 0;
	for (int i = optind; i < argc; i++)
		extract_calls_file(argv[i], index_file, &call_count);
	if (index_file) {
		fprintf(index_file, "</html>\n");
		fclose(index_file);
	}
	return 0;
}

