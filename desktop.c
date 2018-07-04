/*
 * desktop.c - Helpers for parsing .desktop files
 * Copyright (C) 2018 Christoph Böhmwalder <christoph@boehmwalder.at>
 *
 * Distributed under terms of the GPLv3 license.
 */

#include "desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define KEY_TYPE "Type="
#define KEY_NAME "Name="
#define KEY_EXEC "Exec="
#define KEY_NO_DISPLAY "NoDisplay="

static const char *const KEYS[DESKTOP_KEY_COUNT] =
{
    "Type=",
    "Version=",
    "Name=",
    "GenericName=",
    "NoDisplay=",
    "Comment=",
    "Icon=",
    "Hidden=",
    "OnlyShowIn=",
    "NotShowIn=",
    "DBusActivatable=",
    "TryExec=",
    "Exec=",
    "Path=",
    "Terminal=",
    "Actions=",
    "MimeType=",
    "Categories=",
    "Implements=",
    "Keywords=",
    "StartupNotify=",
    "StartupWMClass=",
    "URL="
};

/* length of a constant string */
#define CSLEN(x) (sizeof(x) - 1)

void init_desktop_file(struct desktop_file *file)
{
    file->id = "";
    file->groups = NULL;
    file->group_count = 0;
}

static struct desktop_group *new_group(struct desktop_file *file, const char *line)
{
    struct desktop_group *group;
    const char *name;
    size_t name_len;
    char *close_bracket_pos;

    /* figure out the group name */
    name = &line[1]; /* skip the [ */
    close_bracket_pos = strrchr(line, ']'); /* find ] */
    if (!close_bracket_pos) {
        /* ] not found */
        return NULL;
    }

    name_len = close_bracket_pos - name;

/*    printf("DBG: new group name: %.*s (%d)\n", (int)name_len, name, (int)name_len);*/

    /* resize array */
    file->group_count++;
    file->groups = realloc(file->groups,
            file->group_count * sizeof(struct desktop_group));

    /* populate the new group */
    group = &file->groups[file->group_count - 1];
    group->name = malloc(name_len + 1);
    strncpy(group->name, name, name_len);
    group->name[name_len] = '\0';
    group->entries = NULL;
    group->entry_count = 0;

    /*printf("DBG: new group done\n");*/
    return group;
}

static struct desktop_entry *new_entry(struct desktop_group *group,
        const char *line, size_t line_len)
{
    struct desktop_entry *entry;
    const char *key, *value;
    size_t key_len, val_len;
    const char *eq_sign_pos;

    /* figure out offsets and lengths */
    key = line;
    eq_sign_pos = strchr(line, '=');
    key_len = eq_sign_pos - key;

    value = eq_sign_pos + 1;
    val_len = line_len - key_len - 2; /* -2 to chop off the = and \n */

    /*printf("DBG: new key: \"%.*s\" (%ld), val: \"%.*s\" (%ld)\n",
            (int)key_len, key, key_len, (int)val_len, value, val_len);*/

    /* resize array */
    group->entry_count++;
    group->entries = realloc(group->entries,
            group->entry_count * sizeof(struct desktop_entry));

    entry = &group->entries[group->entry_count - 1];
    entry->key= malloc(key_len + 1);
    strncpy(entry->key, key, key_len);
    entry->key[key_len] = '\0';

    entry->value = malloc(val_len + 1);
    strncpy(entry->value, value, val_len);
    entry->value[val_len] = '\0';

    //printf("DBG: new entry done\n");
    return entry;
}

void free_desktop_file(struct desktop_file *file)
{
    struct desktop_group *group;
    struct desktop_entry *entry;
    int i, j;
    for (i = 0; i < file->group_count; i++) {
        group = &file->groups[i];

        for (j = 0; j < group->entry_count; j++) {
            entry = &group->entries[j];
            free(entry->key);
            free(entry->value);
        }

        free(group->name);
        free(group->entries);
    }

    free(file->groups);
    file->groups = NULL;
    file->group_count = 0;
}

/* returns:
 *  * 0 on success
 *  * -1 on error
 */
int parse_desktop_file(char *path, struct desktop_file *result)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    struct desktop_group *current_group = NULL;

    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        /* Look for the beginning of a new group */
        if (line[0] == '[') {
            /* Allocate a new group */
            current_group = new_group(result, line);
            continue;
        }

        if (!current_group) {
            /*
             * Spin as long as we haven't hit a group yet.
             * To quote the desktop entry specification:
             *
             *     There should be nothing preceding [the "Desktop Entry"] group
             *     in the desktop entry file but possibly one or more comments.
             */
            continue;
        }

        if (line[0] == '#' || line[0] == '\n') {
            /* Skip comments and blank lines */
            continue;
        }

        /*
         * Check all possible types and parse them accordingly
         */
        for (int i = 0; i < DESKTOP_KEY_COUNT; i++) {
            if (strncmp(line, KEYS[i], strlen(KEYS[i])) == 0) {
                new_entry(current_group, line, read);
                break;
            }
        }
    }

    free(line);
    fclose(fp);

    return 0;
}

