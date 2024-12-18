
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define WHITESPACE " \t\n" // We want to split our command line up into tokens \
                           // so we need to define what delimits our tokens.   \
                           // In this case  white space                        \
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 10 // Mav shell only supports five arguments

// token and cmd_str used for tokenizing user input
char *token[MAX_NUM_ARGUMENTS]; //Parsed input string separated by white space
char cmd_str[MAX_COMMAND_SIZE];//Entire string inputted by the user. It will be parsed into multiple tokens (47)

char BS_OEMName[8];
int16_t BPB_BytesPerSec;//The amount of bytes in each sector of the fat32 file image
int8_t BPB_SecPerClus;//The amount of sectors per cluster of the fat32 file image
int16_t BPB_RsvdSecCnt;//Amount of reserved sectors in the fat32 image
int8_t BPB_NumFATs;
int16_t BPB_RootEntCnt; //Root entry count
int32_t BPB_FATSz32;
int32_t BPB_RootClus;//Rootcluster location in the fat32 image

int32_t RootDirSectors = 0; //Amount of root directory sectors
int32_t FirstDataSector = 0;//Where the first data sector exists in the fat32 file image.
int32_t FirstSectorofCluster = 0;//First sector of the data cluster exists at point 0 in the fat32 file image.

int32_t currentDirectory;//Current working directory
char formattedDirectory[12];//String to contain the fully formatted string
char BPB_Volume[11];//String to store the volume of the fat32 file image

struct __attribute__((__packed__)) DirectoryEntry
{
    char DIR_Name[11];//Name of the directory retrieved
    uint8_t DIR_Attr;//Attribute count of the directory retrieved
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4];
    uint16_t DIR_FirstClusterLow;     
    uint32_t DIR_FileSize;//Size of the directory (Always 0)
};
struct DirectoryEntry dir[16];//Creation of the directory 

FILE *fp;

void getInput();//Receives input from the user that is parsed into tokens.
void execute();//Main function of the program, acts as the shell receiving commands
void openImage(char file[]);//Opens a file system image to be used.
void closeImage();//Closes the file system before exiting the program.
void printDirectory();//Prints the current working directory (ls)
void changeDirectory(int32_t sector);//Changes directory by user specification (cd)
void getDirectoryInfo();//Prints directory info stored in struct above (line 67)
int32_t getCluster(char *dirname);//Receives the cluster of information to be used in execute (line 82)
int32_t getSizeOfCluster(int32_t cluster);//Receives of the size of the cluster as an attribute
void formatDirectory(char *dirname);//Formats the directory to remove whitespace and concatenate a period between the name and extension.
void get();//Pulls file from the file system image into your cwd (current working directory)
void decToHex(int dec);//Converts decimal numbers to hex to be printed in info (see execute, line 82)
void stat(char *dirname);//Prints the attributes of the directory 
void volume();//Prints the name of the volume in the fat32 file system image
void readFile(char *dirname, int position, int numOfBytes);//Reads the bytes specified by the user in the file of their choice

int main()
{

    while (1)
    {
        getInput();
        execute();
    }
    return 0;
}

int LBAToOffset(int32_t sector)
{
    if (sector == 0)
        sector = 2;
    return ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec);
}

int16_t NextLB(uint32_t sector)
{
    uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
    int16_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 2, 1, fp);
    return val;
}

