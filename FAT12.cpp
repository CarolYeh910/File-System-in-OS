#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <stdlib.h>
#include "FAT12.h"

void PrintMBR(char* MBR) {
    Fat12MBR rf;
    memcpy(reinterpret_cast<char*>(&rf), MBR, sizeof(rf));

    rf.BS_VolLab[10] = 0;
    rf.BS_FileSysType[7] = 0;

    printf("BS_OEMName: %s\n", rf.BS_OEMName);
    printf("BPB_BytsPerSec: 0x%X\n", rf.BPB_BytsPerSec);
    printf("BPB_SecPerClus: 0x%X\n", rf.BPB_SecPerClus);
    printf("BPB_RsvdSecCnt: 0x%X\n", rf.BPB_RsvdSecCnt);
    printf("BPB_NumFATs: 0x%X\n", rf.BPB_NumFATs);
    printf("BPB_RootEntCnt: 0x%X\n", rf.BPB_RootEntCnt);
    printf("BPB_TotSec16: 0x%X\n", rf.BPB_TotSec16);
    printf("BPB_Media: 0x%X\n", rf.BPB_Media);
    printf("BPB_FATSz16: 0x%X\n", rf.BPB_FATSz16);
    printf("BPB_SecPerTrk: 0x%X\n", rf.BPB_SecPerTrk);
    printf("BPB_NumHeads: 0x%X\n", rf.BPB_NumHeads);
    printf("BPB_HiddSec: %u\n", rf.BPB_HiddSec);
    printf("BPB_TotSec32: %u\n", rf.BPB_TotSec32);
    printf("BS_DrvNum: %u\n", rf.BS_DrvNum);
    printf("BS_Reserved1: %u\n", rf.BS_Reserved1);
    printf("BS_BootSig: 0x%X\n", rf.BS_BootSig);
    printf("BS_VolID: 0x%X\n", rf.BS_VolID);
    printf("BS_VolLab: %s\n", rf.BS_VolLab);
    printf("BS_FileSysType: %s\n", rf.BS_FileSysType);

    unsigned char b510 = MBR[510];
    unsigned char b511 = MBR[511];
    printf("Byte 510: 0x%X\n", b510);
    printf("Byte 511: 0x%X\n", b511);
}

void read_block(char* img, char* block, int num, bool cls) {
    if (cls)            
        num += 31;
    char* pos = img + num * 512;
    memcpy(block, pos, 512);
}

void fat12(char* img, int size) {
    char block1[512], block2[512];
    read_block(img, block1, 0, 0);
    Fat12MBR rf;
    memcpy(reinterpret_cast<char*>(&rf), block1, sizeof(rf));

    bool fat = true;
    
    if (strncmp(rf.BS_FileSysType, "FAT12   ", 8) || rf.BPB_NumFATs != 2 || 
        rf.BPB_SecPerClus != 1 || rf.BPB_BytsPerSec * rf.BPB_TotSec16 != size)
        fat = false;

    int init = rf.BPB_RsvdSecCnt;
    int num = rf.BPB_FATSz16;
    for (int i = 0; i < num; i++) {     
        read_block(img, block1, init + i, 0);
        read_block(img, block2, init + i + num, 0);
        if (strncmp(block1, block2, 512))
            fat = false;
    }

    if (fat)
        printf("It's a FAT12 file system.\n");
    else
        printf("It's not a FAT12 file system.\n");
}

int find_roots(node* roots, char* img) {
    char root_dir_block[512];
    int block_num = 19;
    read_block(img, root_dir_block, block_num, 0);
    int cnt = 0;

    
    while (block_num < 33 && root_dir_block[0] != 0) {
        
        for (int i = 0; i < 16 && root_dir_block[i * 32] != 0; i++, cnt++) {
            char* pos = root_dir_block + i * 32;
            memcpy(reinterpret_cast<char*>(&(roots[cnt].obj)), pos, 32);

            if (roots[cnt].obj.DIR_Attr == 0x10)
                roots[cnt].dir = true;
            roots[cnt].offset = block_num * 512 + i * 32;
            roots[cnt].prev = roots;
        }

        read_block(img, root_dir_block, ++block_num, 0);
    }

    return cnt;
}

