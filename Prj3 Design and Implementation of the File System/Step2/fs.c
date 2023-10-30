/*
***************************************************************
This program is used to simulate the file system.
***************************************************************
 source file:
    fs.c

 compile:
    make

 usage:
    ./fs
*/
#include <stdio.h>
#include <string.h>
#include <gdcache.h>
#include <time.h>

//fixed size in this program
#define CYLINDER_NUM 16
#define SECTOR_PER_CYLINDER 16

#define SECTOR_SIZE 256
#define MAX_CMD_LEN 1024
#define MAX_FILE_NAME_LEN 8
#define INODE_BLOCK_INODE_NUM 8
#define MAX_INDIRECT_BLOCK_INODE_NUM 64

#define false 0
#define true 1

//check if the string is a number
int is_nature_number(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        if(str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }
    return 1;
}
//convert string to unsigned int
int stou(char *str) {
    int res = 0;
    for(int i = 0; i < strlen(str); i++) {
        res = res * 10 + (str[i] - '0');
    }
    return res;
}
//split
int split(char *str, char *res[], char *delim) {
    char *p;
    int i = 0;
    p = strtok(str, delim);
    while(p) {
        res[i] = p;
        i++;
        p = strtok(NULL, delim);
    }
    return i;
}
//sector 0
struct super_block_struct {
    int inode_bitmap_block_location;
    int data_block_bitmap_location;
    int inode_block_begin_location;
    int inode_num;
    int inode_block_num;
    int data_block_num;
    int data_block_begin_location;
};
//sector 1
struct inode_bitmap_block_struct {
    _Bool inode_bitmap[32];
};
//sector 2
struct data_bitmap_block_struct {
    _Bool data_block_bitmap[SECTOR_SIZE];
};
struct data_block_struct {
    char data[SECTOR_SIZE];
};
struct dir_block_struct {
    //248/12 = 20
    char child_name[20][MAX_FILE_NAME_LEN];
    int child_location[20]; //inode virtual location
    int parent;
    int child_num;
};

struct LUT_struct {
    unsigned char day_high: 8;
    unsigned char day_low: 4;
    unsigned char hour_high: 4;
    unsigned char hour_low: 2;
    unsigned char minute: 6;
};

struct ls_file_struct {
    char filename[MAX_FILE_NAME_LEN];
    char type[5];
    int size;
    char last_modified_time[20];
};

struct inode_struct {
    _Bool type;   //0: file, 1: directory
    struct LUT_struct LUT;
    char filename[MAX_FILE_NAME_LEN];
    int direct[4];
    int indirect;
};
//sector 3, 4, 5, 6
struct inode_block_struct {
    struct inode_struct inode[INODE_BLOCK_INODE_NUM];
};
struct indirect_block_struct {
    int direct[64];
};
union Sector_union {
    //size = 256bytes
    struct super_block_struct super_block;
    struct inode_bitmap_block_struct inode_bitmap_block;
    struct data_bitmap_block_struct data_bitmap_block;
    struct inode_block_struct inode_block;
    struct indirect_block_struct indirect_block;
    struct dir_block_struct dir_block;
    struct data_block_struct data_block;
};

