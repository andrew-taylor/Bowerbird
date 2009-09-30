#include "i.h"

/** all loaded keyfiles are stored here */
static GArray *keyfiles;
/** used when parameters are set that aren't in an existing keyfile */
static GKeyFile *default_keyfile;


GKeyFile *
param_get_keyfile(const char *group, const char *key) {
	dp(31, "get_keyfile(group=%s, key=%s)\n", group, key);
	if(!keyfiles)
		param_initialize();
	assert(keyfiles);
	dp(31, "keyfiles=%p\n", keyfiles);
	for (int i = 0; i < keyfiles->len; i++) {
		GKeyFile *keyfile = g_array_index(keyfiles, GKeyFile *, i);
		if (g_key_file_has_key(keyfile, group, key, NULL))
			return keyfile;
	}
	dp(31, "No keyfile has parameter %s::%s\n", group, key);
	if (!default_keyfile)
		default_keyfile = g_key_file_new();
	return default_keyfile;
}


/** This function doesn't seem to do anything useful - Cam 30/9/9 
 * 	It may require being updated due to other changes.
 */
GKeyFile *
param_set_keyfile(const char *group, const char *key) {
	if(!keyfiles)
		param_initialize();
	assert(keyfiles);
	if (keyfiles->len)
		return g_array_index(keyfiles, GKeyFile *, 0);
	die("setting parameters without config files unimplemented\n");
}


void
param_initialize(void) {
	if(keyfiles)
		return;
	keyfiles = g_array_new(0, 0, sizeof (GKeyFile *));
	dp(31, "keyfiles->len=%d\n", keyfiles->len);
	const char *directories[] = {NULL, "$BOWERBIRD_HOME", g_get_user_data_dir(), g_get_user_config_dir(), "$HOME", "$home",  "../..","..","."};
	char *names[] = {"bowerbird_config", ".bowerbird_config"};
//	verbosity = 33;
	for (int i = (sizeof directories/sizeof directories[0]) - 1 ; i >= 0; i--) {
		const char *directory = directories[i];
		if (directory) {
			if (directory[0] == '$')
				directory = g_getenv(directory+1);
			if (!directory)
				continue;
		}
		for (int j = 0; j < sizeof names/sizeof names[0]; j++)
			param_load_file(directory, names[j]);
	}
}	

void
param_load_file(const char *directory, const char *filename) {
	dp(31, "param_load_file(%s, %s)\n", directory, filename);
	if (!filename)
		return;
	char pathname[(directory?strlen(directory):0)+strlen(filename)+5];
	if (!directory)
		sprintf(pathname, "%s", filename);
	else
		sprintf(pathname, "%s/%s", directory, filename);
	param_add_config_file(pathname, 1);	
}

void
param_add_config_file(const char *pathname, int ignore_missing) {
	dp(21, "checking %s\n", pathname);
	if (access(pathname, R_OK)) {
		if (ignore_missing)
			return;
		die("config file '%s' can not be accessed\n", pathname);
	}
	GKeyFile *keyfile = g_key_file_new();
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS;
  	dp(2, "Loading configuration information from %s\n", pathname);
  	GError *error = NULL;
	if (!g_key_file_load_from_file(keyfile, pathname, flags, &error))
		die("config file '%s' load failed: %s", pathname, error->message);
	g_array_append_val(keyfiles, keyfile);
}

void
param_assignment(const char *assignment, const char *default_group) {
	char **a = g_regex_split_simple("(([^:=\\s]+) ?: ?)?([^:=\\s]+) ?= ?([^\\s]+)", assignment, 0, 0);
	dp(30, "split assignment %s into:\n", assignment);
	for (int i=0; a[i];i++) 
		dp(30," %d: '%s'\n", i, a[i]);
	dp(30, "\n");
	if (!a[1])
		die("Can not parse parameter assignment '%s'", assignment);
	else if (strlen(a[2]))
		param_set_string(a[2], a[3], a[4]);
	else
		param_set_string(default_group, a[3], a[4]);
	g_strfreev(a);
}


int
param_get_boolean(const char *group, const char *key) {
	return param_get_boolean_with_default(group, key, INT_MIN);
}

