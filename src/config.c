#define _GNU_SOURCE
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "errors.h"
#include "log.h"
#include "main.h"

typedef struct keyval_t {
	char *key;
	char *val;
	struct keyval_t *next;
} keyval_t;

keyval_t *config;
char filename[LINE_MAX];

int config_bool_convert(char *val, long long *llval)
{
	int i;
	char *truth[] = { "1", "true", "yes", "on" };
	char *falsy[] = { "0", "false", "no", "off" };
	for (i = 0; i < sizeof(truth) / sizeof(char *); i++) {
		if (strcmp(val, truth[i]) == 0) {
			*llval = 1;
			return 0;
		}
	}
	for (i = 0; i < sizeof(falsy) / sizeof(char *); i++) {
		if (strcmp(val, falsy[i]) == 0) {
			*llval = 0;
			return 0;
		}
	}
	return ERROR_CONFIG_BOOLEAN;
}

void config_defaults()
{
	logmsg(LOG_DEBUG, "setting config defaults...");
#define X(key, type, val, desc) assert(config_set(key, val) == 0);
CONFIG_DEFAULTS(X)
#undef X
}

char * config_filename()
{

	if (geteuid() == 0) {
		/* we are root */
		snprintf(filename, sizeof(filename), "/etc/%s.conf", PROGRAM_NAME);
	}
        else {
		/* not root, use config in user home */
	        snprintf(filename, sizeof(filename), "%s/.%s.conf", getenv("HOME"), PROGRAM_NAME);
	}

        return filename;
}

void config_free()
{
	config_lock();
	keyval_t *c = config;
	keyval_t *n;
	while (c != '\0') {
		n = c;
		c = c->next;
		free (n->key);
		free (n->val);
		free (n);
	}
	config = '\0';
	config_unlock();
}

void * config_get(char *key)
{
	config_lock();
	keyval_t *c = config;
	while (c != '\0') {
		if (strcmp(key, c->key) == 0) {
			config_unlock();
			return c->val;
		}
		c = c->next;
	}
	config_unlock();
	return NULL;
}

long long config_get_num(char * key)
{
	char *val;
	long long llval;

	assert(config_numeric(key));
	val = config_get(key);

	/* ensure we can logmsg() before config defaults are set */
	if (strcmp(key, "loglevel") == 0 && val == NULL)
		return 0;

	assert(val != NULL);
	llval = strtoll(val, NULL, 10);

	return llval;
}

int config_lock()
{
	return pthread_mutex_lock(&config_mutex);
}

long long config_min(char *key)
{
	CONFIG_LIMITS(CONFIG_MIN)
	return LLONG_MIN;
}

long long config_max(char *key)
{
	CONFIG_LIMITS(CONFIG_MAX)
	return LLONG_MAX;
}

int config_numeric(char * key)
{
	config_type_t type = config_type(key);
	return (type == CONFIG_TYPE_BOOL || type == CONFIG_TYPE_INT) ? 1 : 0;
}

void config_print(int fd)
{
	keyval_t *c = config;
	while (c != '\0') {
		dprintf(fd, "%s %s\n", c->key, c->val);
		c = c->next;
	}
}

int config_process_line(char *line)
{
	char key[LINE_MAX], value[LINE_MAX];
	long long llval;

	if (line[0] == '#') {
		logmsg(LOG_DEBUG, "config_process_line: skipping comment");
	}
	else if (sscanf(line, "%s %lli", key, &llval) == 2) {
		logmsg(LOG_DEBUG, "config_process_line: numeric config option");
		return config_set_num(key, llval);
	}
	else if (sscanf(line, "%[a-zA-Z0-9]", value) == 0) {
		logmsg(LOG_DEBUG, "config_process_line: skipping blank line");
	}
	else if (sscanf(line, "%s %[^\n]", key, value) == 2) {
		logmsg(LOG_DEBUG, "config_process_line: string config option");
		return config_set(key, value);
	}
	else {
		return ERROR_CONFIG_INVALID;
	}

	return 0;
}