void parse_day(int day, int *month, int *day_of_month) {
    int month_day[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int i = 0;
    while (day > month_day[i]) {
        day -= month_day[i];
        i++;
    }
    *month = i + 1;
    *day_of_month = day;
}

void LUT_print(char res[] ,struct inode_struct *inode) {
    struct LUT_struct LUT = inode->LUT;
    int day = (LUT.day_high << 4) + LUT.day_low;
    int hour = (LUT.hour_high << 2) + LUT.hour_low;
    int minute = LUT.minute;
    //get month from day
    int month = 1;
    int day_of_month = 0;
    parse_day(day, &month, &day_of_month);
    if(minute < 10) {
        sprintf(res, "2023/%d/%d %d:0%d", month, day_of_month, hour, minute);
        return;
    } else {
        sprintf(res, "2023/%d/%d %d:%d", month, day_of_month, hour, minute);
        return;
    }
}

void inode_LUT_update(struct inode_struct *inode) {
    struct LUT_struct LUT;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int day = tm->tm_yday + 1;
    int hour = tm->tm_hour;
    int minute = tm->tm_min;
    LUT.day_high = day >> 4;
    LUT.day_low = day & 0xf;
    LUT.hour_high = hour >> 2;
    LUT.hour_low = hour & 0x3;
    LUT.minute = minute & 0x3f;
    inode->LUT = LUT;
}

void ls_sort(struct ls_file_struct q[], int num) {
    for(int i = 0; i < num; i++) {
        for(int j = i + 1; j < num; j++) {
            if (strcmp(q[i].filename, q[j].filename) > 0) {
                struct ls_file_struct temp = q[i];
                q[i] = q[j];
                q[j] = temp;
            }
        }
    }
}

//file ptr
struct file_ptr{
    char *ptr;
    int inner_idx;      //idx in cur section
    int section_idx;    //idx of section, such as direct[0]: 0, direct[1]: 1, direct[2]: 2, direct[3]: 3, indirect: 4, indirect[0]: 5, indirect[1]: 6
    struct inode_struct *cur_inode;
};
//file ptr next
_Bool file_ptr_next(struct file_ptr *fp, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    _Bool change_section_flag = false;
    fp->inner_idx++;
    if(fp->inner_idx == SECTOR_SIZE) {
        fp->section_idx++;
        fp->inner_idx = 0;
        change_section_flag = true;
    }
    //reach the end of file
    if(fp->section_idx == 4 && fp->cur_inode->indirect == -1) {
        return false;
    }
    if(fp->section_idx < 4 && fp->cur_inode->direct[fp->section_idx] == -1) {
        return false;
    }
    if(fp->section_idx >= 4 && fp->cur_inode->indirect != -1 && file_storage[fp->cur_inode->indirect / SECTOR_PER_CYLINDER][fp->cur_inode->indirect % SECTOR_PER_CYLINDER].indirect_block.direct[fp->section_idx - 4] == -1) {
        return false;
    }
    //update
    if(fp->section_idx < 4 && change_section_flag) {
        int cur_data_block_idx = fp->cur_inode->direct[fp->section_idx];
        int cur_data_block_cylinder = cur_data_block_idx / SECTOR_PER_CYLINDER;
        int cur_data_block_sector = cur_data_block_idx % SECTOR_PER_CYLINDER;
        fp->ptr = file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block.data;
    }
    else if (fp->section_idx >= 4 && change_section_flag) {
        int cur_indirect_block_idx = fp->cur_inode->indirect;
        int cur_indirect_block_cylinder = cur_indirect_block_idx / SECTOR_PER_CYLINDER;
        int cur_indirect_block_sector = cur_indirect_block_idx % SECTOR_PER_CYLINDER;
        int cur_data_block_idx = file_storage[cur_indirect_block_cylinder][cur_indirect_block_sector].indirect_block.direct[fp->section_idx - 4];
        int cur_data_block_cylinder = cur_data_block_idx / SECTOR_PER_CYLINDER;
        int cur_data_block_sector = cur_data_block_idx % SECTOR_PER_CYLINDER;
        fp->ptr = file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block.data;
    } else {
        fp->ptr++;
    }
    //reach the end of content
    if(*(fp->ptr) == '\0') {
        return false;
    }
    return true;
}
//file ptr init
void make_file_ptr(struct file_ptr *fp, struct inode_struct *inode, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    fp->cur_inode = inode;
    fp->section_idx = 0;
    fp->inner_idx = 0;
    if(inode->direct[0] != -1) {
        fp->ptr = file_storage[inode->direct[0] / SECTOR_PER_CYLINDER][inode->direct[0] % SECTOR_PER_CYLINDER].data_block.data;
        } else {
        fp->ptr = NULL;
    }
}

//get inode
struct inode_struct * get_inode(int inode_virtual_location, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    int cur_dir_inode_block_idx = inode_virtual_location / INODE_BLOCK_INODE_NUM;   //virtual block idx
    int cur_dir_inode_idx = inode_virtual_location % INODE_BLOCK_INODE_NUM;  //inode idx in block
    struct inode_struct *cur_inode = &(file_storage[0][cur_dir_inode_block_idx + spb->inode_block_begin_location].inode_block.inode[cur_dir_inode_idx]);
    return cur_inode;
}
//get dir data block
struct dir_block_struct *get_dir_data_block(struct inode_struct *cur_dir_inode, union Sector_union file_storage[16][16], struct super_block_struct *spb) {
    int cur_dir_data_block_idx = cur_dir_inode->direct[0];
    int dir_data_block_cylinder = cur_dir_data_block_idx / SECTOR_PER_CYLINDER;
    int dir_data_block_sector = cur_dir_data_block_idx % SECTOR_PER_CYLINDER;
    struct dir_block_struct *cur_dir_data_block = &(file_storage[dir_data_block_cylinder][dir_data_block_sector].dir_block);
    return cur_dir_data_block;
}
//get data block
struct data_block_struct *get_file_data_block(int cur_data_block_idx, union Sector_union file_storage[16][16], struct super_block_struct *spb) {
    int cur_data_block_cylinder = cur_data_block_idx / SECTOR_PER_CYLINDER;
    int cur_data_block_sector = cur_data_block_idx % SECTOR_PER_CYLINDER;
    struct data_block_struct *cur_data_block = &(file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block);
    return cur_data_block;
}
//bitmap check
_Bool bitmap_check(const _Bool *bitmap, int bitmap_size, int required_num, int q[]) {
    int count = 0;
    for(int i = 0; i < bitmap_size; i++) {
        if (bitmap[i] == 0) {
            q[count++] = i;
            if (count == required_num) {
                return 1;
            }
        }
    }
    return 0;
}
//get file size
int get_file_size(struct inode_struct* cur_file, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    int cur_file_size = 0;
    for(int i = 0; i < 4; i++) {
        if(cur_file->direct[i] != -1) {
            cur_file_size += SECTOR_SIZE;
        }
    }
    if (cur_file->indirect != -1) {
        //get indirect data block
        int indirect_block_cylinder = cur_file->indirect / SECTOR_PER_CYLINDER;
        int indirect_block_sector = cur_file->indirect % SECTOR_PER_CYLINDER;
        struct indirect_block_struct *indirect_block = &(file_storage[indirect_block_cylinder][indirect_block_sector].indirect_block);
        for(int i = 0; i < MAX_INDIRECT_BLOCK_INODE_NUM; i++) {
            if(indirect_block->direct[i] != -1) {
                cur_file_size += SECTOR_SIZE;
            }
        }
    }
    return cur_file_size;
}
//get content size
int get_content_size(struct inode_struct* cur_file, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    int cur_content_size = 0;
    struct file_ptr fp;
    make_file_ptr(&fp, cur_file, file_storage);
    do {
        if(*fp.ptr == '\0') {
            break;
        }
        cur_content_size++;
    } while(file_ptr_next(&fp, file_storage));
    return cur_content_size;
}

void make_ls_struct(struct ls_file_struct *ls_file, struct inode_struct *inode, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    strcpy(ls_file->filename, inode->filename);
    if (inode->type == 0) {
        strcpy(ls_file->type, "file");
        ls_file->size = get_content_size(inode, file_storage, spb);
    } else {
        strcpy(ls_file->type, "dir ");
        ls_file->size = 256;
    }
    LUT_print(ls_file->last_modified_time, inode);
}

//file resize
_Bool file_resize(struct inode_struct* cur_file, int size, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    //floor size
    if (size % SECTOR_SIZE != 0) {
        size = (size / SECTOR_SIZE + 1) * SECTOR_SIZE;
    }
    int cur_file_size = get_file_size(cur_file, file_storage);
    //file expand
    if (cur_file_size < size) {
        //expand
        int data_block_diff = (size - cur_file_size) / SECTOR_SIZE;
        int *data_block_idx = (int *)malloc(sizeof(int) * data_block_diff);
        //check remain space
        if(!bitmap_check(file_storage[0][2].data_bitmap_block.data_block_bitmap, SECTOR_SIZE, data_block_diff, data_block_idx) ) {
            printf("no enough space\n");
            return false;
        }
        //set bitmap
        for (int i = 0; i < data_block_diff; i++) {
            file_storage[0][2].data_bitmap_block.data_block_bitmap[data_block_idx[i]] = true;
        }
        //set inode to add new data block
        int idx = 0;
        for(int j = 0; j < 4; j++) {
            if(cur_file->direct[j] == -1 && idx < data_block_diff) {
                cur_file->direct[j] = data_block_idx[idx++];
                //clean this data block
                int cur_data_block_cylinder = cur_file->direct[j] / SECTOR_PER_CYLINDER;
                int cur_data_block_sector = cur_file->direct[j] % SECTOR_PER_CYLINDER;
                struct data_block_struct *cur_data_block = &(file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block);
                for(int k = 0; k < SECTOR_SIZE; k++) {
                    cur_data_block->data[k] = '\0';
                }
            }
        }
        //if there is no remain space in direct block, set an indirect block
        if (idx < data_block_diff && cur_file->indirect == -1) {
            int indirect_block_idx = -1;
            if (!bitmap_check(file_storage[0][2].data_bitmap_block.data_block_bitmap, SECTOR_SIZE, 1,
                              &indirect_block_idx)) {
                printf("no enough space\n");
                return false;
            }
            file_storage[0][2].data_bitmap_block.data_block_bitmap[indirect_block_idx] = true;
            cur_file->indirect = indirect_block_idx;
            //set indirect block to -1
            int indirect_block_cylinder = cur_file->indirect / SECTOR_PER_CYLINDER;
            int indirect_block_sector = cur_file->indirect % SECTOR_PER_CYLINDER;
            struct indirect_block_struct *indirect_block = &(file_storage[indirect_block_cylinder][indirect_block_sector].indirect_block);
            for (int i = 0; i < MAX_INDIRECT_BLOCK_INODE_NUM; i++) {
                indirect_block->direct[i] = -1;
            }
        }
        //set indirect block
        while (idx < data_block_diff) {
            int indirect_block_cylinder = cur_file->indirect / SECTOR_PER_CYLINDER;
            int indirect_block_sector = cur_file->indirect % SECTOR_PER_CYLINDER;
            struct indirect_block_struct *indirect_block = &(file_storage[indirect_block_cylinder][indirect_block_sector].indirect_block);
            //get indirect data block
            for(int j = 0; j < MAX_INDIRECT_BLOCK_INODE_NUM; j++) {
                if(indirect_block->direct[j] == -1 && idx < data_block_diff) {
                    indirect_block->direct[j] = data_block_idx[idx++];
                    //clean this data block
                    int cur_data_block_cylinder = indirect_block->direct[j] / SECTOR_PER_CYLINDER;
                    int cur_data_block_sector = indirect_block->direct[j] % SECTOR_PER_CYLINDER;
                    struct data_block_struct *cur_data_block = &(file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block);
                    for(int k = 0; k < SECTOR_SIZE; k++) {
                        cur_data_block->data[k] = '\0';
                    }
                }
            }
        }
        free(data_block_idx);
    } else if (cur_file_size > size) {
//        printf("cur_file_size: %d\n", cur_file_size);
        //shrink
        int data_block_diff = (cur_file_size - size) / SECTOR_SIZE;
        int *data_block_idx = (int *)malloc(sizeof(int) * data_block_diff);
//        printf("data_block_diff: %d\n", data_block_diff);
        //get data block idx
        int idx = 0;
        if (cur_file->indirect != -1) {
            int indirect_block_cylinder = cur_file->indirect / SECTOR_PER_CYLINDER;
            int indirect_block_sector = cur_file->indirect % SECTOR_PER_CYLINDER;
            struct indirect_block_struct *indirect_block = &(file_storage[indirect_block_cylinder][indirect_block_sector].indirect_block);
            int max_idx = MAX_INDIRECT_BLOCK_INODE_NUM - 1;
            for(; max_idx >= 0; max_idx--) {
                if(indirect_block->direct[max_idx] != -1) {
//                    printf("max_id: %d\n", max_idx);
                    data_block_idx[idx++] = indirect_block->direct[max_idx];
                    indirect_block->direct[max_idx] = -1;
//                    printf("indirect->direct[%d] = %d\n", max_idx, indirect_block->direct[max_idx]);
                    if(idx == data_block_diff) {
                        break;
                    }
                }
            }
            if(max_idx == -1) {
                printf("indirect block is empty\n");
                file_storage[0][2].data_bitmap_block.data_block_bitmap[cur_file->indirect] = false;
                cur_file->indirect = -1;
//                printf("indirect = %d\n", cur_file->indirect);
            }
        }
        if (idx < data_block_diff) {
            for(int i = 3; i >= 0; i--) {
                if(cur_file->direct[i] != -1) {
                    data_block_idx[idx++] = cur_file->direct[i];
                    cur_file->direct[i] = -1;
//                    printf("direct[%d] = %d\n", i, cur_file->direct[i]);
                    if(idx == data_block_diff) {
                        break;
                    }
                }
            }
        }
        //set bitmap
        for (int i = 0; i < data_block_diff; i++) {
//            printf("data_block_idx[%d] = %d\n", i, data_block_idx[i]);
            file_storage[0][2].data_bitmap_block.data_block_bitmap[data_block_idx[i]] = false;
        }
        free(data_block_idx);
    }
    return true;
}
//file delete
_Bool file_delete(char* file_name, int cur_dir_inode_virtual_location ,union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    //get cur dir
    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
    //find file
    _Bool found = false;
    for(int i = 0; i < cur_dir_data_block->child_num; i++) {
        struct inode_struct* child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
        if (child_inode->type == 1) {
            continue;
        }
        if (strcmp(cur_dir_data_block->child_name[i], file_name) == 0) {
            found = true;
            //delete file
            file_resize(child_inode, 0, file_storage);
            //set inode bitmap
            file_storage[0][1].inode_bitmap_block.inode_bitmap[cur_dir_data_block->child_location[i]] = false;
            //set dir block
            cur_dir_data_block->child_num--;
            for (int j = i; j < cur_dir_data_block->child_num; j++) {
                strcpy(cur_dir_data_block->child_name[j], cur_dir_data_block->child_name[j + 1]);
                cur_dir_data_block->child_location[j] = cur_dir_data_block->child_location[j + 1];
            }
            break;
        }
    }
    return found;
}
//dir delete
_Bool dir_delete(char* dir_name, int cur_dir_inode_virtual_location ,union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    //get cur dir
    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
    //find file
    _Bool found = false;
    for(int i = 0; i < cur_dir_data_block->child_num; i++) {
        struct inode_struct* child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
        if (child_inode->type == 0) {
            continue;
        }
        if (strcmp(cur_dir_data_block->child_name[i], dir_name) == 0) {
            found = true;
            //delete all the content in the dir
            struct dir_block_struct *child_dir_data_block = get_dir_data_block(child_inode, file_storage, spb);
            //traverse all item in child_dir and delete
            for(int j = 0; j < child_dir_data_block->child_num; j++) {
                struct inode_struct* child_child_inode = get_inode(child_dir_data_block->child_location[j], file_storage, spb);
                //ignore . and ..
                if (strcmp(child_dir_data_block->child_name[j], ".") == 0 || strcmp(child_dir_data_block->child_name[j], "..") == 0) {
                    continue;
                }
                if (child_child_inode->type == 0) {
                    //file, its bitmap will be set in file_delete()
                    file_delete(child_dir_data_block->child_name[j], cur_dir_data_block->child_location[i], file_storage, spb);
                    j = 0;
                } else {
                    //dir
                    dir_delete(child_dir_data_block->child_name[j], cur_dir_data_block->child_location[i], file_storage, spb);
                    //set inode bitmap
                    file_storage[0][1].inode_bitmap_block.inode_bitmap[child_dir_data_block->child_location[i]] = false;
                    //reset
                    j = 0;
                }
            }
            //set inode bitmap
            file_storage[0][1].inode_bitmap_block.inode_bitmap[cur_dir_data_block->child_location[i]] = false;
            file_storage[0][2].data_bitmap_block.data_block_bitmap[child_inode->direct[0]] = false;
            //set dir block
            cur_dir_data_block->child_num--;
            for (int j = i; j < cur_dir_data_block->child_num; j++) {
                strcpy(cur_dir_data_block->child_name[j], cur_dir_data_block->child_name[j + 1]);
                cur_dir_data_block->child_location[j] = cur_dir_data_block->child_location[j + 1];
            }
        }
    }
    return found;
}
//
int main(int argc, char *argv[]) {
    //check argc
    if (argc != 1) {
        printf("Usage: ./fs\n");
        return 0;
    }

    //create disk log
    FILE *log = fopen("fs.log", "w+");
    if (log == NULL) {
        printf("Error: cannot create disk log file.\n");
        return 0;
    }

    //create file_storage
    union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER];
    _Bool inited = false;
    int cur_dir_inode_virtual_location = 0;

    //receive request
    while(1) {
        //get command
        char cmd[MAX_CMD_LEN];
        //print green prompt
        printf("\033[1;32m> \033[0m");
        fflush(stdin);
        fgets(cmd, MAX_CMD_LEN, stdin);
        fflush(stdin);
        char *cur_argv[MAX_CMD_LEN];
        cmd[strlen(cmd) - 1] = '\0';
        int cur_argc = split(cmd, cur_argv, " ");

        //super block pointer to increase readability
        struct super_block_struct *spb = &(file_storage[0][0].super_block);

        //execute command
        for(int v = 0; v < 1; v++) {
            //check init
            if (!inited) {
                if(strcmp(cur_argv[0], "e") == 0) {
                    fprintf(log, "Goodbye!\n");
                    fclose(log);
                    return 0;
                    break;
                }
                if (strcmp(cur_argv[0], "f") != 0) {
                    printf("Error: file system not initialized.\n");
                    fprintf(log, "NO: file system not initialized.\n");
                    break;
                }
            }

            //execute command
            if (strcmp(cur_argv[0], "f") == 0) {
                //check
                if (cur_argc != 1) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //init 0: super block, 1: inode bitmap, 2: data block bitmap, 3-6: inode block, 7: root dir block, 8-255: unused
                //init super block
                spb->inode_bitmap_block_location = 1;
                spb->data_block_bitmap_location = 2;
                spb->inode_block_begin_location = 3;
                spb->inode_num = 32;
                spb->inode_block_num = 4;
                spb->data_block_num = 256;
                spb->data_block_begin_location = 7;
                //init inode bitmap
                for(int i = 0; i < 32; i++) {
                    file_storage[0][1].inode_bitmap_block.inode_bitmap[i] = false;
                }
                //init data block bitmap & set initial 8 blocks used
                for(int i = 0; i < SECTOR_SIZE; i++) {
                    file_storage[0][2].data_bitmap_block.data_block_bitmap[i] = false;
                }
                for(int i = 0; i < 8; i++) {
                    file_storage[0][2].data_bitmap_block.data_block_bitmap[i] = true;
                }
                //init inode block
                for(int i = 0; i < spb->inode_block_num; i++) {
                    for(int j = 0; j < 8; j++) {
                        file_storage[0][i + spb->inode_block_num - 1].inode_block.inode[j].type = -1;
                        for(int k = 0; k < 4; k++) {
                            file_storage[0][i + spb->inode_block_num - 1].inode_block.inode[j].direct[k] = -1;
                        }
                        file_storage[0][i + spb->inode_block_num - 1].inode_block.inode[j].indirect = -1;
                    }
                }
                //init root dir block
                file_storage[0][7].dir_block.child_num = 1; //.
                for(int i = 0; i < 20; i++) {
                    file_storage[0][7].dir_block.child_location[i] = -1;
                }
                strcpy(file_storage[0][7].dir_block.child_name[0], ".");
                file_storage[0][7].dir_block.child_location[0] = 0; //inode 0
                file_storage[0][7].dir_block.parent = -1;
                cur_dir_inode_virtual_location = 0;
                file_storage[0][1].inode_bitmap_block.inode_bitmap[0] = true;
                //set dir inode
                file_storage[0][3].inode_block.inode[0].type = 1;   //dir
                file_storage[0][3].inode_block.inode[0].direct[0] = 7;   //dir block
                file_storage[0][3].inode_block.inode[0].filename[0] = '/';

                //write to file
                inited = true;
                fprintf(log, "YES: file system inited\n");
            }
            else if (strcmp(cur_argv[0], "d") == 0) {
                //check: d filename offset length
                if (cur_argc != 4) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //find file
                //get cur dir
                struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                //find file
                _Bool found = false;
                for(int i = 0; i < cur_dir_data_block->child_num; i++) {
                    struct inode_struct* child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
                    if (child_inode->type == 1) {
                        continue;
                    }
                    if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                        found = true;
                        if(!is_nature_number(cur_argv[2])) {
                            printf("Error: invalid argument.\n");
                            fprintf(log, "NO: invalid argument\n");
                            break;
                        }
                        if(!is_nature_number(cur_argv[3])) {
                            printf("Error: invalid argument.\n");
                            fprintf(log, "NO: invalid argument\n");
                            break;
                        }
                        //init offset & len
                        int offset = stou(cur_argv[2]);
                        int len = stou(cur_argv[3]);
                        if(offset > get_content_size(child_inode, file_storage, spb)) {
                            //no need to delete
                            break;
                        }
                        //set len
                        if (offset + len > get_content_size(child_inode, file_storage, spb)) {
                            len = get_content_size(child_inode, file_storage, spb) - offset;
                        }
                        //set file ptr
                        struct file_ptr fp, fp2, end;
                        make_file_ptr(&fp, child_inode, file_storage);
                        make_file_ptr(&fp2, child_inode, file_storage);
                        make_file_ptr(&end, child_inode, file_storage);
                        //delete
                        for(int j = 0; j < get_content_size(child_inode, file_storage, spb) - len; j++) {
                            file_ptr_next(&end, file_storage);
                        }
                        for(int j = 0; j < offset; j++) {
                            file_ptr_next(&fp, file_storage);
                        }
                        for(int j = 0; j < len; j++) {
                            *fp.ptr = '\0';
                            file_ptr_next(&fp, file_storage);
                        }
                        //move
                        fp2 = fp;
                        make_file_ptr(&fp, child_inode, file_storage);
                        for(int j = 0; j < offset; j++) {
                            file_ptr_next(&fp, file_storage);
                        }
                        do {
                            *fp.ptr = *fp2.ptr;
                            file_ptr_next(&fp, file_storage);
                            file_ptr_next(&fp2, file_storage);
                        } while(*fp2.ptr != '\0');
                        //clean remain space
                        do{
                            *end.ptr = '\0';
                            file_ptr_next(&end, file_storage);
                        } while(*end.ptr != '\0');
                        //resize
                        file_resize(child_inode, get_content_size(child_inode, file_storage, spb), file_storage);
                        //update LTU
                        inode_LUT_update(child_inode);
                        inode_LUT_update(cur_dir_inode);
                        fprintf(log, "YES: delete file content successfully!\n");
                    }
                }
                if (!found) {
                    printf("Error: file not found.\n");
                    fprintf(log, "NO: file not found.\n");
                }
            }
            else if (strcmp(cur_argv[0], "w") == 0) {
                //check: w filename offset content
                if (cur_argc < 4) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //find file
                //get cur dir
                struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                //find file
                _Bool found = false;
                for(int i = 0; i < cur_dir_data_block->child_num; i++) {
                    struct inode_struct* child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
                    if (child_inode->type == 1) {
                        continue;
                    }
                    if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                        found = true;
                        if(!is_nature_number(cur_argv[2])) {
                            printf("Error: invalid argument.\n");
                            fprintf(log, "NO: invalid argument\n");
                            break;
                        }
                        //set len
                        int len = stou(cur_argv[2]);
                        //set write content
                        char content[MAX_CMD_LEN] = "";
                        for(int j = 3; j < cur_argc; j++) {
                            strcat(content, cur_argv[j]);
                            if(j != cur_argc - 1) {
                                strcat(content, " ");
                            }
                        }
                        //handle "\n" to '\n'
                        for(int j = 0; j < strlen(content); j++) {
                            if(content[j] == '\\' && content[j + 1] == 'n') {
                                content[j] = '\n';
                                for(int k = j + 1; k < strlen(content); k++) {
                                    content[k] = content[k + 1];
                                }
                            }
                        }
                        //check len and length of content
                        if(len < strlen(content)) {
                            content[len] = '\0';
                        } else if (len > strlen(content)) {
                            len = (int)strlen(content);
                        }
                        //resize
                        file_resize(child_inode, 0, file_storage);  //clean file
                        if(!file_resize(child_inode, (int)strlen(content), file_storage)) {
                            printf("Error: no enough space.\n");
                            fprintf(log, "NO: no enough space\n");
                            break;
                        }
                        char* content_ptr = content;
                        //set file ptr
                        struct file_ptr fp;
                        make_file_ptr(&fp, child_inode, file_storage);
                        //write
                        while(*content_ptr != '\0') {
                            *fp.ptr = *content_ptr;
                            fp.ptr++;
                            content_ptr++;
                        }
                        inode_LUT_update(child_inode);
                        fprintf(log, "YES: write to file successfully.\n");
                    }
                }
                if (!found) {
                    printf("Error: file not found.\n");
                    fprintf(log, "NO: file not found.\n");
                }
            }
            else if (strcmp(cur_argv[0], "cat") == 0) {
                //check
                if (cur_argc != 2) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //find file
                //get cur dir
                struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                //find file
                _Bool found = false;
                for(int i = 0; i < cur_dir_data_block->child_num; i++) {
                    struct inode_struct* child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
                    if (child_inode->type == 1) {
                        continue;
                    }
                    if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                        found = true;
                        fprintf(log, "YES:\n");
                        struct file_ptr fp;
                        make_file_ptr(&fp, child_inode, file_storage);
                        do {
                            if(fp.ptr == NULL) break;
                            printf("%c", *fp.ptr);
                            fprintf(log, "%c", *fp.ptr);
                        } while(file_ptr_next(&fp, file_storage));
                        printf("\n");
                        fprintf(log, "\n");
                    }
                }
                if (!found) {
                    printf("Error: file not found.\n");
                    fprintf(log, "NO: file not found.\n");
                }
            }
            else if (strcmp(cur_argv[0], "i") == 0) {
                //check: i filename offset content
                if (cur_argc < 5) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //find file
                //get cur dir
                struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                //find file
                _Bool found = false;
                for(int i = 0; i < cur_dir_data_block->child_num; i++) {
                    struct inode_struct* child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
                    if (child_inode->type == 1) {
                        continue;
                    }
                    if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                        found = true;
                        int cur_file_max_length = get_file_size(child_inode, file_storage);
                        if(cur_file_max_length == 0) {
                            if(!file_resize(child_inode, SECTOR_SIZE, file_storage)) {
                                printf("Error: no enough space.\n");
                                fprintf(log, "NO: no enough space\n");
                                break;
                            }
                        }
                        int cur_file_content_length = get_content_size(child_inode, file_storage, spb);
                        if(!is_nature_number(cur_argv[2])) {
                            printf("Error: invalid offset.\n");
                            fprintf(log, "NO: invalid offset\n");
                            break;
                        }
                        if(!is_nature_number(cur_argv[3])) {
                            printf("Error: invalid length.\n");
                            fprintf(log, "NO: invalid length\n");
                            break;
                        }
                        //set offset
                        int offset = stou(cur_argv[2]);
                        if (offset > cur_file_content_length) {
                            //change offset to append content
                            offset = cur_file_content_length;
                        }
                        //set write content
                        char content[MAX_CMD_LEN] = "";
                        for(int j = 4; j < cur_argc; j++) {
                            strcat(content, cur_argv[j]);
                            if(j != cur_argc - 1) {
                                strcat(content, " ");
                            }
                        }
                        //set length
                        int len = stou(cur_argv[3]);
                        if(len > strlen(content)) {
                            len = strlen(content);
                        } else {
                            for(int j = len; j < strlen(content); j++) {
                                content[j] = '\0';
                            }
                        }

                        //resize
                        if(strlen(content) + cur_file_content_length > cur_file_max_length) {
                            if(!file_resize(child_inode, (int)strlen(content) + cur_file_content_length, file_storage)) {
                                printf("Error: no enough space.\n");
                                fprintf(log, "NO: no enough space\n");
                                break;
                            }
                        }
                        char* content_ptr = content;
                        //set file ptr
                        struct file_ptr fp;
                        make_file_ptr(&fp, child_inode, file_storage);
                        if(offset == cur_file_content_length) {
                            //append
                            if(cur_file_content_length != 0) while(file_ptr_next(&fp, file_storage));
                            while(*content_ptr != '\0') {
                                *fp.ptr = *content_ptr;
                                content_ptr++;
                                file_ptr_next(&fp, file_storage);
                            }
                        }
                        else {
                            //insert
                            //move content to get enough space
                            for(int j = 0; j < offset; j++) {
                                file_ptr_next(&fp, file_storage);
                            }
                            //temporary store content after offset
                            char* cache = (char*)malloc(sizeof(char) * (cur_file_content_length - offset) + 1);
                            memset(cache, 0, sizeof(char) * (cur_file_content_length - offset));
                            char* cache_ptr = cache;
                            do {
                                *cache_ptr = *fp.ptr;
                                cache_ptr++;
                            } while(file_ptr_next(&fp, file_storage));
                            //write content
                            make_file_ptr(&fp, child_inode, file_storage);
                            for(int j = 0; j < offset; j++) {
                                file_ptr_next(&fp, file_storage);
                            }
                            while(*content_ptr != '\0') {
                                *fp.ptr = *content_ptr;
                                content_ptr++;
                                file_ptr_next(&fp, file_storage);
                            }
                            //write cache
                            make_file_ptr(&fp, child_inode, file_storage);
                            //check '\n' in content, if there is a '\n', content_len - 1
                            for(int j = 0; j < offset + strlen(content); j++) {
                                file_ptr_next(&fp, file_storage);
                            }
                            cache_ptr = cache;
                            for(int j = 0; j < cur_file_content_length - offset; j++) {
                                //handle '\n'
                                if(*cache_ptr == '\\') {
                                    cache_ptr++;
                                    if(*cache_ptr == 'n') {
                                        *fp.ptr = '\n';
                                        cache_ptr++;
                                        file_ptr_next(&fp, file_storage);
                                        continue;
                                    } else {
                                        cache_ptr--;
                                    }
                                }
                                *fp.ptr = *cache_ptr;
                                cache_ptr++;
                                file_ptr_next(&fp, file_storage);
                            }
                        }
                        inode_LUT_update(child_inode);
                        fprintf(log, "YES: insert to file successfully.\n");
                    }
                }
                if (!found) {
                    printf("Error: file not found.\n");
                    fprintf(log, "NO: file not found.\n");
                }
            }
            else if (strcmp(cur_argv[0], "cd") == 0) {
                if (cur_argc != 2) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                char *path[MAX_CMD_LEN];
                //parse path
                int path_len = split(cur_argv[1], path, "/");
                //if absolute path
                if (cur_argv[1][0] == '/') {
                    cur_dir_inode_virtual_location = 0;
                }
                if(path_len == 0) {
                    fprintf(log, "YES\n");
                    break;
                }
                //try to find dir
                _Bool found = false;
                for(int j = 0; j < path_len; j++) {
                    //get cur dir
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i],file_storage, spb);
                        if (child_inode->type == 0) {
                            continue;
                        }
                        if (strcmp(cur_dir_data_block->child_name[i], path[j]) == 0) {
                            cur_dir_inode_virtual_location = cur_dir_data_block->child_location[i];
                            if(j == path_len - 1) {
                                found = true;
                            }
                            break;
                        }
                    }
                }
                if (!found) {
                    printf("Error: not found.\n");
                    fprintf(log, "NO: not found.\n");
                } else {
                    fprintf(log, "YES\n");
                }
            }
            else if (strcmp(cur_argv[0], "rm") == 0) {
                if(cur_argc != 2) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                _Bool found = file_delete(cur_argv[1], cur_dir_inode_virtual_location, file_storage, spb);
                if (!found) {
                    printf("Error: file not found.\n");
                    fprintf(log, "NO: file not found.\n");
                } else {
                    fprintf(log, "YES: remove file successfully.\n");
                }
            }
            else if (strcmp(cur_argv[0], "rmdir") == 0) {
                if(cur_argc != 2) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                _Bool found = dir_delete(cur_argv[1], cur_dir_inode_virtual_location, file_storage, spb);
                if (!found) {
                    printf("Error: directory not found.\n");
                    fprintf(log, "NO: directory not found.\n");
                } else {
                    fprintf(log, "YES: remove directory successfully.\n");
                }
            }
            else if (strcmp(cur_argv[0], "mk") == 0) {
                //get cur dir
                struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                //check
                if (cur_argc != 2) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //check name
                if (strlen(cur_argv[1]) > MAX_FILE_NAME_LEN - 1) {
                    printf("Error: file name too long.\n");
                    fprintf(log, "NO: file name too long.\n");
                    break;
                }
                //check if name exists
                _Bool name_exists = false;
                for(int i = 0; i < cur_dir_data_block->child_num; i++) {
                    if (strcmp(cur_argv[1], cur_dir_data_block->child_name[i]) == 0) {
                        name_exists = true;
                        break;
                    }
                }
                if (name_exists) {
                    printf("Error: file name already exists.\n");
                    fprintf(log, "NO: file name already exists.\n");
                    break;
                }
                //check if inode bitmap is full
                int available_inode_location;
                if (!bitmap_check(file_storage[0][1].inode_bitmap_block.inode_bitmap, 32, 1, &available_inode_location)) {
                    printf("Error: inode bitmap is full.\n");
                    fprintf(log, "NO: inode bitmap is full.\n");
                    break;
                }
                //check if data block bitmap is full
                int available_data_block_location;
                if (!bitmap_check(file_storage[0][2].data_bitmap_block.data_block_bitmap, SECTOR_SIZE, 1, &available_data_block_location)) {
                    printf("Error: data block bitmap is full.\n");
                    fprintf(log, "NO: data block bitmap is full.\n");
                    break;
                }
                //check if dir block is full
                if(cur_dir_data_block->child_num == 20) {
                    printf("Error: directory is full.\n");
                    fprintf(log, "NO: directory is full.\n");
                    break;
                }
                //temp variables to increase readability
                int inode_block_idx = spb->inode_block_begin_location + available_inode_location / INODE_BLOCK_INODE_NUM;
                int inode_idx = available_inode_location % INODE_BLOCK_INODE_NUM;
                int data_block_idx = available_data_block_location;
                //create
                //update inode bitmap
                file_storage[0][1].inode_bitmap_block.inode_bitmap[available_inode_location] = true;
                //update data block bitmap
                file_storage[0][2].data_bitmap_block.data_block_bitmap[available_data_block_location] = true;
                //update inode
                file_storage[0][inode_block_idx].inode_block.inode[inode_idx].type = 0; //file
                inode_LUT_update(&file_storage[0][inode_block_idx].inode_block.inode[inode_idx]);   //update LUT
                file_storage[0][inode_block_idx].inode_block.inode[inode_idx].direct[0]= data_block_idx;
                strcpy(file_storage[0][inode_block_idx].inode_block.inode[inode_idx].filename, cur_argv[1]);
                //update dir block
                strcpy(cur_dir_data_block->child_name[cur_dir_data_block->child_num], cur_argv[1]);
                cur_dir_data_block->child_location[cur_dir_data_block->child_num] = available_inode_location;
                cur_dir_data_block->child_num++;
                //clean new allocated data block
                struct data_block_struct *data_block = get_file_data_block(data_block_idx, file_storage, spb);
                for (int i = 0; i < SECTOR_SIZE; i++) {
                    data_block->data[i] = '\0';
                }
                //write to file
                fprintf(log, "YES: create file %s\n", cur_argv[1]);
            }
            else if (strcmp(cur_argv[0], "mkdir") == 0) {
                //get cur dir
                struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                //check
                if (cur_argc != 2) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //check name
                if (strlen(cur_argv[1]) > MAX_FILE_NAME_LEN - 1) {
                    printf("Error: dir name too long.\n");
                    fprintf(log, "NO: dir name too long.\n");
                    break;
                }
                //check if name exists
                _Bool name_exists = false;
                for(int i = 0; i < cur_dir_data_block->child_num; i++) {
                    if (strcmp(cur_argv[1], cur_dir_data_block->child_name[i]) == 0) {
                        name_exists = true;
                        break;
                    }
                }
                if (name_exists) {
                    printf("Error: dir name already exists.\n");
                    fprintf(log, "NO: dir name already exists.\n");
                    break;
                }
                //check if inode bitmap is full
                int available_inode_location;
                if (!bitmap_check(file_storage[0][1].inode_bitmap_block.inode_bitmap, 32, 1, &available_inode_location)) {
                    printf("Error: inode bitmap is full.\n");
                    fprintf(log, "NO: inode bitmap is full.\n");
                    break;
                }
                //check if data block bitmap is full
                int available_data_block_location;
                if (!bitmap_check(file_storage[0][2].data_bitmap_block.data_block_bitmap, SECTOR_SIZE, 1, &available_data_block_location)) {
                    printf("Error: data block bitmap is full.\n");
                    fprintf(log, "NO: data block bitmap is full.\n");
                    break;
                }
                //check if dir block is full
                if(cur_dir_data_block->child_num == 20) {
                    printf("Error: directory is full.\n");
                    fprintf(log, "NO: directory is full.\n");
                    break;
                }
                //temp variables to increase readability
                int inode_block_idx = spb->inode_block_begin_location + available_inode_location / INODE_BLOCK_INODE_NUM;
                int inode_idx = available_inode_location % INODE_BLOCK_INODE_NUM;
                int data_block_idx = available_data_block_location;
                //create
                //update inode bitmap
                file_storage[0][1].inode_bitmap_block.inode_bitmap[available_inode_location] = true;
                //update data block bitmap
                file_storage[0][2].data_bitmap_block.data_block_bitmap[available_data_block_location] = true;
                //update inode
                file_storage[0][inode_block_idx].inode_block.inode[inode_idx].type = 1; //dir
                inode_LUT_update(&file_storage[0][inode_block_idx].inode_block.inode[inode_idx]);   //update LUT
                file_storage[0][inode_block_idx].inode_block.inode[inode_idx].direct[0]= data_block_idx;
                strcpy(file_storage[0][inode_block_idx].inode_block.inode[inode_idx].filename, cur_argv[1]);
                //update dir block
                strcpy(cur_dir_data_block->child_name[cur_dir_data_block->child_num], cur_argv[1]);
                cur_dir_data_block->child_location[cur_dir_data_block->child_num] = available_inode_location;
                cur_dir_data_block->child_num++;
                //get data block
                int data_block_cylinder = data_block_idx / SECTOR_PER_CYLINDER;
                int data_block_sector = data_block_idx % SECTOR_PER_CYLINDER;
                struct dir_block_struct *data_block = &(file_storage[data_block_cylinder][data_block_sector].dir_block);
                //add . and ..
                strcpy(data_block->child_name[0], ".");
                data_block->child_location[0] = available_inode_location;
                strcpy(data_block->child_name[1], "..");
                data_block->child_location[1] = cur_dir_inode_virtual_location;
                data_block->child_num = 2;
                //write to file
                fprintf(log, "YES: create directory %s\n", cur_argv[1]);
            }
            else if (strcmp(cur_argv[0], "ls") == 0) {
                //check and set ls_arg
                if (cur_argc != 1) {
                    printf("Error: invalid argument.\n");
                    fprintf(log, "NO: invalid argument\n");
                    break;
                }
                //print
                struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                struct dir_block_struct *cur_dir = get_dir_data_block(cur_dir_inode, file_storage, spb);
                //lexicographical order
                struct ls_file_struct file_temp[32];
                int file_temp_num = 0;
                struct ls_file_struct dir_temp[32];
                int dir_temp_num = 0;
                //get name and type
                for(int i = 0; i < cur_dir->child_num; i++) {
                    int child_inode_block_idx = cur_dir->child_location[i] / INODE_BLOCK_INODE_NUM;
                    int child_inode_idx = cur_dir->child_location[i] % INODE_BLOCK_INODE_NUM;
                    struct inode_struct* child_inode = (struct inode_struct *) (struct inode *) &file_storage[0][spb->inode_block_begin_location + child_inode_block_idx].inode_block.inode[child_inode_idx];
                    if (child_inode->type == 0) {
                        make_ls_struct(&file_temp[file_temp_num], child_inode, file_storage, spb);
                        file_temp_num++;
                    }
                    else if (child_inode->type == 1) {
                        if (strcmp(cur_dir->child_name[i], ".") == 0 || strcmp(cur_dir->child_name[i], "..") == 0) {
                            continue;
                        }
                        make_ls_struct(&dir_temp[dir_temp_num], child_inode, file_storage, spb);
                        dir_temp_num++;
                    }
                }
                //sort
                ls_sort(file_temp, file_temp_num);
                ls_sort(dir_temp, dir_temp_num);
                //print
                for (int i = 0; i < file_temp_num; i++) {
                    printf("%s  ", file_temp[i].type);
                    printf("%s    ", file_temp[i].filename);
                    printf("%d    ", file_temp[i].size);
                    printf("Last Update Time: %s\n", file_temp[i].last_modified_time);
                    fprintf(log, "%s  ", file_temp[i].type);
                    fprintf(log, "%s    ", file_temp[i].filename);
                    fprintf(log, "%d    ", file_temp[i].size);
                    fprintf(log, "Last Update Time: %s\n", file_temp[i].last_modified_time);
                }
                for (int i = 0; i < dir_temp_num; i++) {
                    printf("%s  ", dir_temp[i].type);
                    printf("%s    ", dir_temp[i].filename);
                    printf("%d    ", dir_temp[i].size);
                    printf("Last Update Time: %s\n", dir_temp[i].last_modified_time);
                    fprintf(log, "%s  ", dir_temp[i].type);
                    fprintf(log, "%s    ", dir_temp[i].filename);
                    fprintf(log, "%d    ", dir_temp[i].size);
                    fprintf(log, "Last Update Time: %s\n", dir_temp[i].last_modified_time);
                }
            }
            else if (strcmp(cur_argv[0], "e") == 0) {
                fprintf(log, "Goodbye!\n");
                fclose(log);
                return 0;
            }
            else {
                printf("Error: Unsupported command.\n");
                fprintf(log, "NO\n");
            }
        }
    }

}
