#include "comb_filter.h"

enum {
	NHASH	= 4093,
	MULTIPLIER = 31
};


typedef struct entry {
	char			*key;
	char			*value;
	struct entry	*next;
} entry;

static entry *table[NHASH];

int hash(char *s) {
	unsigned int h = 0;
	while (*s != '\0')
		h = MULTIPLIER * h + *s++;
	return h % NHASH;
}

static entry *lookup(char *s, int create) {
	int h = hash(s);
	entry *e;
	for (e = table[h]; e != NULL; e = e->next)
		if (strcmp(s, e->key) == 0)
			return e;
	if (create) {
		e = (entry *)salloc(sizeof *e);
		e->key = sstrdup(s);
		e->next = table[h];
		table[h] = e;
	}
	return e;
}

void
set_option(char *name, char *value) {
	lookup(name, 1)->value = sstrdup(value);
	dp(35, "option %s <- %s\n", name, value ? value : "NULL");
}

void
set_option_double(char *name, double d) {
	char buffer[1024];
	sprintf(buffer, "%g", d);
	lookup(name, 1)->value = sstrdup(buffer);
	dp(35, "option %s <- %g\n", name, d);
}

void
set_option_int(char *name, int i) {
	char buffer[1024];
	sprintf(buffer, "%d", i);
	lookup(name, 1)->value = sstrdup(buffer);
	dp(35, "option %s <- %d\n", name, i);
}

void
set_option_boolean(char *name, int i) {
	if (i != 0 && i != 1)
		die("bad second argument: %d to set_option_boolean", i);
	set_option_int(name, i);
}

char *
get_option(char *name) {
	entry *e = lookup(name, 0);
	if (e) {
		dp(35, "option %s -> %s\n", name, e->value ? e->value : "NULL");
		return e->value;
	}
	die("Unknown variable: '%s'", name);
	return NULL;
}

int
defined_option(char *name) {
	return lookup(name, 0) != NULL;
}

double
get_option_double(char *name) {
	char *s = get_option(name);
	if (s == NULL)
		return 0;
	else
		return atof(s);
}

int
get_option_int(char *name) {
	char *s = get_option(name);
	if (s == NULL)
		return 0;
	else
		return round(atof(s));
}

int
get_option_boolean(char *name) {
	char *s = get_option(name);
	if (s == NULL)
		return 0;
	else if (s[0] == '\0' || atoi(s))
		return 1;
	else {
		return !strchr("fFnN0", s[0]); /* false, no or 0 */
	}
}

int
get_option_keyword(char *name, char **keyword_table) {
	int i;
	char *value = get_option(name);
	if ((i = lookup_keyword(value, keyword_table)) >= 0)
		return i;
	die("Unknown value for option %s: '%s'", name, value);
	return -1;
}


void
parse_option_assignment(char *assignment) {
	char *t, *s = sstrdup(assignment);
	for (t = s; *t != '\0'; t++)
		if (!isalnum(*t) && *t != '_')
			break;
	if (*t != '\0')	{
		*t = '\0';
		t++;
		while (*t != '\0' && isspace(*t))
			t++;
	}
	if (get_option(s) == NULL) {
		die("Unknown variable: '%s'", s);
	}
	set_option(s, t);
	free(s);
}

int
lookup_keyword(char *w, char **keyword_table) {
	int i;
	if (w == NULL)
		return -2;
	for (i = 0;  keyword_table[i] != NULL; i++)
		if (strcmp(w,  keyword_table[i]) == 0)
			return i;
	return -1;
}

double epsilon;

void
initialize_options(void) {
	epsilon = sqrt(DBL_MIN);
	set_option("input_file", "");
	set_option_int("sound_buffer_size", 64000);
	set_option_int("sound_buffer_frames", 1*60*16000);
	
	set_option("sound_file_dir", "ramfs/share");
	set_option("sound_file_name", "%s/%d.sw");
	set_option("sound_details_name", "%s/%d.details");
	set_option("sound_details_name", "%s/%d.details");
	set_option_int("sound_n_files", 3);
	set_option_int("sound_n_skip", -1);
	set_option_int("sound_mono_buffer_size", 65536);
	set_option_boolean("sound_to_stdout", 0);
	set_option_boolean("sound_multiplier", 0);
	
	set_option("alsa_pcm_name", "hw:0,0");
	set_option_int("alsa_n_channels", 2);
	set_option_int("alsa_sampling_rate", 16000);
	set_option_int("alsa_periods_size", 16000);
	set_option_int("alsa_n_periods", 2);
	set_option_int("alsa_buffer_size", 16384);
	
	set_option_boolean("active_high", 0);
	set_option_boolean("beep", 0);
	
	set_option_int("watchdog_sleep_seconds", 5);
	set_option_int("watchdog_max_start_seconds", 1200);
	set_option("watchdog_path1", "ramfs/share/");
	set_option_int("watchdog_path1_seconds", 7200);
	set_option("watchdog_path2", "ramfs/network_up");
	set_option_int("watchdog_path2_seconds", 24*3600);
	
	set_option_double("comb_filter_frequency", 1000);
	set_option_double("comb_filter_feedback_weight", 0.04);
	
}
void
postprocess_options(void) {
}
