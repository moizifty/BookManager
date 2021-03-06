#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <linux/limits.h>
#include <unistd.h>
#include <dirent.h>

#define errprintf(...) fprintf(stderr, __VA_ARGS__)
#define STATMENT_L_SIDE_LEN 100
#define STATMENT_R_SIDE_LEN BUFSIZ

struct Statement
{
    char lSide[STATMENT_L_SIDE_LEN];
    char rSide[STATMENT_R_SIDE_LEN];
};

struct BookMetaDataEntry
{
    struct Statement data;
};

struct Book
{
    char *path;
    char name[FILENAME_MAX];
    struct BookMetaDataEntry *metaData;
    int numMetaData;
    int currReadPages;
};

struct Settings
{
    char booksDir[FILENAME_MAX];
    char zathuraHist[FILENAME_MAX];
};

void newBookFromPath(char *path, struct Book *book);
FILE *openSettingsFile(char *path);
void tokeniseSettingsFile(char *str, char lSide[], char rSide[]);
void parseSettingsFile(struct Settings *settings, FILE *fp);
struct Book *parseZathuraHist(char *path);
int readPageNumberZathura(FILE *fp);
void readBooksDirectory(char *booksDir, struct Book **list, int *numAlloc, int *n);
void freeBookList(struct Book *bookList);
struct BookMetaDataEntry *getBookMetaData(char *path, int *numMetaData);
int getTotalPageCount(struct Book *book);
char *getFileType(char *path);
extern FILE *popen (const char *__command, const char *__modes);

int 
main(int argc, char **argv)
{
    struct Settings appSettings = {0};
    FILE *settingsFile = openSettingsFile(argv[1]);
    parseSettingsFile(&appSettings, settingsFile);
    struct Book *zathuraBookList = parseZathuraHist(appSettings.zathuraHist);
    int totalAlloc = 10;
    int n = 0;
    struct Book *directoryBooks = malloc(totalAlloc * sizeof(struct Book));
    readBooksDirectory(appSettings.booksDir, &directoryBooks, &totalAlloc, &n);
    for(int i = 0; i < n; i++)
    {
        printf("[%.30s]\nname: %.30s...\n", directoryBooks[i].path, directoryBooks[i].name);
        
        putchar('\n');
    }
    
    fclose(settingsFile);
    freeBookList(zathuraBookList);
    freeBookList(directoryBooks);
}

void
newBookFromPath(char *path, struct Book *book)
{
    if(strcmp(getFileType(path), "pdf"))
        return;
    
    int i = strlen(path) - 1;
    for(; i >= 0 && path[i] != '/'; i--)
    {;}

    if( (book->path = malloc(strlen(path) + 1)) == NULL)
    {
        errprintf("Could not allocate memory for book path\n");
        exit(EXIT_FAILURE);
    }
    strcpy(book->path, path);
    strcpy(book->name, path + i + 1);
    book->metaData = getBookMetaData(path, &book->numMetaData);
}