void find_sub_entry(node* root, char* img) {
    int chain[3], num = 1;
    chain[0] = root->obj.DIR_FstClus;
    char dir_block[512];

    node* pre = root;
    read_fat(img, chain, num);      

    
    for (int k = 0; k < num; k++) {
        read_block(img, dir_block, chain[k], 1);

        
        for (int i = 2; i < 16 && dir_block[i * 32] != 0; i++) {
            char* pos = dir_block + i * 32;
            node* cur = (node*)malloc(sizeof(node));
            memcpy(reinterpret_cast<char*>(&(cur->obj)), pos, 32);

            if (cur->obj.DIR_Attr == 0x10)
                cur->dir = true;
            else
                cur->dir = false;
            cur->offset = (chain[k] + 31) * 512 + i * 32;
            cur->prev = root;
            cur->next = NULL;
            cur->down = NULL;

            if (i == 2)     
                pre->next = cur;
            else
                pre->down = cur;
            
            pre = cur;
            if (cur->dir && cur->obj.DIR_Name[0] != -27)
                find_sub_entry(cur, img);
        }
    }
}

int tree(node* roots, char* img) {
    int cnt = find_roots(roots, img);

    
    for (int i = 0; i < cnt; i++)
        if (roots[i].dir && roots[i].obj.DIR_Name[0] != -27)
            find_sub_entry(&(roots[i]), img);

    return cnt;
}

void PrintEntry(entry obj, int& num, int& size) {       
    if (obj.DIR_Attr == 0x10)
        obj.DIR_Name[10] = 0;
    if (obj.DIR_Attr == 0x27 || obj.DIR_Name[0] == -27)
        return;

    printf("%s ", obj.DIR_Name);
    if (obj.DIR_Attr == 0x10)
        printf(" <DIR>        ");
    else
        printf("     %6d ", obj.DIR_FileSize);

    date(obj.DIR_WrtDate);
    time(obj.DIR_WrtTime);

    num++;
    size += obj.DIR_FileSize;
}

void file_name(node* tmp) {     
    char filename[13];
    int l = 0;
    for (; l < 8; l++) {
        if (tmp->obj.DIR_Name[l] != 0x20)
            filename[l] = tmp->obj.DIR_Name[l];
        else {
            if (!tmp->next || tmp->next->obj.DIR_Name[0] == -27)
                break;
            filename[l] = '-';
        }
    }
    filename[l] = 0;

    if (tmp->obj.DIR_Name[8] != 0x20) {
        strcat_s(filename, ".");
        l++;
        for (int i = 8; i < 11; i++, l++)
            filename[l] = tmp->obj.DIR_Name[i];
        filename[l] = 0;
    }

    printf("%s", filename);
}

void dfs(node* node, int depth) {       
    if (!node)
        return;
    if (node->obj.DIR_Name[0] != -27)
        file_name(node);

    if (node->next)
        dfs(node->next, depth + 1);
    if (!node->down)
        return;

    
    if (node->down->obj.DIR_Name[0] != -27) {
        printf("\n");
        for (int i = 0; i < depth - 1; i++)
            printf("||\t");
        printf("||\n");
        for (int i = 0; i < depth - 1; i++)
            printf("||\t");
        printf("++------");
    }
    dfs(node->down, depth);
}

void show_tree(node* roots, int cnt, node* now) {       
    if (now != roots) {
        file_name(now);
        dfs(now->next, 1);
        printf("\n||\n++\n");
        return;
    }

    for (int i = 0; i < cnt; i++) {
        if (roots[i].obj.DIR_Attr != 0x27 && roots[i].obj.DIR_Name[0] != -27) {
            if (roots[i].dir && roots[i].next)
                dfs(&(roots[i]), 0);
            else
                file_name(&(roots[i]));
            printf("\n||\n");
        }
    }
    printf("++\n");
}