void getInput()
{
    printf(">> ");

    memset(cmd_str, '\0', MAX_COMMAND_SIZE);
    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(cmd_str, MAX_COMMAND_SIZE, stdin))
        ;
    /* Parse input */

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str = strdup(cmd_str);

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    memset(&token, '\0', MAX_NUM_ARGUMENTS);

    // Tokenize the input strings with whitespace used as the delimiter
    memset(&token, '\0', sizeof(MAX_NUM_ARGUMENTS));
    while (((arg_ptr = strsep(&working_str, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
        token[token_count] = strndup(arg_ptr, MAX_COMMAND_SIZE);
        if (strlen(token[token_count]) == 0)
        {
            token[token_count] = NULL;
        }
        token_count++;
    }

    free(working_root);
}

void execute()
{
    // If the user just hits enter, do nothing
    if (token[0] == NULL)
    {
        return;
    }

    if (strcmp(token[0], "open") == 0)
    {
        if (fp != NULL)
        {
            printf("Error: File system image already open.\n");
            return;
        }

        if (token[1] != NULL && fp == NULL)
        {
            openImage(token[1]);
        }
        else if (token[1] == NULL)
        {
            printf("ERR: Must give argument of file to open\n");
        }
        return;
    }
    else if (fp == NULL)
    {
        printf("Error: File system image must be opened first.\n");
        return;
    }
    else if (strcmp(token[0], "info") == 0)
    {
        printf("BPB_BytesPerSec: %d - ", BPB_BytesPerSec);
        decToHex(BPB_BytesPerSec);
        printf("\n");
        printf("BPB_SecPerClus: %d - ", BPB_SecPerClus);
        decToHex(BPB_SecPerClus);
        printf("\n");
        printf("BPB_RsvdSecCnt: %d - ", BPB_RsvdSecCnt);
        decToHex(BPB_RsvdSecCnt);
        printf("\n");
        printf("BPB_NumFATs: %d - ", BPB_NumFATs);
        decToHex(BPB_NumFATs);
        printf("\n");
        printf("BPB_FATSz32: %d - ", BPB_FATSz32);
        decToHex(BPB_FATSz32);
        printf("\n");
    }
    else if (strcmp(token[0], "ls") == 0)
    {
        printDirectory();
    }
    else if (strcmp(token[0], "cd") == 0)
    {
        if (token[1] == NULL)
        {
            printf("ERR: Please provide which directory you would like to open\n");
            return;
        }
        changeDirectory(getCluster(token[1]));
    }
    else if (strcmp(token[0], "get") == 0)
    {
        get(token[1]);
    }
    else if (strcmp(token[0], "stat") == 0)
    {
        stat(token[1]);
    }
    else if (strcmp(token[0], "volume") == 0)
    {
        volume();
    }
    else if (strcmp(token[0], "read") == 0)
    {
        if (token[1] == NULL || token[2] == NULL || token[3] == NULL)
        {
            printf("Please input valid arguments.\n");
            return;
        }
        readFile(token[1], atoi(token[2]), atoi(token[3]));
    }
    else if (strcmp(token[0], "close") == 0)
    {
        closeImage();
    }
}

void openImage(char file[])
{
    fp = fopen(file, "r");
    if (fp == NULL)
    {
        printf("Image does not exist\n");
        return;
    }
    printf("%s opened.\n", file);

    fseek(fp, 3, SEEK_SET);
    fread(&BS_OEMName, 8, 1, fp);

    fseek(fp, 11, SEEK_SET);
    fread(&BPB_BytesPerSec, 2, 1, fp);
    fread(&BPB_SecPerClus, 1, 1, fp);
    fread(&BPB_RsvdSecCnt, 2, 1, fp);
    fread(&BPB_NumFATs, 1, 1, fp);
    fread(&BPB_RootEntCnt, 2, 1, fp);

    fseek(fp, 36, SEEK_SET);
    fread(&BPB_FATSz32, 4, 1, fp);

    fseek(fp, 44, SEEK_SET);
    fread(&BPB_RootClus, 4, 1, fp);
    currentDirectory = BPB_RootClus;

    int offset = LBAToOffset(currentDirectory);
    fseek(fp, offset, SEEK_SET);
    fread(&dir[0], 32, 16, fp);
}

void get(char *dirname)
{
    char *dirstring = (char *)malloc(strlen(dirname));
    strncpy(dirstring, dirname, strlen(dirname));
    int cluster = getCluster(dirstring);
    int size = getSizeOfCluster(cluster);
    FILE *newfp = fopen(token[1], "w");
    fseek(fp, LBAToOffset(cluster), SEEK_SET);
    unsigned char *ptr = malloc(size);
    fread(ptr, size, 1, fp);
    fwrite(ptr, size, 1, newfp);
    fclose(newfp);
}

void formatDirectory(char *dirname)
{
    char expanded_name[12];
    memset(expanded_name, ' ', 12);

    char *token = strtok(dirname, ".");

    if (token)
    {
        strncpy(expanded_name, token, strlen(token));

        token = strtok(NULL, ".");

        if (token)
        {
            strncpy((char *)(expanded_name + 8), token, strlen(token));
        }

        expanded_name[11] = '\0';

        int i;
        for (i = 0; i < 11; i++)
        {
            expanded_name[i] = toupper(expanded_name[i]);
        }
    }
    else
    {
        strncpy(expanded_name, dirname, strlen(dirname));
        expanded_name[11] = '\0';
    }
    strncpy(formattedDirectory, expanded_name, 12);
}

void volume()
{
    fseek(fp, 71, SEEK_SET);
    fread(&BPB_Volume, 11, 1, fp);
    printf("Volume name: %s\n", BPB_Volume);
}

int32_t getCluster(char *dirname)
{
    //Compare dirname to directory name (attribute), if same, cd into FirstClusterLow
    formatDirectory(dirname);

    int i;
    for (i = 0; i < 16; i++)
    {
        char *directory = malloc(11);
        memset(directory, '\0', 11);
        memcpy(directory, dir[i].DIR_Name, 11);

        if (strncmp(directory, formattedDirectory, 11) == 0)
        {
            int cluster = dir[i].DIR_FirstClusterLow;
            return cluster;
        }
    }

    return -1;
}

int32_t getSizeOfCluster(int32_t cluster)
{
    int i;
    for (i = 0; i < 16; i++)
    {
        if (cluster == dir[i].DIR_FirstClusterLow)
        {
            int size = dir[i].DIR_FileSize;
            return size;
        }
    }
    return -1;
}

void stat(char *dirname)
{
    int cluster = getCluster(dirname);
    int size = getSizeOfCluster(cluster);
    printf("Size: %d\n", size);
    int i;
    for (i = 0; i < 16; i++)
    {
        if (cluster == dir[i].DIR_FirstClusterLow)
        {
            printf("Attr: %d\n", dir[i].DIR_Attr);
            printf("Starting Cluster: %d\n", cluster);
            printf("Cluster High: %d\n", dir[i].DIR_FirstClusterHigh);
        }
    }
}

void changeDirectory(int32_t cluster)
{
    if (strcmp(token[1], "..") == 0)
    {
        int i;
        for (i = 0; i < 16; i++)
        {
            if (strncmp(dir[i].DIR_Name, "..", 2) == 0)
            {
                int offset = LBAToOffset(dir[i].DIR_FirstClusterLow);
                currentDirectory = dir[i].DIR_FirstClusterLow;
                fseek(fp, offset, SEEK_SET);
                fread(&dir[0], 32, 16, fp);
                return;
            }
        }
    }
    int offset = LBAToOffset(cluster);
    currentDirectory = cluster;
    fseek(fp, offset, SEEK_SET);
    fread(&dir[0], 32, 16, fp);
}

void getDirectoryInfo()
{
    int i;
    for (i = 0; i < 16; i++)
    {
        fread(&dir[i], 32, 1, fp);
    }
}

void readFile(char *dirname, int position, int numOfBytes)
{
    int cluster = getCluster(dirname);
    int offset = LBAToOffset(cluster);
    fseek(fp, offset + position, SEEK_SET);
    char *bytes = malloc(numOfBytes);
    fread(bytes, numOfBytes, 1, fp);
    printf("%s\n", bytes);
}

// ls
void printDirectory()
{
    if (fp == NULL)
    {
        printf("No image is opened\n");
        return;
    }

    int offset = LBAToOffset(currentDirectory);
    fseek(fp, offset, SEEK_SET);

    int i;
    for (i = 0; i < 16; i++)
    {
        fread(&dir[i], 32, 1, fp);

        if ((dir[i].DIR_Name[0] != (char)0xe5) &&
            (dir[i].DIR_Attr == 0x1 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20))
        {
            char *directory = malloc(11);
            memset(directory, '\0', 11);
            memcpy(directory, dir[i].DIR_Name, 11);
            printf("%s\n", directory);
        }
    }
}

void decToHex(int dec)
{
    char hex[100];
    int i = 1;
    int j;
    int temp;
    while (dec != 0)
    {
        temp = dec % 16;
        if (temp < 10)
        {
            temp += 48;
        }
        else
        {
            temp += 55;
        }
        hex[i++] = temp;
        dec /= 16;
    }
    for (j = i - 1; j > 0; j--)
    {
        printf("%c", hex[j]);
    }
}

void closeImage()
{
    if (fp == NULL)
    {
        printf("Error: File system not open.");
        return;
    }

    fclose(fp);
    fp = NULL;
}