#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"
#include "string.h"
#include "errno.h"
#include "debug.h"

#define   RM_MAX_MAPS_PER_POOL	25
#define   RM_MAX_POOLS		MAX_RESPONSE_MSGES - 2
#define   RM_POOLS_CMD          "pools"
#define   RM_RELOAD_MAPS_CMD    "reload-maps"
#define   RM_RANDOM_MAP_CMD     "rm"

static int npools = 0;
static struct pool **pools = NULL;
static char **commands;
static const int command_qty = 3;
static char pools_cmd[] = RM_POOLS_CMD;
static char reload_maps_cmd[] = RM_RELOAD_MAPS_CMD;
static char random_map_cmd[] = RM_RANDOM_MAP_CMD;
static char plug_name[] = "random map pools";

char **get_commands()
{
	return commands;
}

int get_command_qty()
{
	return command_qty;
}

char *get_plug_name()
{
	return plug_name;
}

/*
 * Represents a map pool.
 *
 * name is the name of the pool (e.g. the tournament or league
 * of the pool's origin).
 *
 * maps is an array of maps that exist in the pool.
 *
 * nmaps is the number of maps in the pool.
 */
struct pool
{
	char *name;
	char **maps;
	int nmaps;
};

/*
 * Creates and initializes a pool struct
 *
 * name is the name of the map pool. The pointer must point to a
 * memory location that has already been allocated.
 *
 * maps is an array of maps contained in this pool. Each map
 * must already have been allocated.
 *
 * nmaps is the number of maps in the pool.
 */
struct pool *create_pool(char *name, char **maps, int nmaps)
{
	struct pool *p;
	int i;

	p = malloc(sizeof(*p));
	p->name = name;
	p->nmaps = nmaps;

	p->maps = malloc(sizeof(*(p->maps)) * nmaps);
	for (i = 0; i < nmaps; i++)
		p->maps[i] = maps[i];

	log_info("Created pool %s", name);
	return p;
}

/*
 * Frees a map pool and all associated memory.
 */
void free_pool(struct pool *p)
{
	int i;

	for (i = 0; i < p->nmaps; i++)
		free(p->maps[i]);
	free(p->maps);
	free(p->name);
	free(p);

	return;
}

static int filter(const struct dirent *d)
{
	if (strcmp(d->d_name, "..") == 0 || strcmp(d->d_name, ".") == 0)
		return 0;

	return 1;
}

int plug_init()
{
	FILE *fp;
	char buf[100];
	char *maps[RM_MAX_MAPS_PER_POOL];
	struct dirent **namelist;
	int n, i;

	commands = malloc(sizeof(*commands) * command_qty);
	commands[0] = pools_cmd;
	commands[1] = random_map_cmd;
	commands[2] = reload_maps_cmd;

	npools = 0;
	pools = malloc(sizeof(*pools) * RM_MAX_POOLS);
	n = scandir("../rmap", &namelist, &filter, alphasort);

	if (n < 0) {
		log_err("Error scanning directory for plugins: %s",
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	while (n-- && npools < RM_MAX_POOLS) {
		char location[100] = "../rmap/";
		strcpy(location + 8, namelist[n]->d_name);

		fp = fopen(location, "r");
		if (!fp) {
			log_warn("%s: Error loading  maps: %s", location,
					strerror(errno));
			continue;
		}

		i = 0;
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			char *tok;

			if (strrchr(buf, '\n') == NULL) {
				log_warn("%s:%d was too long to load.",
						location, i + 1);
				continue;
			}

			tok = strtok(buf, "\r\n");
			if (tok == NULL) {
				log_warn("%s:%d blank line ignored.",
						location, i + 1);
				continue;
			}
			maps[i]= strdup(tok);
			i++;

		}
		fclose(fp);

		/* only count this as a valid map pool if the pool has
		 * at least one map */
		if (i > 0) {
			struct pool *p;
			char *name;

			name = strdup(namelist[n]->d_name);
			p = create_pool(name, maps, i);
			pools[npools] = p;
			npools++;
		}
		else {
			log_warn("%s had no maps listed.", location);
		}

		free(namelist[n]);
	}

	if (npools == RM_MAX_POOLS) {
		log_warn("Too many map pools; truncating remaining pools.");
	}

	free(namelist);
	return 0;
}

int plug_close()
{
	int i;

	free(commands);

	for (i = 0; i < npools; i++)
		free_pool(pools[i]);
	free(pools);

	return 0;
}

int create_cmd_response(char *src, char *dest,
		char *cmd, char *msg, struct plug_msg **responses,
		int *count)
{
	char buf[IRC_BUF_LENGTH];
	char *n;

	*count = 0;

	/* pool number (if specified) */
	n = strtok(msg, " ");

	if (strcmp(cmd, RM_POOLS_CMD) == 0) {
		int i;

		sprintf(buf, "%s", "Available map pools:");
		responses[*count] =
			create_plug_msg(channel, buf);
		if (responses[*count])
			(*count)++;

		sprintf(buf, "%s", "All pools (don't specify a number)");
		responses[*count] =
			create_plug_msg(channel, buf);
		if (responses[*count])
			(*count)++;

		for (i = 0; i < npools; i++) {
			sprintf(buf, "%d. %s", i+1, pools[i]->name);
			responses[*count] =
				create_plug_msg(channel, buf);
			if (responses[*count])
				(*count)++;
		}
	} else if (strcmp(cmd, RM_RELOAD_MAPS_CMD) == 0) {
		plug_close();
		plug_init();

		sprintf(buf, "%s", "Reloaded map pools");
		responses[*count] =
			create_plug_msg(channel, buf);
		if (responses[*count])
			(*count)++;
	} else if (strcmp(cmd, RM_RANDOM_MAP_CMD) == 0) {
		int pn, choice;

		if (n != NULL) {
			/* pick a map from the specified pool */
			pn = atoi(n);
			if (pn == 0 || pn > npools)
				return -1;
			pn--;

			choice = random() % (pools[pn]->nmaps);

			sprintf(buf, "Map: %s",pools[pn]->maps[choice]);
			responses[*count] =
				create_plug_msg(channel, buf);
			if (responses[*count])
				(*count)++;
		} else {
			/* pick a map from the union of all pools */
			int totalmaps = 0, i;

			for (i = 0; i < npools; i++)
				totalmaps += pools[i]->nmaps;
			choice = random() % totalmaps;

			i = 0;
			while (choice >= pools[i]->nmaps) {
				choice -= pools[i]->nmaps;
				i++;
			}

			sprintf(buf, "Map: %s", pools[i]->maps[choice]);
			responses[*count] =
				create_plug_msg(channel, buf);
			if (responses[*count])
				(*count)++;
		}
	}

	return 0;
}