node* path_analyze(char* param, node* roots, int cnt, node* now) {  
    if (!strcmp(param, "\\"))       
        return roots;
    else if (!strcmp(param, ".."))  
        return now->prev;
    else {
        node* cur;
        char* text;
        char path[60];
        if (param[0] == '\\') {     
            cur = roots;
            strcpy_s(path, param + 1);
        }
        else {          
            cur = now;
            strcpy_s(path, param);
        }

        strtok_s(path, "\\", &text);    
        
        while (strlen(path)) {
            if (cur == roots) {         
                int i = 0;
                for (; i < cnt; i++) {
                    int l = 0;
                    for (; l < 8; l++)
                        if (roots[i].obj.DIR_Name[l] == 0x20)
                            break;

                    if (roots[i].dir && !strncmp(roots[i].obj.DIR_Name, path, l))
                        break;
                }

                if (i == cnt)
                    return NULL;
                else
                    cur = &(roots[i]);
            }
            else {          
                node* tmp = cur->next;
                while (tmp) {
                    int l = 0;
                    for (; l < 8; l++)
                        if (tmp->obj.DIR_Name[l] == 0x20)
                            break;

                    if (tmp->dir && !strncmp(tmp->obj.DIR_Name, path, l))
                        break;
                    tmp = tmp->down;
                }

                if (tmp == NULL) {
                    return NULL;
                }
                else
                    cur = tmp;
            }

            if (strlen(text) == 0)
                path[0] = '\0';
            else {
                strcpy_s(path, text);
                strtok_s(path, "\\", &text);
            }
        }
        return cur;     
    }
}

char tmp[60];


void new_header(char* param, char* header, node* roots, node* cur) {
    if (!strcmp(param, "\\") || cur==roots)     
        strcpy_s(tmp, "A:\\>");
    else if (!strcmp(param, "..")) {        
        strcpy_s(tmp, header);
        for (int i = strlen(tmp) - 1; i >= 0; i--) {
            if (tmp[i] == '\\') {
                tmp[i] = '>';
                tmp[i + 1] = '\0';
                break;
            }
        }
    }
    else if (!strcmp(param, ""))        
        strcpy_s(tmp, header);
    else if (param[0] == '\\') {        
        strcpy_s(tmp, "A:");
        strcat_s(tmp, param);
        strcat_s(tmp, ">");
    }
    else {
        strcpy_s(tmp, header);

        int pos = strlen(tmp) - 1;      
        if (tmp[pos - 1] != '\\') {
            tmp[pos] = '\\';
            tmp[pos + 1] = '\0';
        }
        else
            tmp[pos] = '\0';

        strcat_s(tmp, param);
        strcat_s(tmp, ">");
    }
}

char* cd(char* param, char* header, node** now, node* roots, int cnt) {
    if (!strcmp(param, "")) {
        printf("Lack of argument\n");
        return header;
    }

    
    node* cur = path_analyze(param, roots, cnt, *now);

    if (cur) {
        *now = cur;     
        new_header(param, header, roots, cur);
        return tmp;
    }
    else {          
        printf("Invalid directory\n");
        return header;
    }
}


void introduction(char*mbr, char* param, char* header, node* roots, node* cur) {
    int VolID;
    char VolLab[11];
    memcpy(&VolID, mbr + 39, 4);
    memcpy(VolLab, mbr + 43, 11);
    VolLab[10] = 0;

    printf("\n Volume in drive A has ");
    if (!strncmp(VolLab, "NO NAME", 7))
        printf("no label\n");
    else
        printf("label %s\n", VolLab);

    printf(" Volume Serial Number is %X-%X\n", VolID / 65536, VolID % 65536);
    
    new_header(param, header, roots, cur);
    int l = strlen(tmp);
    tmp[l - 1] = '\0';
    printf(" Directory of %s\n\n", tmp);
}


void first_two_dir(node* tmp, int& num, int& size) {
    entry cur, pre;
    memcpy(&cur, &(tmp->obj), sizeof(entry));
    strcpy_s(cur.DIR_Name, ".         ");
    memcpy(&pre, &(tmp->obj), sizeof(entry));

    if (tmp->prev)
        pre.DIR_FstClus = tmp->prev->obj.DIR_FstClus;
    else
        pre.DIR_FstClus = 0;
    strcpy_s(pre.DIR_Name, "..        ");

    PrintEntry(cur, num, size);
    PrintEntry(pre, num, size);
}