int
param_get_boolean_with_default(const char *group, const char *key, int default_value) {
	GError *g_error = NULL;
	int value = g_key_file_get_boolean(param_get_keyfile(group, key), group, key, &g_error);
	if (!value) {
		if (g_error->code == G_KEY_FILE_ERROR_INVALID_VALUE) {
			dp(0, "Config file has invalid value for %s:%s\n", group, key);
			return default_value;
		} else if (g_error->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND
				|| g_error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
			// ignore missing config entries - that's why we have the default
			dp(30, "%s:%s -> %s (default)\n", group, key, default_value ? "TRUE" : "FALSE");
			return default_value;
		}
	}

	dp(30, "%s:%s -> %s\n", group, key, value ? "TRUE" : "FALSE");
	return value;
}

void
param_set_boolean(const char *group, const char *key, int value) {
	dp(30, "%s:%s <- %s\n", group, key, value ? "TRUE" : "FALSE");
	g_key_file_set_boolean(param_get_keyfile(group, key), group, key, value);
}

int
param_get_integer(const char *group, const char *key) {
	return param_get_integer_with_default(group, key, INT_MIN);
}

int
param_get_integer_with_default(const char *group, const char *key, int default_value) {
	GError *g_error = NULL;
	int value = g_key_file_get_integer(param_get_keyfile(group, key), group, key, &g_error);
	if (value == 0) {
		if (g_error->code == G_KEY_FILE_ERROR_INVALID_VALUE) {
			dp(0, "Config file has invalid value for %s:%s\n", group, key);
			return default_value;
		} else if (g_error->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND
				|| g_error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
			// ignore missing config entries - that's why we have the default
			dp(30, "%s:%s -> %d (default)\n", group, key, default_value);
			return default_value;
		}
	}

	dp(30, "%s:%s -> %d\n", group, key, value);
	return value;
}

void
param_set_integer(const char *group, const char *key, int value) {
	dp(30, "%s:%s <- %d\n", group, key, value);
	g_key_file_set_integer(param_get_keyfile(group, key), group, key, value);
}

double
param_get_double(const char *group, const char *key) {
	return param_get_double_with_default(group, key, DBL_MIN);
}

double
param_get_double_with_default(const char *group, const char *key, double default_value) {
	GError *g_error = NULL;
	double value = g_key_file_get_double(param_get_keyfile(group, key), group, key, &g_error);
	if (value == 0.0) {
		if (g_error->code == G_KEY_FILE_ERROR_INVALID_VALUE) {
			dp(0, "Config file has invalid value for %s:%s\n", group, key);
			return default_value;
		} else if (g_error->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND
				|| g_error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
			// ignore missing config entries - that's why we have the default
			dp(30, "%s:%s -> %g (default)\n", group, key, default_value);
			return default_value;
		}
	}

	dp(30, "%s:%s -> %g\n", group, key, value);
	return value;
}

void
param_set_double(const char *group, const char *key, double value) {
	dp(30, "%s:%s <- %g\n", group, key, value);
	g_key_file_set_double(param_get_keyfile(group, key), group, key, value);
}

char *
param_get_string(const char *group, const char *key) {
	return param_get_string_with_default(group, key, "");
}

char *
param_get_string_with_default(const char *group, const char *key, char *default_value) {
	GError *g_error = NULL;
	char *value = g_key_file_get_string(param_get_keyfile(group, key), group, key, &g_error);
	if (value == NULL) {
		dp(30, "%s:%s -> '%s' (default)\n", group, key, default_value);
		return default_value;
	}

	dp(30, "%s:%s -> '%s'\n", group, key, value);
	return value;
}

char *
param_get_string_n(const char *group, const char *key) {
	GKeyFile *kf = param_get_keyfile(group, key);
	char *value = g_key_file_get_string(kf, group, key, NULL);
	if (!value) {
		dp(30, "%s:%s -> NULL\n", group, key);
		return NULL;
	}
	dp(30, "%s:%s -> '%s'\n", group, key, value);
	if (strlen(value)) 
		return value;
	g_free(value);
	return NULL;
}

void
param_set_string(const char *group, const char *key, const char *value) {
	dp(30, "%s:%s <- '%s'\n", group, key, value);
	g_key_file_set_string(param_get_keyfile(group, key), group, key, value);
}

char *
param_sprintf(char *group, char *param, ...) {
	va_list ap;
	va_start(ap, param);
	char *format = param_get_string(group, param);
	char *s = NULL;
	if (format && strlen(format))
		s = g_strdup_vprintf(format, ap);
	if (format)
		g_free(format);		
	return s;
}
