#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>

#define errprintf(...) fprintf(stderr, __VA_ARGS__)
#define SETTINGS_L_SIDE_LEN 32
#define SETTINGS_R_SIDE_LEN BUFSIZ

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
int readPageNumberZathura(FILE *fp);
void readBooksDirectory(char *booksDir);

int 
main(int argc, char **argv)
{
    /* get usename
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    */
    
    char pathName[PATH_MAX];
    fz_realpath(argv[1], pathName);

    struct Settings appSettings = {0};
    FILE *settFile = openSettingsFile();
    parseSettingsFile(&appSettings, settFile);
    FILE *zathuraFP = fopen(appSettings.zathuraHist, "rb");

    readBooksDirectory(appSettings.booksDir);

    char ch;
    char filename[FILENAME_MAX];
    bool readBrack = false;
    for(int i = 0; (ch = getc(zathuraFP)) != EOF; )
    {
        if(readBrack && ch == ']')
        {
            filename[i] = '\0';
            if(!strcmp(filename, pathName))
            {
                printf("%s\n", filename);
                printf("%d\n", readPageNumberZathura(zathuraFP));
                break;
            }
            memset(filename, '\0', i);
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
    fclose(settFile);
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

void
readBooksDirectory(char *booksDir)
{
    DIR *dir = opendir(booksDir);
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL)
    {
        printf("%s\n", ent->d_name);
    }
    closedir(dir);
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