void dir(char* img, char* param, char* header, node* now, node* roots, int cnt) {
    node* tmp;
    if (!strcmp(param, ""))   
        tmp = now;
    else                    
        tmp = path_analyze(param, roots, cnt, now);

    if (tmp) {
        int num = 0, size = 0;
        char mbr[512];
        read_block(img, mbr, 0, 0);
        introduction(mbr, param, header, roots, tmp);

        
        if (tmp == roots) {
            for (int i = 0; i < cnt; i++)
                PrintEntry(roots[i].obj, num, size);
        }
        else {
            first_two_dir(tmp, num, size);
            node* t = tmp->next;
            while (t) {
                PrintEntry(t->obj, num, size);
                t = t->down;
            }
        }
        
        printf("%8d file(s)%14d bytes\n", num, size);
    }
    else
        printf("Path not found\n");
}

char name[60];

void mkdir(char* img, char* param, node* roots, int &cnt, node* now) {
    node* same = path_analyze(param, roots, cnt, now);
    node* dir = find_dir(param, roots, cnt, now);
    node* tmp = dir->next;

    if (same || !dir) {     
        printf("Bad command or file name\n");
        return;
    }

    if (dir == roots) {
        int pos;
        edit_roots(img, roots, cnt, pos, 0);    
        roots[pos].dir = true;
        tmp = &(roots[pos]);
    }
    else {              
        while (tmp && tmp->obj.DIR_Name[0] != -27)
            tmp = tmp->down;

        if (tmp) {      
            tmp->obj.DIR_FileSize = 0;
            edit_entry(img, tmp);   
        }
        else {
            tmp = (node*)malloc(sizeof(node));
            memset(&(tmp->obj), 0, sizeof(entry));
            edit_sub_dir(img, dir, tmp);    
        }
        tmp->dir = true;
    }
    
    edit_empty_dir(img, tmp, tmp->obj.DIR_FstClus);
}


void edit_roots(char* img, node* roots, int &cnt, int &pos, int size) {
    for (pos = 0; pos < cnt; pos++) {
        if (roots[pos].obj.DIR_Name[0] == -27) {    
            roots[pos].obj.DIR_FileSize = size;
            edit_entry(img, &(roots[pos]));         
            break;
        }
    }
    if (pos == cnt) {       
        memset(&(roots[cnt].obj), 0, sizeof(entry));
        roots[cnt].obj.DIR_FileSize = size;
        roots[cnt].offset = roots[cnt - 1].offset + 32;
        edit_entry(img, &(roots[cnt]));
        cnt++;
    }
}


void edit_sub_dir(char* img, node* dir, node* tmp) {
    node* pre = NULL, * cur = dir->next;
    int pos = 0;
    while (cur && cur->obj.DIR_Name[0] != -27) {
        if (cur->offset > pos)
            pos = cur->offset;      
        if (cur->down == NULL)
            pre = cur;          
        cur = cur->down;
    }

    if (pos) {
        tmp->offset = pos + 32;
        if (tmp->offset % 512 == 0) {       
            int chain[3], num = 1;
            chain[0] = dir->obj.DIR_FstClus;
            read_fat(img, chain, num);
            find_0_fat(img, &(chain[num]), 1);
            edit_fat(img, &(chain[num - 1]), 2);
            tmp->offset = (chain[num] + 31) * 512;
        }
    }
    else      
        tmp->offset = (dir->obj.DIR_FstClus + 31) * 512 + 64;
    edit_entry(img, tmp);

    tmp->prev = dir;
    tmp->next = NULL;
    tmp->down = NULL;

    if (pre)
        pre->down = tmp;
    else
        dir->next = tmp;
}

void edit_empty_dir(char* img, node* tmp, int cls_num) {
    entry cur, pre;
    memcpy(&cur, &(tmp->obj), sizeof(entry));
    strcpy_s(cur.DIR_Name, ".         ");       
    cur.DIR_Name[10] = ' ';
    memcpy(&pre, &cur, sizeof(entry));

    if (tmp->prev)
        pre.DIR_FstClus = tmp->prev->obj.DIR_FstClus;
    else
        pre.DIR_FstClus = 0;
    pre.DIR_Name[1] = '.';      

    char block[512];
    memset(block, 0, 512);
    memcpy(block, reinterpret_cast<char*>(&cur), sizeof(entry));
    memcpy(block + 32, reinterpret_cast<char*>(&pre), sizeof(entry));
    write_block(img, block, cls_num, 1);    
}

