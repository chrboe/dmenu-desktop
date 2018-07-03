/*
 * main.c
 * Copyright (C) 2018 christoph <christoph@boehmwalder.at>
 *
 * Distributed under terms of the GPLv3 license.
 */

/*
 * currently not portable because of getline()
 */
#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <dirent.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define KEY_TYPE "Type="
#define KEY_TYPE_LEN (sizeof(KEY_TYPE) - 1)

#define KEY_NAME "Name="
#define KEY_NAME_LEN (sizeof(KEY_NAME) - 1)

#define KEY_EXEC "Exec="
#define KEY_EXEC_LEN (sizeof(KEY_EXEC) - 1)

#define KEY_NO_DISPLAY "NoDisplay="
#define KEY_NO_DISPLAY_LEN (sizeof(KEY_NO_DISPLAY) -1)

struct user_choice
{
    char *name;
    char *exec;
};

/* returns:
 *  * 0 on success
 *  * -1 on error
 *  * -2 on skip
 */
int parse_desktop_file(char *path, struct user_choice *result)
{
    FILE *fp;
    char *line = malloc(64);
    size_t len = 0;
    ssize_t read;
    bool in_group = false;
    bool exec_found = false, name_found = false;
    size_t valuelen;

    fp = fopen(path, "r");

    while ((read = getline(&line, &len, fp)) != -1) {
        if (!in_group) {
            /* scan for the [Desktop Entry] group */
            if (strcmp(line, "[Desktop Entry]\n") != 0) {
                continue;
            }

            in_group = true;
            continue;
        }

        /* check that we aren't entering any other group */
        if (line[0] == '[') {
            break;
        }

        /* Type= */
        if (strncmp(line, KEY_TYPE, KEY_TYPE_LEN) == 0) {
            if (strcmp(line + KEY_TYPE_LEN, "Application\n") != 0) {
                /* type is not application, quit */
                return -2;
            }
        }

        /* Name= */
        if (strncmp(line, KEY_NAME, KEY_NAME_LEN) == 0) {
            /* TODO: localization? */
            valuelen = strlen(line) - KEY_NAME_LEN;

            /* copy one less byte because of the newline */
            strncpy(result->name, line + KEY_NAME_LEN, valuelen - 1);
            result->name[valuelen - 1] = '\0';
            name_found = true;
        }

        /* Exec= */
        if (strncmp(line, KEY_EXEC, KEY_EXEC_LEN) == 0) {
            valuelen = strlen(line) - KEY_EXEC_LEN;

            /* copy one less byte because of the newline */
            strncpy(result->exec, line + KEY_EXEC_LEN, valuelen - 1);
            result->exec[valuelen - 1] = '\0';
            exec_found = true;
        }

        /* NoDisplay= */
        if (strncmp(line, KEY_NO_DISPLAY, KEY_NO_DISPLAY_LEN) == 0) {
            if (strcmp(line + KEY_NO_DISPLAY_LEN, "true\n") == 0) {
                /* skip NoDisplay=true */
                return -2;
            }
        }
    }

    /* didn't find the group header */
    if (!in_group)
        return -1;

    /* invalid desktop file */
    if (!name_found || !exec_found)
        return -1;

    free(line);
    fclose(fp);

    return 0;
}

bool contains_name(struct user_choice ***choices, size_t index, char *name)
{
    for (int i = 0; i < index; i++) {
        if (strcmp((*choices)[i]->name, name) == 0) {
            return true;
        }
    }

    return false;
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
    struct dirent *dir;
    char *fnametmp;
    char *token;
    char current_abspath[PATH_MAX];
    size_t choice_index = 0;
    size_t choices_len = 64;
    int rc;

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
            while ((dir = readdir(d)) != NULL) {
                fnametmp = NULL;

                /* only handle regular files (TODO: symlinks too?) */
                if (dir->d_type != DT_REG)
                    continue;

                /* check if it ends with ".desktop" */
                fnametmp = strrchr(dir->d_name, '.');
                if (!fnametmp || strcmp(fnametmp, ".desktop") != 0)
                    continue;

                snprintf(current_abspath, PATH_MAX, "%s%s", token, dir->d_name);

                struct user_choice *choice = malloc(sizeof(struct user_choice));
                choice->name = malloc(256);
                choice->exec = malloc(256);
                rc = parse_desktop_file(current_abspath, choice);
                if (rc == -1 ||  rc == -2) {
                    /* error or not interesting */
                    free(choice->name);
                    free(choice->exec);
                    free(choice);
                    continue;
                }

                if (contains_name(choices, choice_index, choice->name)) {
                    /* skip duplicate names */
                    free(choice->name);
                    free(choice->exec);
                    free(choice);
                    continue;
                }

                (*choices)[choice_index] = choice;
                choice_index++;

                /* check if list is too small */
                if (choice_index >= choices_len) {
                    choices_len *= 2;
                    *choices = realloc(*choices, choices_len);
                }
            }
            closedir(d);
        }

        token = strtok(NULL, ":");
    }

    return choice_index;
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
        execl("/usr/bin/dmenu", "dmenu", "-i", (char *)NULL);
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

int main(int argc, char **argv)
{
    struct user_choice **choices;
    int written;

    written = search_desktop_files(&choices);
    launch_dmenu(choices, written);
    free_choices(choices, written);
}
