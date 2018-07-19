/*
 * main.c
 * Copyright (C) 2018 Christoph Böhmwalder <christoph@boehmwalder.at>
 *
 * Distributed under terms of the GPLv3 license.
 */

/*
 * currently not portable because of getline()
 */
#define _GNU_SOURCE

#include "dmenu-desktop.h"
#include "desktop.h"
#include "cache.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <dirent.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>

bool contains_name(struct user_choice ***choices, size_t index, char *name)
{
    for (int i = 0; i < index; i++) {
        if (strcmp((*choices)[i]->name, name) == 0) {
            return true;
        }
    }

    return false;
}

void handle_field_codes(char *exec, char **buf, size_t bufsiz)
{
    size_t bufpos = 0;
    char *p = exec;
    while(*p) {
        switch(*p) {
            case '%':
                p++;
                /* BIG TODO: actually handle codes instead of skipping them */
                break;
            default:
                (*buf)[bufpos++] = *p;
                break;
        }
        p++;
    }

    (*buf)[bufpos] = '\0';
}

static int populate_choice(struct user_choice *choice, struct desktop_file *file)
{
    int i, j;
    struct desktop_group *group;
    struct desktop_entry *entry;
    bool name_found = false, exec_found = false;

    for (i = 0; i < file->group_count; i++) {
        group = &file->groups[i];

        /* we're only interested in the [Desktop Entry] group */
        if (strcmp("Desktop Entry", group->name) != 0) {
            continue;
        }

        for (j = 0; j < group->entry_count; j++) {
            entry = &group->entries[j];

            /* if type isn't Application, skip */
            if (strcmp("Type", entry->key) == 0 &&
                    strcmp("Application", entry->value) != 0) {
                return -2;
            }

            /* copy name */
            if (strcmp("Name", entry->key) == 0) {
                strcpy(choice->name, entry->value);
                name_found = true;
            }

            /* copy exec */
            if (strcmp("Exec", entry->key) == 0) {
                strcpy(choice->exec, entry->value);
                exec_found = true;
            }

            /* TODO: implement TryExec */
        }

        /* parse only the Desktop Entry group for now */
        break;
    }

    if (!name_found || !exec_found) {
        /* name AND exec are required */
        return -1;
    }

    /*printf("new user choice. name: %s, exec: %s\n", choice->name, choice->exec);*/

    return 0;
}

/*
 * return:
 *    * 0 on success
 *    * -1 on error
 *    * -2 on skip
 */
static int search_directory(char *parent_dir, DIR *d,
        struct user_choice ***choices, size_t *choice_index,
        size_t *choices_len)
{
    char *fnametmp;
    int rc;
    char current_abspath[PATH_MAX];
    struct desktop_file file;
    struct dirent *dir;
    int error = 0;

    while ((dir = readdir(d)) != NULL) {
        /* only handle regular files (TODO: symlinks too?) */
        if (dir->d_type != DT_REG) {
            continue;
        }

        /* check if it ends with ".desktop" */
        fnametmp = strrchr(dir->d_name, '.');
        if (!fnametmp || strcmp(fnametmp, ".desktop") != 0) {
            continue;
        }

        snprintf(current_abspath, PATH_MAX, "%s%s", parent_dir, dir->d_name);

        struct user_choice *choice = malloc(sizeof(struct user_choice));
        choice->name = malloc(256);
        choice->exec = malloc(256);

        init_desktop_file(&file);
        rc = parse_desktop_file(current_abspath, &file);

        if (rc == -1) {
            /* error */
            free(choice->name);
            free(choice->exec);
            free(choice);

            free_desktop_file(&file);
            printf("error %d on file %s\n", rc, current_abspath);
            continue;
        }

        if (contains_name(choices, *choice_index, choice->name)) {
            /* skip duplicate names */
            free(choice->name);
            free(choice->exec);
            free(choice);

            free_desktop_file(&file);
            printf("duplicate name: ignoring file %s\n", current_abspath);
            continue;
        }

        rc = populate_choice(choice, &file);

        if (rc != 0) {
            free(choice->name);
            free(choice->exec);
            free(choice);

            free_desktop_file(&file);
            continue;
        } else {
            (*choices)[*choice_index] = choice;
            (*choice_index)++;
        }

        /* check if list is too small */
        if (*choice_index >= *choices_len) {
            *choices_len *= 2;
            *choices = realloc(*choices, *choices_len);
        }

        free_desktop_file(&file);
    }

    return error;
}

/*
 * returns:
 *    * number of written entries on success
 */
int search_desktop_files(struct user_choice ***choices)
{
    char searchdirs[PATH_MAX];
    char *data_home, *data_dirs;
    int offs;
    DIR *d;
    char *token;
    size_t choice_index = 0;
    size_t choices_len = 64;

    /* get search directories */
    data_home = getenv("XDG_DATA_HOME");
    if (data_home)
        offs = snprintf(searchdirs, PATH_MAX, "%s/applications/", data_home);
    else
        offs = snprintf(searchdirs, PATH_MAX, "%s%s", getenv("HOME"), "/.local/share/applications/");

    data_dirs = getenv("XDG_DATA_DIRS");
    if (data_dirs)
        snprintf(searchdirs + offs, PATH_MAX, ":%s/applications/", data_dirs);
    else
        snprintf(searchdirs + offs, PATH_MAX, ":/usr/local/share/:/usr/share/applications/");

    /* alloc list */
    *choices = malloc(sizeof(choices) * choices_len);

    /* actually search */
    token = strtok(searchdirs, ":");

    while (token != NULL) {
        d = opendir(token);
        if (d) {
            search_directory(token, d, choices, &choice_index, &choices_len);
            closedir(d);
        }

        token = strtok(NULL, ":");
    }

    return choice_index;
}