void copy(char* img, char* param, node* roots, int& cnt, node* now) {
    char* f1 = param, * f2;
    strtok_s(param, " ", &f2);
    char des_copy[30];
    strcpy_s(des_copy, f2);

    char buffer[10240];
    memset(buffer, 0, sizeof(buffer));
    int length = 0;

    if (!strncmp(f1, "CON", 3))     
        input(buffer, sizeof(buffer), length);
    else {
        node* dir1 = find_dir(f1, roots, cnt, now);
        node* file1 = NULL;
        if (dir1)
            file1 = find_file(roots, cnt, dir1);

        if (file1) {    
            int chain[30], num = 1;
            char* pos = buffer;

            chain[0] = file1->obj.DIR_FstClus;
            length = file1->obj.DIR_FileSize;
            read_fat(img, chain, num);      

            for (int k = 0; k < num; pos += 512, k++)
                read_block(img, pos, chain[k], 1);      

            char* end = buffer + file1->obj.DIR_FileSize;
            memset(end, 0, sizeof(buffer) + buffer - end);  
        }
        else {
            printf("Source file not found\n");
            return;
        }
    }

    node* same = path_analyze(f2, roots, cnt, now);
    if (same && same->dir) {
        printf("Invalid path\n");
        return;
    }

    node* dir2 = find_dir(f2, roots, cnt, now);
    char fname[30];
    strcpy_s(fname, name);

    if (dir2) {
        node* file2 = find_file(roots, cnt, dir2), * tmp = dir2->next;
        if (file2)      
            del(img, des_copy, roots, cnt, now);
        strcpy_s(name, fname);

        if (dir2 == roots) {    
            int pos;
            edit_roots(img, roots, cnt, pos, length);
            roots[pos].dir = false;
            tmp = &(roots[pos]);
        }
        else {      
            while (tmp && tmp->obj.DIR_Name[0] != -27)
                tmp = tmp->down;

            if (tmp) {      
                tmp->obj.DIR_FileSize = length;
                edit_entry(img, tmp);
            }
            else {          
                tmp = (node*)malloc(sizeof(node));
                memset(&(tmp->obj), 0, sizeof(entry));
                tmp->obj.DIR_FileSize = length;
                edit_sub_dir(img, dir2, tmp);
            }
            tmp->dir = false;
        }

        
        int chain[30], num = 1;
        chain[0] = tmp->obj.DIR_FstClus;
        read_fat(img, chain, num);

        
        for (int i = 0; i < num; i++)
            write_block(img, buffer + 512 * i, chain[i], 1);

        printf("%5d file(s) copied\n", 1);
    }
    else
        printf("Invalid path\n");
}

void input(char* buffer, int size, int &length) {
    gets_s(buffer, size);
    length = strlen(buffer);
    char* pos = buffer;

    while (length < size && buffer[length - 1] != 26) {  
        buffer[length] = 0xd;
        buffer[length + 1] = 0xa;
        length += 2;
        
        if (length < size) {
            pos = buffer + length;
            gets_s(pos, size - length);
            length += strlen(pos);
        }
    }

    length--;
    if (length % 512) {     
        srand((unsigned)time((time_t*)NULL));
        for (int i = length; i % 512; i++)
            buffer[i] = rand() % 256;
    }
}

void rmdir(char* img, char* param, node* roots, int cnt, node* now) {
    node* cur = path_analyze(param, roots, cnt, now);
    if (cur && !cur->next) {
        cur->obj.DIR_Name[0] = 0xe5;    
        empty_fat(img, cur->obj.DIR_FstClus);   

        char block[512];
        read_block(img, block, (cur->offset) / 512, 0);
        block[cur->offset % 512] = 0xe5;
        write_block(img, block, (cur->offset) / 512, 0);    
    }
    else {
        printf("Invalid path, not directory,\n");
        printf("or directory not empty\n");
    }
}

