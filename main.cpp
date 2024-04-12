#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FAT12.h"

char img[1474560];

int main() {
    FILE* fp;
    
    const char* location = "dossys.img";
    fopen_s(&fp, location, "r+");       

    if (fp == NULL) {
        printf("Can't open the .img file\n");
        return 0;
    }
    else
        printf("Starting MS-DOS...\n");
    fread(img, sizeof(char), sizeof(img), fp);
    fclose(fp);
    
    char header[60] = "A:\\>";
    char cmd[60] = "";
    char param[60] = "";
    char* text = NULL;

    node roots[224];
    int cnt = tree(roots, img);     
    node* now = roots;

    while (1) {
        printf(header);
        gets_s(cmd, 60);            
        if (!strcmp(cmd, "")) {
            printf("\n");
            continue;
        }

        _strupr_s(cmd);
        strtok_s(cmd, " ", &text);
        if (text)
            strcpy_s(param, text);

        if (!strcmp(cmd, "MBR")) {      
            char mbr[512];
            read_block(img, mbr, 0, 0);
            PrintMBR(mbr);
        }
        else if (!strcmp(cmd, "FAT12"))
            fat12(img, sizeof(img));
        else if (!strcmp(cmd, "CD"))
            strcpy_s(header, cd(param, header, &now, roots, cnt));
        else if (!strcmp(cmd, "DIR"))
            dir(img, param, header, now, roots, cnt);
        else if (!strcmp(cmd, "MD"))
            mkdir(img, param, roots, cnt, now);
        else if (!strcmp(cmd, "RD"))
            rmdir(img, param, roots, cnt, now);
        else if (!strcmp(cmd, "TREE"))
            show_tree(roots, cnt, now);
        else if (!strcmp(cmd, "TYPE"))
            type(img, param, roots, cnt, now);
        else if (!strcmp(cmd, "COPY"))
            copy(img, param, roots, cnt, now);
        else if (!strcmp(cmd, "DEL"))
            del(img, param, roots, cnt, now);
        else if (!strcmp(cmd, "HELP"))
            help();
        else if (!strcmp(cmd, "CLS"))
            system("cls");
        else if (!strcmp(cmd, "EXIT"))
            break;
        else {
            printf("Bad command or file name\n");
            printf("Input 'help' for more infomation\n");
        }

        if (strcmp(cmd, "CLS"))
            printf("\n");
    }

    clear_tree(roots, cnt);     
    fopen_s(&fp, location, "rb+");
    fwrite(img, sizeof(char), sizeof(img), fp);     
    fclose(fp);
    return 0;
}
