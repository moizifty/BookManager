#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <linux/limits.h>
#include <unistd.h>
#include <dirent.h>

#define errprintf(...) fprintf(stderr, __VA_ARGS__)
#define SETTINGS_L_SIDE_LEN 32
#define SETTINGS_R_SIDE_LEN BUFSIZ

struct Book
{
    char path[PATH_MAX];
    char name[FILENAME_MAX];
    int currReadPages;
};

struct SettingsStatement
{
    char lSide[SETTINGS_L_SIDE_LEN];
    char rSide[SETTINGS_R_SIDE_LEN];
};

struct Settings
{
    char booksDir[FILENAME_MAX];
    char zathuraHist[FILENAME_MAX];
};

FILE *openSettingsFile();
void tokeniseSettingsFile(char *str, char lSide[], char rSide[]);
void parseSettingsFile(struct Settings *settings, FILE *fp);
char **parseZathuraHist(char *path);
int readPageNumberZathura(FILE *fp);
void readBooksDirectory(char *booksDir, char **zb);

int 
main(int argc, char **argv)
{
    struct Settings appSettings = {0};
    FILE *settingsFile = openSettingsFile();
    parseSettingsFile(&appSettings, settingsFile);
    char **zathuraBookList = parseZathuraHist(appSettings.zathuraHist);
    readBooksDirectory(appSettings.booksDir, zathuraBookList);

    fclose(settingsFile);
    free(zathuraBookList);
}

FILE *
openSettingsFile()
{
    FILE *fp;
    if((fp = fopen("settings", "ab+")) == NULL)
    {
        errprintf("Could not open or create settings file\n");
        exit(EXIT_FAILURE);
    }

    return fp;
}

void
parseSettingsFile(struct Settings *settings, FILE *fp)
{
    char line[BUFSIZ];
    struct SettingsStatement stat;
    while(fgets(line, BUFSIZ, fp) !=  NULL)
    {
        tokeniseSettingsFile(line, stat.lSide, stat.rSide);

        if(!strcmp(stat.lSide, "booksDir"))
        {
            strcpy(settings->booksDir, stat.rSide);
            memset(&stat, 0, sizeof(struct SettingsStatement));
        }
        else if(!strcmp(stat.lSide, "zathuraHist"))
        {
            strcpy(settings->zathuraHist, stat.rSide);
            memset(&stat, 0, sizeof(struct SettingsStatement));
        }
    }
}

void
tokeniseSettingsFile(char *str, char lSide[], char rSide[])
{
    int i = 0;
    bool seenEqual = false;
    
    for(i = 0; *str != '\n' && *str && *str != '#'; i++, str++)
    {
        if(*str == '=' && !seenEqual)
        {
            lSide[i] = '\0';
            seenEqual = true;
            i = -1;
            continue;
        }
        if(!seenEqual)
        {
            lSide[i] = *str;
        }
        else
        {
            rSide[i] = *str;
        }
    }
    rSide[i] = '\0';
}

char **
parseZathuraHist(char *path)
{
    char ch;
    char filename[FILENAME_MAX];
    int currNumBooks = 0;
    char **bookList;
    if((bookList = malloc(10 * sizeof(char *))) == NULL)
    {
        errprintf("Could not allocate memory at function %s\n", __func__);
        exit(EXIT_FAILURE);
    }
    bool readBrack = false;
    FILE *zathuraFP;
    if((zathuraFP = fopen(path, "rb")) == NULL)
    {
        errprintf("Could not open zathura history file at path: %s\n", path);
        exit(EXIT_FAILURE);
    }
    for(int i = 0; (ch = getc(zathuraFP)) != EOF; )
    {
        if(readBrack && ch == ']')
        {
            filename[i] = '\0';
            if((bookList[currNumBooks++] = malloc(i)) == NULL)
            {
                errprintf("Could not allocate memory at function %s\n", __func__);
                exit(EXIT_FAILURE);
            }
            strcpy(bookList[currNumBooks - 1], filename);
            readBrack = i = 0;
        }
        if(readBrack)
        {
            filename[i++] = ch;
        }
        if(ch == '[' && !readBrack)
        {
            readBrack = true;
            continue;
        }
    }
    fclose(zathuraFP);
    bookList[currNumBooks] = NULL;
    return bookList;
}

int 
readPageNumberZathura(FILE *fp)
{
    int ch;
    char pgeNum[10];
    while((ch = getc(fp)) != '=');

    for(int i = 0; isdigit(ch = getc(fp)); i++)
    {
        pgeNum[i] = ch;
    }

    return atoi(pgeNum);
}

void
readBooksDirectory(char *booksDir, char **zb)
{
    DIR *dir = opendir(booksDir);
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL)
    {
        for(char **bookStr = zb; *bookStr != NULL; bookStr++)
        {
            char pathName[PATH_MAX] = {0};
            strcat(pathName, booksDir);
            strcat(pathName, "/");
            strcat(pathName, ent->d_name);
            
            if(!strcmp(pathName, *bookStr))
            {
                printf("%s\n", ent->d_name);
            }
        }
    }
    closedir(dir);
}