void read_fat(char* img, int* chain, int &num) {
    int pos = 1.5 * chain[0];
    int cls = chain[0], blk_num = pos / 512 + 1;
    unsigned char block[512];
    read_block(img, (char*)block, blk_num, 0);

    while (cls != 0xfff) {      
        if (blk_num != pos / 512 + 1) {     
            blk_num = pos / 512 + 1;
            read_block(img, (char*)block, blk_num, 0);
        }
        
        int j = pos % 512;
        if (j == 511) {     
            if (blk_num % 3 == 1) {     
                cls = block[j] / 16;
                read_block(img, (char*)block, ++blk_num, 0);
                cls += block[0] * 16;
            }
            else if (blk_num % 3 == 2) {    
                cls = block[j];
                read_block(img, (char*)block, ++blk_num, 0);
                cls += (block[0] % 16) * 256;
            }
        }
        else if (cls % 2)   
            cls = block[j] / 16 + block[j + 1] * 16;
        else                
            cls = block[j] + (block[j + 1] % 16) * 256;

        if (cls != 0xfff) {
            chain[num] = cls;
            num++;
            pos = 1.5 * cls;    
        }
    }
}

void empty_fat(char* img, int init) {
    int pos = 1.5 * init, next = 0;
    int cls = init, blk_num = pos / 512 + 1;
    unsigned char block[512];
    read_block(img, (char*)block, blk_num, 0);

    while (next != 0xfff) {     
        if (blk_num != pos / 512 + 1) {
            blk_num = pos / 512 + 1;
            read_block(img, (char*)block, blk_num, 0);
        }

        
        int j = pos % 512;
        if (j == 511) {
            if (blk_num % 3 == 1) {
                next = block[j] / 16;
                block[j] %= 16;
                write_block(img, (char*)block, blk_num, 0);
                read_block(img, (char*)block, ++blk_num, 0);
                next += block[0] * 16;
                block[0] = 0;
            }
            else if (blk_num % 3 == 2) {
                next = block[j];
                block[j] = 0;
                write_block(img, (char*)block, blk_num, 0);
                read_block(img, (char*)block, ++blk_num, 0);
                next += (block[0] % 16) * 256;
                block[0] -= block[0] % 16;
            }
        }
        else if (cls % 2) {
            next = block[j] / 16 + block[j + 1] * 16;
            block[j] %= 16;
            block[j + 1] = 0;
        }
        else {
            next = block[j] + (block[j + 1] % 16) * 256;
            block[j] = 0;
            block[j + 1] -= block[j + 1] % 16;
        }

        
        write_block(img, (char*)block, blk_num, 0);
        cls = next;
        pos = 1.5 * cls;    
    }

    
    memcpy(img + 5120, img + 512, 512 * 9);
}

void find_0_fat(char* img, int* chain, int num) {
    int cls=0x9d, pos = 1.5 * cls;
    int blk_num = pos / 512 + 1, next = cls;
    unsigned char block[512];
    read_block(img, (char*)block, blk_num, 0);
    
    for (int i = 0; i < num; i++) {
        while (next) {
            if (blk_num != pos / 512 + 1) {
                blk_num = pos / 512 + 1;
                read_block(img, (char*)block, blk_num, 0);
            }
            
            
            int j = pos % 512;
            if (j == 511) {
                if (blk_num % 3 == 1) {
                    next = block[j] / 16;
                    read_block(img, (char*)block, ++blk_num, 0);
                    next += block[0] * 16;
                }
                else if (blk_num % 3 == 2) {
                    next = block[j];
                    read_block(img, (char*)block, ++blk_num, 0);
                    next += (block[0] % 16) * 256;
                }
            }
            else if (cls % 2)
                next = block[j] / 16 + block[j + 1] * 16;
            else
                next = block[j] + (block[j + 1] % 16) * 256;

            if (next) {     
                if (next == 0xfff)
                    cls++;
                else
                    cls = next;
                pos = 1.5 * cls;
            }
            else
                chain[i] = cls;     
        }
    }
}

