/*
 * desktop.h - Helpers for parsing .desktop files
 * Copyright (C) 2018 Christoph Böhmwalder <christoph@boehmwalder.at>
 *
 * Distributed under terms of the GPLv3 license.
 */

#ifndef DESKTOP_H
#define DESKTOP_H

#include <stddef.h>

struct desktop_entry
{
    char *key;
    char *value;
};

struct desktop_group
{
    char *name;
    struct desktop_entry *entries;
    size_t entry_count;
};

struct desktop_file
{
    char *id;
    struct desktop_group *groups;
    size_t group_count;
};

enum desktop_key
{
    TYPE = 0,
    VERSION,
    NAME,
    GENERIC_NAME,
    NO_DISPLAY,
    COMMENT,
    ICON,
    HIDDEN,
    ONLY_SHOW_IN,
    NOT_SHOW_IN,
    DBUS_ACTIVATABLE,
    TRY_EXEC,
    EXEC,
    PATH,
    TERMINAL,
    ACTIONS,
    MIME_TYPE,
    CATEGORIES,
    IMPLEMENTS,
    KEYWORDS,
    STARTUP_NOTIFY,
    STARTUP_WM_CLASS,
    URL,
    DESKTOP_KEY_COUNT /* enum end, not an actual entry */
};

int parse_desktop_file(char *path, struct desktop_file *result);
void init_desktop_file(struct desktop_file *file);
void free_desktop_file(struct desktop_file *file);

#endif /* !DESKTOP_H */
