#include "i.h"

static GArray *keyfiles;
GKeyFile *
param_get_keyfile(char *group, char *key) {
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
	die("No value for parameter %s::%s\n", group, key);
}



GKeyFile *
param_set_keyfile(char *group, char *key) {
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
param_add_config_file(char *pathname, int ignore_missing) {
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
param_assignment(char *assignment) {
	char **a = g_regex_split_simple("[:=\\s]+", assignment, 0, 0);
//	for (int i=0; a[i];i++) printf("'%s'\n", a[i]);
	int n = 0;
	while (a[n++]);
	if (n != 4)
		die("Can not parse parameter assignment '%s' (n=%d)", assignment, n);
	param_set_string(a[0], a[1], a[2]);
	g_strfreev(a);
}


int
param_get_boolean(char *group, char *key) {
	int value = g_key_file_get_boolean(param_get_keyfile(group, key), group, key, NULL);
	dp(30, "%s:%s -> %s\n", group, key, value ? "TRUE" : "FALSE");
	return value;
}

int
param_get_boolean_with_default(char *group, char *key, int default_value) {
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
param_set_boolean(char *group, char *key, int value) {
	dp(30, "%s:%s <- %s\n", group, key, value ? "TRUE" : "FALSE");
	g_key_file_set_boolean(param_get_keyfile(group, key), group, key, value);
}

int
param_get_integer(char *group, char *key) {
	int value = g_key_file_get_integer(param_get_keyfile(group, key), group, key, NULL);
	dp(30, "%s:%s -> %d\n", group, key, value);
	return value;
}

int
param_get_integer_with_default(char *group, char *key, int default_value) {
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
param_set_integer(char *group, char *key, int value) {
	dp(30, "%s:%s <- %d\n", group, key, value);
	g_key_file_set_integer(param_get_keyfile(group, key), group, key, value);
}

double
param_get_double(char *group, char *key) {
	double value = g_key_file_get_double(param_get_keyfile(group, key), group, key, NULL);
	dp(30, "%s:%s -> %g\n", group, key, value);
	return value;
}

double
param_get_double_with_default(char *group, char *key, double default_value) {
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
param_set_double(char *group, char *key, double value) {
	dp(30, "%s:%s <- %g\n", group, key, value);
	g_key_file_set_double(param_get_keyfile(group, key), group, key, value);
}

char *
param_get_string(char *group, char *key) {
	char *value = g_key_file_get_string(param_get_keyfile(group, key), group, key, NULL);
	dp(30, "%s:%s -> '%s'\n", group, key, value);
	return value;
}

char *
param_get_string_with_default(char *group, char *key, char *default_value) {
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
param_get_string_n(char *group, char *key) {
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
param_set_string(char *group, char *key, char *value) {
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