void edit_fat(char* img, int* chain, int num) {
    unsigned char block[512];

    for (int i = 0; i < num; i++) {
        int cls = chain[i], pos = 1.5 * cls;
        int blk_num = pos / 512 + 1, next;
        read_block(img, (char*)block, blk_num, 0);

        if (i == num - 1)   
            next = 0xfff;
        else
            next = chain[i + 1];    

        
        int j = pos % 512;
        if (j == 511) {
            if (blk_num % 3 == 1) {
                block[j] += (next % 16) * 16;
                write_block(img, (char*)block, blk_num, 0);
                read_block(img, (char*)block, ++blk_num, 0);
                block[0] = next / 16;
            }
            else if (blk_num % 3 == 2) {
                block[j] = next % 256;
                write_block(img, (char*)block, blk_num, 0);
                read_block(img, (char*)block, ++blk_num, 0);
                block[0] += next / 256;
            }
        }
        else if (cls % 2) {
            block[j] += (next % 16) * 16;
            block[j + 1] = next / 16;
        }
        else {
            block[j] = next % 256;
            block[j + 1] += next / 256;
        }

        
        write_block(img, (char*)block, blk_num, 0);
    }

    
    memcpy(img + 5120, img + 512, 512 * 9);
}

node* find_dir(char* param, node* roots, int cnt, node* now) {
    int pos = strlen(param) - 1;
    for (; pos >= 0; pos--)
        if (param[pos] == '\\')     
            break;
    pos++;
    strcpy_s(name, param + pos);    

    node* dir;
    if (pos > 1) {
        param[pos - 1] = '\0';
        char path[50];
        strcpy_s(path, param);
        dir = path_analyze(path, roots, cnt, now);  
    }
    else if (pos == 1)      
        dir = roots;
    else                    
        dir = now;

    return dir;
}

node* find_file(node* roots, int cnt, node* dir) {
    node* file = NULL;
    char* suffix, * origin;
    strtok_s(name, ".", &suffix);   
    if (strlen(suffix) == 0)
        return NULL;

    int l = strlen(name);
    if (l > 8)
        l = 8;

    if (dir == roots) {     
        for (int i = 0; i < cnt; i++) {
            origin = roots[i].obj.DIR_Name;
            
            if (!strncmp(name, origin, l) && !strncmp(suffix, origin+8, 3)) {
                file = &(roots[i]);
                break;
            }
        }
    }
    else {
        file = dir->next;
        while (file) {      
            origin = file->obj.DIR_Name;
            
            if (!strncmp(name, origin, l) && !strncmp(suffix, origin+8, 3))
                break;
            file = file->down;
        }
    }

    return file;
}

void type(char* img, char* param, node* roots, int cnt, node* now) {
    char copy[60];
    strcpy_s(copy, param);
    node* dir = find_dir(param, roots, cnt, now);   

    node* file = NULL;
    if (dir)
        file = find_file(roots, cnt, dir);      

    if (!file)
        printf("File not found - %s\n", copy);  
    else {
        int size = file->obj.DIR_FileSize;
        char block[513];

        int chain[30], num = 1;
        chain[0] = file->obj.DIR_FstClus;
        read_fat(img, chain, num);      

        
        for (int i = 0, k=0; k < num; i += 512, k++) {
            read_block(img, block, chain[k], 1);
            if (i + 512 <= size)
                block[512] = 0;
            else       
                block[size - i] = 0;
            printf("%s", block);
        }
    }
}

void del(char* img, char* param, node* roots, int cnt, node* now) {
    node* dir = find_dir(param, roots, cnt, now);   
    node* file;
    if (dir)
        file = find_file(roots, cnt, dir);      
    else
        file = NULL;

    if (file) {
        file->obj.DIR_Name[0] = 0xe5;       
        empty_fat(img, file->obj.DIR_FstClus);  

        char block[512];
        read_block(img, block, (file->offset) / 512, 0);
        block[file->offset % 512] = 0xe5;
        write_block(img, block, (file->offset) / 512, 0);   
    }
    else
        printf("File not found\n");
}