int config_read(char *configfile)
{
	FILE *fd;
	char line[LINE_MAX];
	int lc = 0;
	int e = 0;

	if (configfile == NULL)
		configfile = config_get("configfile");

	logmsg(LOG_INFO, "reading config file '%s'", configfile);

	/* open config file */
	fd = fopen(configfile, "r");
	if (fd == NULL) {
		int errsv = errno;
		logmsg(LOG_ERROR, strerror(errsv));
		errno = 0;
		return ERROR_CONFIG_READFAIL;
	}

	/* read line by line */
	while (fgets(line, LINE_MAX, fd) != NULL) {
		lc++;
		if ((e = config_process_line(line))) {
			logmsg(LOG_ERROR, "error in line %i of config file",lc);
			goto config_read_done;
		}
	}

config_read_done:
	/* tidy up */
	fclose(fd);

	return e;
}

int config_reload()
{
	char *configfile;

	configfile = strdup(config_get("configfile"));
	config_free();
	config_defaults();
	config_read(configfile);
	free(configfile);

	return 0;
}

int config_set(char *key, void *val)
{
	keyval_t *c = config;
	keyval_t *p = c;
	keyval_t *n;
	config_type_t type = config_type(key);
	long long min, max, llval;

	logmsg(LOG_DEBUG, "config_set: %s=%s", key, val);

	if (type == CONFIG_TYPE_INVALID) {
		logmsg(LOG_ERROR, "'%s' not a valid configuration option", key);
		return ERROR_CONFIG_INVALID;
	}
	else if (type == CONFIG_TYPE_BOOL) {
		if (config_bool_convert(val, &llval) != 0) {
			logmsg(LOG_ERROR, "'%s' not a boolean value", val);
			return ERROR_CONFIG_BOOLEAN;
		}
	}
	else if (type == CONFIG_TYPE_INT) {
		/* check proposed value is within upper and lower bounds */
		errno = 0;
		llval = strtoll(val, NULL, 10);
		if (errno != 0)
			return ERROR_CONFIG_NOTNUMERIC;
		min = config_min(key);
		max = config_max(key);
		if (llval < min || llval > max)
			return ERROR_CONFIG_BOUNDS;
	}

	if (config_validate_option(key, val) != 0)
		return ERROR_CONFIG_INVALID;

	/* set value */
	config_unset(key);
	config_lock();
	while (c != '\0') {
		p = c;
		c = c->next;
	}
	n = calloc(sizeof(keyval_t), 1);
	n->key = strdup(key);
	n->val = strdup(val);
	if (config == '\0')
		config = n;
	else
		p->next = n;

	config_unlock();

	return 0;
}

int config_set_num(char *key, long long llval)
{
	char *val;
	int e;

	if (asprintf(&val, "%lli", llval) == -1) {
		int errsv = errno;
		print_error(0, errsv, "config_set_num");
		return ERROR_MALLOC;
	}
	e = config_set(key, val);
	free(val);

	return e;
}

config_type_t config_type(char *key)
{

	CONFIG_DEFAULTS(CONFIG_TYPE)
	return CONFIG_TYPE_INVALID;
}

int config_unlock()
{
	return pthread_mutex_unlock(&config_mutex);
}

int config_unset(char *key)
{
	int i = 0;
	keyval_t *c = config;
	keyval_t *p = c;
	config_lock();
	while (c != '\0') {
		if (strcmp(c->key, key) == 0) {
			i++;
			p->next = c->next;
			free(c->key);
			free(c->val);
			free(c);
			c = p->next;
		}
		p = c;
		c = c->next;
	}
	config_unlock();
	logmsg(LOG_DEBUG, "unset %i instances of %s", i, key);

	return i;
}

int config_validate_option(char *key, char *val)
{
	/* TODO: perform necessary validation checks on config settings */
	return 0;
}