FILE *
openSettingsFile(char *path)
{
    FILE *fp;
    if((fp = fopen(path, "ab+")) == NULL)
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
    struct Statement stat;
    while(fgets(line, BUFSIZ, fp) !=  NULL)
    {
        tokeniseSettingsFile(line, stat.lSide, stat.rSide);

        if(!strcmp(stat.lSide, "booksDir"))
        {
            strcpy(settings->booksDir, stat.rSide);
            memset(&stat, 0, sizeof(struct Statement));
        }
        else if(!strcmp(stat.lSide, "zathuraHist"))
        {
            strcpy(settings->zathuraHist, stat.rSide);
            memset(&stat, 0, sizeof(struct Statement));
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

struct Book *
parseZathuraHist(char *path)
{
    char ch;
    char filename[FILENAME_MAX];
    int totalNumBooks = 5;
    int currNumBooks = 0;
    struct Book *bookList;
    if((bookList = malloc(totalNumBooks * sizeof(struct Book))) == NULL)
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
            if(!strcmp(getFileType(filename), "pdf"))
            {
                newBookFromPath(filename, &bookList[currNumBooks]);
                bookList[currNumBooks++].currReadPages = readPageNumberZathura(zathuraFP);
                if(currNumBooks >= totalNumBooks)
                {
                    totalNumBooks += 5;
                    bookList = realloc(bookList, (totalNumBooks) * sizeof(struct Book));
                }
            }
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
    bookList[currNumBooks].path = NULL;
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
readBooksDirectory(char *booksDir, struct Book **list, int *numAlloc, int *n)
{
    DIR *dir;
    if( (dir = opendir(booksDir)) == NULL)
    {
        errprintf("Could not open directory : \'%s\'\n", booksDir);
        //dont exit program, as im scanning sub directories too via recursion
        return;
    }
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL)
    {
        // 4 = directory, 8 = file
        if((!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) && ent->d_type == 4)
            continue;

        if(ent->d_type == 4)
        {
            char path[PATH_MAX] = {0};
            strcat(path, booksDir);
            strcat(path, "/");
            strcat(path, ent->d_name);
            readBooksDirectory(path, list, numAlloc, n);
        }
        char path[PATH_MAX] = {0};
        strcat(path, booksDir);
        strcat(path, "/");
        strcat(path, ent->d_name);
        if(strcmp(getFileType(path), "pdf"))
            continue;
        newBookFromPath(path, (&(*list)[*n]));
        (*n)++;
        if((*n) >= (*numAlloc))
        {
            *numAlloc += 10;
            *list = realloc(*list, *numAlloc * sizeof(struct Book));
        }
    }
    closedir(dir);
}

int
getTotalPageCount(struct Book *book)
{
    for(int i = 0; i < book->numMetaData; i++)
    {
        if(!strcmp(book->metaData[i].data.lSide, "Pages"))
        {
            return atoi(book->metaData[i].data.rSide);
        }
    }
}

void
freeBookList(struct Book *bookList)
{
    for(struct Book *book = bookList; book->path != NULL; book++)
    {
        free(book->path);
        free(book->metaData);
    }
    free(bookList);
}

struct BookMetaDataEntry *
getBookMetaData(char *path, int *numMetaData)
{
    int ch = 0;
    int i = 0;
    int totalMDEntries = 10;
    int currNumEntries = 0;
    FILE *fp;
    char command[PATH_MAX] = "pdfinfo ";
    strcat(command, "\"");
    strcat(command, path);
    strcat(command, "\" ");
    strcat(command, "2> ");
    strcat(command, "/dev/null");

    if( (fp = popen(command, "r")) == NULL)
    {
        errprintf("Could not read book metadata: %s\n", path);
    }

    bool seenColon = false;
    struct BookMetaDataEntry *mdEntry = malloc(totalMDEntries * sizeof(struct BookMetaDataEntry));
    while((ch = getc(fp)) != EOF)
    {
        if(ch == '\n')
        {
            mdEntry[currNumEntries++].data.rSide[i] = '\0';
            if(currNumEntries >= totalMDEntries)
            {
                mdEntry = realloc(mdEntry, (currNumEntries + 5) * sizeof(struct BookMetaDataEntry));
                totalMDEntries += 5;
            }
            seenColon = i = 0;
        }
        if(isblank(ch) || isspace(ch))
            continue;
        if(ch == ':' && !seenColon)
        {
            mdEntry[currNumEntries].data.lSide[i] = '\0';
            seenColon = true;
            i = 0;
            continue;
        }

        if(!seenColon)
            mdEntry[currNumEntries].data.lSide[i++] = ch;
        else
            mdEntry[currNumEntries].data.rSide[i++] = ch;
    }
    *numMetaData = currNumEntries - 1;
    pclose(fp);
    return mdEntry;
}

inline char *
getFileType(char *path)
{
    int len = strlen(path);
    if(len <= 4)
        return NULL;

    return path + (len - 3);
}