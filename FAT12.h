#pragma pack(push)
#pragma pack(1)     

struct Fat12MBR {
    char jump[3];
    char BS_OEMName[8];         
    unsigned short BPB_BytsPerSec; 
    unsigned char BPB_SecPerClus;  
    unsigned short BPB_RsvdSecCnt; 
    unsigned char BPB_NumFATs;     
    unsigned short BPB_RootEntCnt; 
    unsigned short BPB_TotSec16;   
    unsigned char BPB_Media;       
    unsigned short BPB_FATSz16;    
    unsigned short BPB_SecPerTrk;  
    unsigned short BPB_NumHeads;   
    unsigned int BPB_HiddSec;      
    unsigned int BPB_TotSec32;     
    unsigned char BS_DrvNum;       
    unsigned char BS_Reserved1;    
    unsigned char BS_BootSig;      
    unsigned int BS_VolID;         
    char BS_VolLab[11];         
    char BS_FileSysType[8];     
};

struct entry {
    char DIR_Name[11];              
    unsigned char DIR_Attr;         
    unsigned char reserve[10];      
    unsigned short DIR_WrtTime;     
    unsigned short DIR_WrtDate;     
    unsigned short DIR_FstClus;     
    unsigned int DIR_FileSize;      
};

#pragma pack(pop)

struct node {
    entry obj;      
    bool dir;       
    int offset;     
    node* prev;     
    node* next;     
    node* down;     
    node(): dir(false), offset(0), prev(NULL), next(NULL), down(NULL) {}
};




void PrintMBR(char* MBR);               
void fat12(char* img, int size);        
char* cd(char* param, char* header, node** now, node* roots, int cnt);  
void dir(char* img, char* param, char* header, node* now, node* roots, int cnt);    
void mkdir(char* img, char* param, node* roots, int& cnt, node* now);   
void rmdir(char* img, char* param, node* roots, int cnt, node* now);    
void type(char* img, char* param, node* roots, int cnt, node* now);     
void del(char* img, char* param, node* roots, int cnt, node* now);      
void copy(char* img, char* param, node* roots, int& cnt, node* now);    
void help();            


int find_roots(node* roots, char* img);     
void find_sub_entry(node* root, char* img); 
int tree(node* roots, char* img);           
void show_tree(node* roots, int cnt, node* now);    
void clear_tree(node* roots, int cnt);      


node* path_analyze(char* param, node* roots, int cnt, node* now);   
void PrintEntry(entry obj, int& num, int& size);    
void edit_roots(char* img, node* roots, int &cnt, int& pos, int size);  
void edit_sub_dir(char* img, node* dir, node* tmp);         
void edit_empty_dir(char* img, node* now, int cls_num);     
void input(char* buffer, int size, int& length);            
node* find_dir(char* param, node* roots, int cnt, node* now);   
node* find_file(node* roots, int cnt, node* dir);       
void edit_entry(char* img, node* obj);                  


void read_fat(char* img, int* chain, int &num);     
void empty_fat(char* img, int init);                
void find_0_fat(char* img, int* chain, int num);    
void edit_fat(char* img, int* chain, int num);      
void read_block(char* img, char* block, int num, bool cls);     
void write_block(char* img, char* block, int num, bool cls);    


void date(unsigned short days);     
void time(unsigned short time);     
void getdate(unsigned short &days);     
void gettime(unsigned short &times);    
