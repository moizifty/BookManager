//pdfinfo Books/os-dev.pdf | \grep --color=auto Pages: | awk '{print $2}'
//above line gets number of pages
//remove mupdf headers and use poppler
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
    char *path;
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
struct Book *parseZathuraHist(char *path);
int readPageNumberZathura(FILE *fp);
void readBooksDirectory(char *booksDir, struct Book *bookList);
void freeBookList(struct Book *bookList);
int getTotalPageCount(char *path);

int 
main(int argc, char **argv)
{
    struct Settings appSettings = {0};
    FILE *settingsFile = openSettingsFile();
    parseSettingsFile(&appSettings, settingsFile);
    struct Book *zathuraBookList = parseZathuraHist(appSettings.zathuraHist);
    readBooksDirectory(appSettings.booksDir, zathuraBookList);
    
    fclose(settingsFile);
    freeBookList(zathuraBookList);
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

struct Book *
parseZathuraHist(char *path)
{
    char ch;
    char filename[FILENAME_MAX];
    int currNumBooks = 0;
    struct Book *bookList;
    if((bookList = malloc(10 * sizeof(struct Book))) == NULL)
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
            if((bookList[currNumBooks++].path = malloc(i)) == NULL)
            {
                errprintf("Could not allocate memory at function %s\n", __func__);
                exit(EXIT_FAILURE);
            }
            bookList[(currNumBooks - 1)].currReadPages = readPageNumberZathura(zathuraFP);
            strcpy(bookList[currNumBooks - 1].path, filename);
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
readBooksDirectory(char *booksDir, struct Book *bookList)
{
    DIR *dir = opendir(booksDir);
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL)
    {
        for(struct Book *bookPtr = bookList; bookPtr->path != NULL; bookPtr++)
        {
            char pathName[PATH_MAX] = {0};
            strcat(pathName, booksDir);
            strcat(pathName, "/");
            strcat(pathName, ent->d_name);
            
            if(!strcmp(pathName, bookPtr->path))
            {
                printf("%s || %d/%d\n", ent->d_name, bookPtr->currReadPages, getTotalPageCount(bookPtr->path));
            }
        }
    }
    closedir(dir);
}

//does not get .djvu page count
int
getTotalPageCount(char *path)
{
    fz_context *ctx;
    fz_document *doc;

    /* Create a context to hold the exception stack and various caches. */
    ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!ctx)
    {
        fprintf(stderr, "cannot create mupdf context\n");
        return EXIT_FAILURE;
    }

    /* Register the default file types to handle. */
    fz_try(ctx)
        fz_register_document_handlers(ctx);
    fz_catch(ctx)
    {
        fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(ctx));
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }

    /* Open the document. */
    fz_try(ctx)
        doc = fz_open_document(ctx, path);
    fz_catch(ctx)
    {
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(ctx));
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }

    fz_try(ctx)
        return fz_count_pages(ctx, doc);
    fz_catch(ctx)
    {
        fprintf(stderr, "cannot count number of pages: %s\n", fz_caught_message(ctx));
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return EXIT_FAILURE;
    }
}

void
freeBookList(struct Book *bookList)
{
    for(struct Book *book = bookList; book->path != NULL; book++)
    {
        free(book->path);
    }

    free(bookList);
}