void edit_entry(char* img, node* tmp) {
    char* text;
    strtok_s(name, ".", &text); 
    if (strlen(name) > 8)       
        name[8] = 0;
    strcpy_s(tmp->obj.DIR_Name, name);

    if (strlen(text) == 0) {    
        tmp->obj.DIR_Attr = 0x10;
        for (int i = strlen(name); i < 11; i++)
            tmp->obj.DIR_Name[i] = 0x20;
    }
    else {
        tmp->obj.DIR_Attr = 0x20;
        for (int i = strlen(name); i < 8; i++)
            tmp->obj.DIR_Name[i] = 0x20;
        for (int i = 8; i < 11; i++)
            tmp->obj.DIR_Name[i] = text[i-8];   
    }
    
    int chain[30];
    int num = tmp->obj.DIR_FileSize / 512;
    if (tmp->obj.DIR_FileSize % 512 || num == 0)    
        num++;
    find_0_fat(img, chain, num);    
    edit_fat(img, chain, num);      
    tmp->obj.DIR_FstClus = chain[0];

    getdate(tmp->obj.DIR_WrtDate);  
    gettime(tmp->obj.DIR_WrtTime);  

    char block[512];
    read_block(img, block, tmp->offset / 512, 0);
    memcpy(block + tmp->offset % 512, reinterpret_cast<char*>(&tmp->obj), sizeof(entry));
    write_block(img, block, tmp->offset / 512, 0);      
}

void write_block(char* img, char* block, int num, bool cls) {
    if (cls)        
        num += 31;
    char* pos = img + num * 512;
    memcpy(pos, block, 512);
}

void clear(node* node) {    
    if (!node)
        return;
    if (node->next)
        clear(node->next);
    if (node->down)
        clear(node->down);
    free(node);
}

void clear_tree(node* roots, int cnt) {     
    for (int i = 0; i < cnt; i++)
        if (roots[i].dir && roots[i].next)
            clear(roots[i].next);
}

void date(unsigned short days) {    
    int day = days%32;
    days /= 32;

    int month = days % 16;
    days /= 16;

    int year = 1980 + days;
    printf("%02d-%02d-%02d  ", month, day, year%100);
}

void time(unsigned short time) {    
    int min = time / 32;
    int hour = min / 64;
    min %= 64;

    if (hour == 12)
        printf("12:%02dp\n", min);
    else if(hour == 24)
        printf("12:%02da\n", min);
    else if (hour > 12)
        printf("%2d:%02dp\n", hour - 12, min);
    else
        printf("%2d:%02da\n", hour, min);
}

void getdate(unsigned short& days) {    
    time_t* timer = NULL;
    time_t t = time(timer);
    tm gm;
    gmtime_s(&gm, &t);

    days = gm.tm_year - 80;
    days *= 16;
    days += (1 + gm.tm_mon);
    days *= 32;
    days += gm.tm_mday;
}

void gettime(unsigned short& times) {   
    time_t* timer = NULL;
    time_t t = time(timer);
    tm gm;
    gmtime_s(&gm, &t);

    times = 8 + gm.tm_hour;
    if (times > 24)
        times %= 24;
    times *= 64;
    times += gm.tm_min;
    times *= 32;
    times += gm.tm_sec / 2;
}

void help() {   
    printf("## Case Insensitive!\n");
    printf("## Supported instructions:\n");
    printf("--MBR\ndisplay the information in MBR\n\n");
    printf("--FAT12\ncheck whether the format of this floppy is FAT12.\n\n");
    printf("--CD + [path]\nenter the directory which the path leads to\n\n");
    printf("--DIR + [path]\nshow the files and directories in the directory where the path leads\n\n");
    printf("--TYPE + [path]\nshow the content of the file which the path leads to\n\n");
    printf("--MD + [path]\ngenerate a directory which fits in with the path\n\n");
    printf("--RD + [path]\nremove the directory where the path leads\n\n");
    printf("--DEL + [path]\ndelete the file which the path leads to\n\n");
    printf("--COPY + [path1]/con + [path2]\ncopy file1 where path1 leads to file2 where path2 leads\n");
    printf("or copy what you input to file2. If file2 doesn't exist, make a new file.\n");
    printf("!!Attention: At the end of file, Ctrl+Z directly and Enter twice to terminate the input.\n");
    printf("  Enter first and then Ctrl+Z can't terminate the input!!\n");
}