void find_and_exec(struct user_choice **choices, size_t num, char *got)
{
    for (int i = 0; i < num; i++) {
        if (strcmp(choices[i]->name, got) == 0) {
            /* exact match */
            if (fork() == 0) {
                char *buf = malloc(256);
                handle_field_codes(choices[i]->exec, &buf, 256);
                /* execute the thing */
                system(buf);
                free(buf);
            }
            return;

        }
    }

    /* no exact match found */
}

void launch_dmenu(struct user_choice **choices, size_t num)
{
    int in_fd[2];
    int out_fd[2];
    pipe(in_fd);
    pipe(out_fd);
    ssize_t nread;
    char *buf;

    /* fork and exec */
    if (fork() == 0) {
        dup2(in_fd[0], 0);
        dup2(out_fd[1], 1);
        close(in_fd[0]);
        close(in_fd[1]);
        close(out_fd[0]);
        close(out_fd[1]);
        execl("/usr/bin/dmenu", "dmenu", "-i", "-f", (char *)NULL);
        exit(0);
    } else {
        int imode = 0;
        close(in_fd[0]);
        close(out_fd[1]);
        for (int i = 0; i < num; i++) {
            dprintf(in_fd[1], "%s\n", choices[i]->name);
        }
        close(in_fd[1]);

        buf = malloc(64);
        nread = read(out_fd[0], buf, 64);

        /* cut off the newline */
        if (nread == 0)
            buf[nread] = '\0';
        else
            buf[nread - 1] = '\0';

        close(out_fd[0]);
        find_and_exec(choices, num, buf);
        free(buf);
    }

}

void free_choices(struct user_choice **choices, size_t len)
{
    for (int i = 0; i < len; i++) {
        free(choices[i]->name);
        free(choices[i]->exec);
        free(choices[i]);
        choices[i] = NULL;
    }

    free(choices);
}

static FILE *fopen_cache(char **cache_file, bool *created)
{
    char cache_dir[PATH_MAX];
    char *xdg_cache_home;
    int mkdir_rc;
    DIR *d;
    FILE *cache_fp;

    xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home)
        snprintf(cache_dir, PATH_MAX, "%s/%s", xdg_cache_home, CACHE_DIRNAME);
    else
        snprintf(cache_dir, PATH_MAX, "%s/%s/%s", getenv("HOME"), ".cache",
                CACHE_DIRNAME);

    /* try to change to cache directory */
    do
    {
        d = opendir(cache_dir);
        if (d) {
            /* success */
            closedir(d);
            break;
        }

        if (errno == ENOENT) {
            /* directory doesn't exist, try to create it */
            mkdir_rc = mkdir(cache_dir, S_IRUSR | S_IWUSR | S_IXUSR);
            if (mkdir_rc == -1) {
                perror("cache mkdir");
                return NULL;
            }
            /* try again */
            continue;
        }

        /* other error */
        perror("cache chdir");
        return NULL;
    } while(!d);

    sprintf(*cache_file, "%s/%s", cache_dir, CACHE_FILENAME);

    /* create the file if it doesn't exist */
    cache_fp = fopen(*cache_file, "r+");
    if (cache_fp) {
        /* success, file already existed */
        *created = false;
        return cache_fp;
    }

    if (errno == ENOENT) {
        cache_fp = fopen(*cache_file, "ab+");
        *created = true;
        if (!cache_fp) {
            perror("cache fopen");
            return NULL;
        }
    }

    return cache_fp;
}

int main(int argc, char **argv)
{
    struct user_choice **choices;
    int written;
    int check_rc;
    FILE *cache_fp;
    bool created;
    bool cache_rebuild;
    char *cache_file = malloc(PATH_MAX);

    cache_fp = fopen_cache(&cache_file, &created);

    cache_rebuild = false;
    if (!created) {
        check_rc = cache_check(cache_file);
        if (check_rc == 0) {
            /* use cache */
            printf("using cache file %s\n", cache_file);
            written = cache_read(cache_fp, &choices);

            /*
             * if there were no entries in the cache, we'll just assume
             * it's broken and rebuild it.
             */
            if (written == 0) {
                cache_rebuild = true;
            }
        } else {
            cache_rebuild = true;
        }
    }

    if (cache_rebuild) {
        printf("rebuilding cache\n");
        written = search_desktop_files(&choices);
        cache_write(cache_fp, choices, written);
        printf("cache written\n");
    }

    fclose(cache_fp);
    free(cache_file);

    launch_dmenu(choices, written);
    free_choices(choices, written);
}
