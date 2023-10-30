/*
***************************************************************
This program is used to simulate the file system.
***************************************************************
 source file:
    fs.c

 compile:
    make

 usage:
    ./fs <DiskPort> <FSPort>
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>

#define SECTOR_SIZE 256
#define MAX_CMD_LEN 1024
#define MAX_FILE_NAME_LEN 8
#define INODE_BLOCK_INODE_NUM 8
#define MAX_INDIRECT_BLOCK_INODE_NUM 64
#define MAX_CHILD_NUM 20
#define INODE_DIRECT_NUM 4
#define INODE_INDIRECT_INDEX_NUM 64
#define BASIC_BLOCK_NUM 8

//fixed size in this program
int  CYLINDER_NUM;
int SECTOR_PER_CYLINDER;
int fs_port;
int disk_port;
int sockfd_between_disk_and_fs;
int sockfd_between_fs_and_client;

//check if the string is a number
int is_nature_number(char *str) {
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }
    return 1;
}

//convert string to nature number
int stoN(char *str) {
    int res = 0;
    for (int i = 0; i < strlen(str); i++) {
        res = res * 10 + (str[i] - '0');
    }
    return res;
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
    char child_name[MAX_CHILD_NUM][MAX_FILE_NAME_LEN];
    int child_location[MAX_CHILD_NUM]; //inode virtual location
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
    char last_modified_time[20];
    int size;
};
struct inode_struct {
    _Bool type;   //0: file, 1: directory
    struct LUT_struct LUT;
    char filename[MAX_FILE_NAME_LEN];
    int direct[INODE_DIRECT_NUM];
    int indirect;
};
//sector 3, 4, 5, 6
struct inode_block_struct {
    struct inode_struct inode[INODE_BLOCK_INODE_NUM];
};
struct indirect_block_struct {
    int direct[INODE_INDIRECT_INDEX_NUM];
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

//update disk
int update_sector[MAX_CMD_LEN];
int update_sector_num = 0;
bool data_block_in_memory[MAX_CMD_LEN];    //data block in memory or not
bool file_in_memory[32];          //file in memory or not

void get_sector_from_disk(int sector_idx, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]);

void transfer(int file_inode_idx, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb);

void sector_write_pre(int sector_idx) {
    update_sector[update_sector_num++] = sector_idx;
}

void sort(int a[], int n) {
    for(int i = 0; i < n; i++) {
        for(int j = i + 1; j < n; j++) {
            if(a[i] > a[j]) {
                int temp = a[i];
                a[i] = a[j];
                a[j] = temp;
            }
        }
    }
}
void update_disk(union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
//    printf("update_disk\n");
    //sort & unique
    sort(update_sector, update_sector_num);
//    for(int i = 0; i < update_sector_num; i++) {
//        printf("%d ", update_sector[i]);
//    }
//    printf("\n");
    int idx = 0;
    for(int i = 1; i < update_sector_num; i++) {
        if(update_sector[i] != update_sector[idx]) {
            update_sector[++idx] = update_sector[i];
        }
    }
    update_sector_num = idx + 1;
    //send
    for(int i = 0; i < update_sector_num; i++) {
        int cylinder = update_sector[i] / SECTOR_PER_CYLINDER;
        int sector = update_sector[i] % SECTOR_PER_CYLINDER;
        char op = 'W';
        send(sockfd_between_disk_and_fs, &op, 1, 0);
        send(sockfd_between_disk_and_fs, &cylinder, sizeof(int), 0);
        send(sockfd_between_disk_and_fs, &sector, sizeof(int), 0);
        send(sockfd_between_disk_and_fs, &file_storage[cylinder][sector], sizeof(union Sector_union), 0);
    }
    //clean
    update_sector_num = 0;
    memset(update_sector, 0, sizeof(update_sector));
}

//file ptr
struct file_ptr {
    char *ptr;
    int inner_idx;      //idx in cur section
    int section_idx;    //idx of section, such as direct[0]: 0, direct[1]: 1, direct[2]: 2, direct[3]: 3, indirect: 4, indirect[0]: 5, indirect[1]: 6
    struct inode_struct *cur_inode;
    bool open_mode;     //0: read, 1: write
};
//open mode: 0: read, 1: write
_Bool file_ptr_next(struct file_ptr *fp, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    _Bool change_section_flag = false;
    fp->inner_idx++;
    if (fp->inner_idx == SECTOR_SIZE) {
        fp->section_idx++;
        fp->inner_idx = 0;
        change_section_flag = true;
    }
    //reach the end of file
    if (fp->section_idx == 4 && fp->cur_inode->indirect == -1) {
        return false;
    }
    if (fp->section_idx < 4 && fp->cur_inode->direct[fp->section_idx] == -1) {
        return false;
    }
    if (fp->section_idx >= 4 && fp->cur_inode->indirect != -1 &&
        file_storage[fp->cur_inode->indirect / SECTOR_PER_CYLINDER][fp->cur_inode->indirect % SECTOR_PER_CYLINDER].indirect_block.direct[fp->section_idx - 4] == -1) {
        return false;
    }
    //update
    if (fp->section_idx < 4 && change_section_flag) {
        int cur_data_block_idx = fp->cur_inode->direct[fp->section_idx];
        if(fp->open_mode) sector_write_pre(cur_data_block_idx);
        int cur_data_block_cylinder = cur_data_block_idx / SECTOR_PER_CYLINDER;
        int cur_data_block_sector = cur_data_block_idx % SECTOR_PER_CYLINDER;
        fp->ptr = file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block.data;
    } else if (fp->section_idx >= 4 && change_section_flag) {
        int cur_indirect_block_idx = fp->cur_inode->indirect;
        if(fp->open_mode) sector_write_pre(cur_indirect_block_idx);
        int cur_indirect_block_cylinder = cur_indirect_block_idx / SECTOR_PER_CYLINDER;
        int cur_indirect_block_sector = cur_indirect_block_idx % SECTOR_PER_CYLINDER;
        int cur_data_block_idx = file_storage[cur_indirect_block_cylinder][cur_indirect_block_sector].indirect_block.direct[
                fp->section_idx - 4];
        if(fp->open_mode) sector_write_pre(cur_data_block_idx);
        int cur_data_block_cylinder = cur_data_block_idx / SECTOR_PER_CYLINDER;
        int cur_data_block_sector = cur_data_block_idx % SECTOR_PER_CYLINDER;
        fp->ptr = file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block.data;
    } else {
        fp->ptr++;
    }
    //reach the end of content
    if (*(fp->ptr) == '\0') {
        return false;
    }
    return true;
}

void make_file_ptr(struct file_ptr *fp, struct inode_struct *inode, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], bool open_mode) {
    fp->cur_inode = inode;
    fp->section_idx = 0;
    fp->inner_idx = 0;
    if (inode->direct[0] != -1) {
        if(fp->open_mode) sector_write_pre(inode->direct[0]);
        fp->ptr = file_storage[inode->direct[0] / SECTOR_PER_CYLINDER][inode->direct[0] % SECTOR_PER_CYLINDER].data_block.data;
    } else {
        fp->ptr = NULL;
    }
    fp->open_mode = open_mode;
}

//get inode
struct inode_struct *
get_inode(int inode_virtual_location, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    int cur_dir_inode_block_idx = inode_virtual_location / INODE_BLOCK_INODE_NUM;   //virtual block idx
    int cur_dir_inode_idx = inode_virtual_location % INODE_BLOCK_INODE_NUM;  //inode idx in block
    int inode_block_cdx = (cur_dir_inode_block_idx + spb->inode_block_begin_location) / SECTOR_PER_CYLINDER;
    int inode_block_sdx = (cur_dir_inode_block_idx + spb->inode_block_begin_location) % SECTOR_PER_CYLINDER;
    struct inode_struct *cur_inode = &(file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[cur_dir_inode_idx]);
    return cur_inode;
}

//get dir data block
struct dir_block_struct *get_dir_data_block(struct inode_struct *cur_dir_inode, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    int cur_dir_data_block_idx = cur_dir_inode->direct[0];
    int dir_data_block_cylinder = cur_dir_data_block_idx / SECTOR_PER_CYLINDER;
    int dir_data_block_sector = cur_dir_data_block_idx % SECTOR_PER_CYLINDER;
    struct dir_block_struct *cur_dir_data_block = &(file_storage[dir_data_block_cylinder][dir_data_block_sector].dir_block);
    return cur_dir_data_block;
}

//get data block
struct data_block_struct * get_data_block(int cur_data_block_idx,union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    int cur_data_block_cylinder = cur_data_block_idx / SECTOR_PER_CYLINDER;
    int cur_data_block_sector = cur_data_block_idx % SECTOR_PER_CYLINDER;
    struct data_block_struct *cur_data_block = &(file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block);
    return cur_data_block;
}

//bitmap check
_Bool bitmap_check(const _Bool *bitmap, int bitmap_size, int required_num, int q[]) {
    int count = 0;
    for (int i = 0; i < bitmap_size; i++) {
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
int get_file_size(struct inode_struct *cur_file, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    int cur_file_size = 0;
    for (int i = 0; i < 4; i++) {
        if (cur_file->direct[i] != -1) {
            cur_file_size += SECTOR_SIZE;
        }
    }
    if (cur_file->indirect != -1) {
        //get indirect data block
        int indirect_block_cylinder = cur_file->indirect / SECTOR_PER_CYLINDER;
        int indirect_block_sector = cur_file->indirect % SECTOR_PER_CYLINDER;
        struct indirect_block_struct *indirect_block = &(file_storage[indirect_block_cylinder][indirect_block_sector].indirect_block);
        for (int i = 0; i < MAX_INDIRECT_BLOCK_INODE_NUM; i++) {
            if (indirect_block->direct[i] != -1) {
                cur_file_size += SECTOR_SIZE;
            }
        }
    }
    return cur_file_size;
}

//get content size
int get_content_size(struct inode_struct *cur_file, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER],struct super_block_struct *spb) {
    int cur_content_size = 0;
    struct file_ptr fp;
    make_file_ptr(&fp, cur_file, file_storage, 0);
    do {
        if (*fp.ptr == '\0') {
            break;
        }
        cur_content_size++;
    } while (file_ptr_next(&fp, file_storage));
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
_Bool file_resize(struct inode_struct *cur_file, int size, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    //floor size
    if (size % SECTOR_SIZE != 0) {
        size = (size / SECTOR_SIZE + 1) * SECTOR_SIZE;
    }
    int DBC = 2 / SECTOR_PER_CYLINDER;
    int DBS = 2 % SECTOR_PER_CYLINDER;
    int cur_file_size = get_file_size(cur_file, file_storage);
    //file expand
    if (cur_file_size < size) {
        //expand
        int data_block_diff = (size - cur_file_size) / SECTOR_SIZE;
        int *data_block_idx = (int *) malloc(sizeof(int) * data_block_diff);
        //check remain space
        if (!bitmap_check(file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap, CYLINDER_NUM * SECTOR_PER_CYLINDER, data_block_diff,
                          data_block_idx)) {
            printf("no enough space\n");
            return false;
        }
        //set bitmap
        for (int i = 0; i < data_block_diff; i++) {
            file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[data_block_idx[i]] = true;
            sector_write_pre(data_block_idx[i]);
        }
        //set inode to add new data block
        int idx = 0;
        for (int j = 0; j < 4; j++) {
            if (cur_file->direct[j] == -1 && idx < data_block_diff) {
                cur_file->direct[j] = data_block_idx[idx++];
                //clean this data block
                int cur_data_block_cylinder = cur_file->direct[j] / SECTOR_PER_CYLINDER;
                int cur_data_block_sector = cur_file->direct[j] % SECTOR_PER_CYLINDER;
                struct data_block_struct *cur_data_block = &(file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block);
                for (int k = 0; k < SECTOR_SIZE; k++) {
                    cur_data_block->data[k] = '\0';
                }
            }
        }
        //if there is no remain space in direct block, set an indirect block
        if (idx < data_block_diff && cur_file->indirect == -1) {
            int indirect_block_idx = -1;
            if (!bitmap_check(file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap, CYLINDER_NUM * SECTOR_PER_CYLINDER, 1,
                              &indirect_block_idx)) {
                printf("no enough space\n");
                return false;
            }
            file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[indirect_block_idx] = true;
            sector_write_pre(indirect_block_idx);
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
            for (int j = 0; j < MAX_INDIRECT_BLOCK_INODE_NUM; j++) {
                if (indirect_block->direct[j] == -1 && idx < data_block_diff) {
                    indirect_block->direct[j] = data_block_idx[idx++];
                    //clean this data block
                    int cur_data_block_cylinder = indirect_block->direct[j] / SECTOR_PER_CYLINDER;
                    int cur_data_block_sector = indirect_block->direct[j] % SECTOR_PER_CYLINDER;
                    struct data_block_struct *cur_data_block = &(file_storage[cur_data_block_cylinder][cur_data_block_sector].data_block);
                    for (int k = 0; k < SECTOR_SIZE; k++) {
                        cur_data_block->data[k] = '\0';
                    }
                }
            }
        }
        free(data_block_idx);
    } else if (cur_file_size > size) {
        //shrink
        int data_block_diff = (cur_file_size - size) / SECTOR_SIZE;
        int *data_block_idx = (int *) malloc(sizeof(int) * data_block_diff);
        //get data block idx
        int idx = 0;
        if (cur_file->indirect != -1) {
            sector_write_pre(cur_file->indirect);
            int indirect_block_cylinder = cur_file->indirect / SECTOR_PER_CYLINDER;
            int indirect_block_sector = cur_file->indirect % SECTOR_PER_CYLINDER;
            struct indirect_block_struct *indirect_block = &(file_storage[indirect_block_cylinder][indirect_block_sector].indirect_block);
            int max_idx = MAX_INDIRECT_BLOCK_INODE_NUM - 1;
            for (; max_idx >= 0; max_idx--) {
                if (indirect_block->direct[max_idx] != -1) {
                    sector_write_pre(indirect_block->direct[max_idx]);
                    data_block_idx[idx++] = indirect_block->direct[max_idx];
                    indirect_block->direct[max_idx] = -1;
                    if (idx == data_block_diff) {
                        break;
                    }
                }
            }
            if (max_idx == -1) {
                file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[cur_file->indirect] = false;
                cur_file->indirect = -1;
            }
        }
        if (idx < data_block_diff) {
            for (int i = 3; i >= 0; i--) {
                if (cur_file->direct[i] != -1) {
                    sector_write_pre(cur_file->direct[i]);
                    data_block_idx[idx++] = cur_file->direct[i];
                    cur_file->direct[i] = -1;
                    if (idx == data_block_diff) {
                        break;
                    }
                }
            }
        }
        //set bitmap
        for (int i = 0; i < data_block_diff; i++) {
            file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[data_block_idx[i]] = false;
        }
        free(data_block_idx);
    }
    return true;
}

//file delete
_Bool file_delete(char *file_name, int cur_dir_inode_virtual_location, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    int IBC = 1 / SECTOR_PER_CYLINDER;
    int IBS = 1 % SECTOR_PER_CYLINDER;
    //get cur dir
    if(!file_in_memory[cur_dir_inode_virtual_location]) {
        transfer(cur_dir_inode_virtual_location, file_storage, spb);
    }
    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
    sector_write_pre(cur_dir_inode->direct[0]);
    //find file
    _Bool found = false;
    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
        struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
        if (child_inode->type == 1) {
            continue;
        }
        if (strcmp(cur_dir_data_block->child_name[i], file_name) == 0) {
            if(!file_in_memory[cur_dir_data_block->child_location[i]]) {
                transfer(cur_dir_data_block->child_location[i], file_storage, spb);
            }
            found = true;
            //delete file
            file_resize(child_inode, 0, file_storage);
            //set inode bitmap
            file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap[cur_dir_data_block->child_location[i]] = false;
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
_Bool dir_delete(char *dir_name, int cur_dir_inode_virtual_location, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    int IBC = 1 / SECTOR_PER_CYLINDER;
    int IBS = 1 % SECTOR_PER_CYLINDER;
    //get cur dir
    if(!file_in_memory[cur_dir_inode_virtual_location]) {
        transfer(cur_dir_inode_virtual_location, file_storage, spb);
    }
    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
    sector_write_pre(cur_dir_inode->direct[0]);
    //find file
    _Bool found = false;
    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
        struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
        if (child_inode->type == 0) {
            continue;
        }
        if (strcmp(cur_dir_data_block->child_name[i], dir_name) == 0) {
            if(!file_in_memory[cur_dir_data_block->child_location[i]]) {
                transfer(cur_dir_data_block->child_location[i], file_storage, spb);
            }
            sector_write_pre(child_inode->direct[0]);
            found = true;
            //delete all the content in the dir
            struct dir_block_struct *child_dir_data_block = get_dir_data_block(child_inode, file_storage, spb);
            //traverse all item in child_dir and delete
            for (int j = 0; j < child_dir_data_block->child_num; j++) {
                struct inode_struct *child_child_inode = get_inode(child_dir_data_block->child_location[j], file_storage, spb);
                //ignore . and ..
                if (strcmp(child_dir_data_block->child_name[j], ".") == 0 ||
                    strcmp(child_dir_data_block->child_name[j], "..") == 0) {
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
                    file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap[child_dir_data_block->child_location[i]] = false;
                    j = 0;
                }
            }
            //set inode bitmap
            file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap[cur_dir_data_block->child_location[i]] = false;
            //set data block bitmap
            int DBC = 2 / SECTOR_PER_CYLINDER;
            int DBS = 2 % SECTOR_PER_CYLINDER;
            file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[child_inode->direct[0]] = false;
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


int split(char *str, char *result[], char *delim) {
    char *p;
    p = strtok(str, delim);
    int i = 0;
    while (p != NULL) {
        result[i] = p;
        i++;
        p = strtok(NULL, delim);
    }
    return i;
}

bool m_kbhit() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return (FD_ISSET(0, &fds));
}

//dynamic support variable

int main(int argc, char *argv[]) {
    //check argc
    if (argc != 3) {
        printf("Usage: ./fs <DiskPort> <FSPort>\n");
        return 0;
    }
    if (!is_nature_number(argv[1]) ) {
        printf("Error: port number must be integer.\n");
        return 0;
    }
    if (!is_nature_number(argv[2]) ) {
        printf("Error: port number must be integer.\n");
        return 0;
    }
    disk_port = stoN(argv[1]);
    fs_port = stoN(argv[2]);
    char dest_ip[] = "127.0.0.1";
    //create disk log
    FILE *log = fopen("fs.log", "w+");
    if (log == NULL) {
        printf("Error: cannot create disk log file.\n");
        return 0;
    }
    //connect to disk
    struct sockaddr_in dest_addr;
    sockfd_between_disk_and_fs = socket(AF_INET, SOCK_STREAM, 0);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(disk_port);
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    bzero(&(dest_addr.sin_zero), 8);

    //try to connect to disk
    int connect_trys = 0;
    while(connect_trys < 5) {
        if (connect(sockfd_between_disk_and_fs, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr)) == -1) {
            if(connect_trys == 4) {
                printf("Connect failed, please set up disk.\n");
                return 0;
            } else {
                connect_trys++;
                printf("Retry...\n");
                sleep(2);
            }
        } else {
            printf("Connect to disk successfully\n");
            break;
        }
    }

    //receive the number of cylinder and number of sector per cylinder from disk
    char msg[MAX_CMD_LEN];
    recv(sockfd_between_disk_and_fs, msg, MAX_CMD_LEN, 0);
    char *msg_argv[2];
    split(msg, msg_argv, " ");
    CYLINDER_NUM = stoN(msg_argv[0]);
    SECTOR_PER_CYLINDER = stoN(msg_argv[1]);
    bzero(data_block_in_memory, sizeof(data_block_in_memory));

    //create file_storage
    union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER];
    _Bool inited = false;
    int cur_dir_inode_virtual_location = 0;

    //receive the file storage status code from disk
    memset(msg, 0, MAX_CMD_LEN);
    recv(sockfd_between_disk_and_fs, msg, MAX_CMD_LEN, 0);  //1: exist, 0: not exist
    int file_storage_status_code = stoN(msg);

    printf("CYLINDER_NUM: %d\n", CYLINDER_NUM);
    printf("SECTOR_PER_CYLINDER: %d\n", SECTOR_PER_CYLINDER);

    //status code: 1, file exist in disk, read from disk and init file_storage
    if (file_storage_status_code) {
        printf("file exist\n");
        inited = true;
        //receive file_storage from disk
        for (int i = 0; i < BASIC_BLOCK_NUM - 1; i++) {
            get_sector_from_disk(i, file_storage);
        }
        transfer(0, file_storage, &(file_storage[0][0].super_block));
        file_in_memory[0] = true;
    } else {
        printf("file not exist\n");
    }

    //dynamic disk support variable
    int IBC = 1 / SECTOR_PER_CYLINDER;  //inode bitmap cylinder index
    int IBS = 1 % SECTOR_PER_CYLINDER;  //inode bitmap sector index
    int DBC = 2 / SECTOR_PER_CYLINDER;  //data bitmap cylinder index
    int DBS = 2 % SECTOR_PER_CYLINDER;  //data bitmap sector index

    while(1) {
        _Bool client_exit_signal = false;
        _Bool file_system_exit_signal = false;
        // connect to client
        //create socket
        sockfd_between_fs_and_client = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_between_fs_and_client < 0) {
            perror("Error opening socket");
            exit(2);
        }
        //bind socket
        int bind_trials = 0;
        while(bind_trials < 5) {
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            serv_addr.sin_port = htons(fs_port);
            //bind and check error
            int bind_status = bind(sockfd_between_fs_and_client, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
            if (bind_status < 0) {
                if(bind_trials == 4) {
                    perror("Error binding socket");
                    exit(2);
                } else {
                    bind_trials++;
                    fs_port++;
                    printf("Retry port %d\n", fs_port);
                }
            } else {
                break;
            }
        }
        //listen
        int listen_status = listen(sockfd_between_fs_and_client, 5);
        if (listen_status < 0) {
            perror("Error listening");
            exit(2);
        }
        printf("File system is listening on port %d...\n", fs_port);
        //accept
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd_between_fs_and_client, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Error accepting");
            exit(2);
        }
        close(sockfd_between_fs_and_client);
        printf("File system is connected by client.\n");

        //receive request
        while (1) {
            //get command
            char cmd[MAX_CMD_LEN];
            memset(cmd, 0, MAX_CMD_LEN);
            memset(msg, 0, MAX_CMD_LEN);
            recv(newsockfd, cmd, MAX_CMD_LEN, 0);
            char *cur_argv[MAX_CMD_LEN];
            int cur_argc = split(cmd, cur_argv, " ");
            //print command
            printf("Command: ");
            for (int i = 0; i < cur_argc; i++) {
                printf("%s ", cur_argv[i]);
            }
            printf("\n");
            //super block pointer to increase readability
            struct super_block_struct *spb = &(file_storage[0][0].super_block);
            //check quit
            if(m_kbhit()) {
                char c;
                //read from stdin
                while (read(STDIN_FILENO, &c, 1) == 1) {
                    if(c == 'q' || c == '\t' || c == '\n') break;
                }
                //if q is pressed, wait for all clients exiting
                if(c == 'q') {
                    getchar();  //consume the newline
                    printf("Waiting for client exiting...\n");
                    file_system_exit_signal = true;
                }
            }

            //execute command
            for (int v = 0; v < 1; v++) {
                if (strcmp(cur_argv[0], "exit") == 0) {
                    client_exit_signal = true;
                    //exit if client exit
                    if(client_exit_signal && file_system_exit_signal) {
                        //transfer initial 8 sectors from disk to file_storage
                        for (int i = 0; i < BASIC_BLOCK_NUM; i++) {
                            sector_write_pre(i);
                        }
                        if (update_sector_num > 0) update_disk(file_storage);
                        //send exit signal to disk
                        char exit_cmd[MAX_CMD_LEN];
                        strcpy(exit_cmd, "E");
                        send(sockfd_between_disk_and_fs, exit_cmd, MAX_CMD_LEN, 0);
                        fprintf(log, "Goodbye!\n");
                        fclose(log);
                        return 0;
                    }
                    break;
                }
                //check init
                if (!inited) {
                    if (strcmp(cur_argv[0], "e") == 0) {
                        char exit_cmd[MAX_CMD_LEN];
                        strcpy(exit_cmd, "E");
                        send(sockfd_between_disk_and_fs, exit_cmd, MAX_CMD_LEN, 0);
                        fprintf(log, "Goodbye!\n");
                        fclose(log);
                        return 0;
                    }
                    int flag = strcmp(cur_argv[0], "f");
                    if (flag != 0) {
                        fprintf(log, "NO: file system not initialized.\n");
                        strcpy(msg, "Error: file system not initialized.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                }
                //execute command
                if (strcmp(cur_argv[0], "f") == 0) {
                    //check
                    if (cur_argc != 1) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    if (inited) {
                        fprintf(log, "NO: file system has been initialized.\n");
                        strcpy(msg, "Error: file system has been initialized.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
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
                    for (int i = 0; i < 32; i++) {
                        file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap[i] = false;
                    }
                    //init data block bitmap & set initial 8 blocks used
                    for (int i = 0; i < SECTOR_SIZE; i++) {
                        file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[i] = false;
                    }
                    for (int i = 0; i < 8; i++) {
                        file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[i] = true;
                    }
                    //init inode block
                    for (int i = 0; i < spb->inode_block_num; i++) {
                        int cdx = (i + spb->inode_block_begin_location) / SECTOR_PER_CYLINDER;
                        int sdx = (i + spb->inode_block_begin_location) % SECTOR_PER_CYLINDER;
                        for (int j = 0; j < INODE_BLOCK_INODE_NUM; j++) {
                            file_storage[cdx][sdx].inode_block.inode[j].type = -1;
                            for (int k = 0; k < INODE_DIRECT_NUM; k++) {
                                file_storage[cdx][sdx].inode_block.inode[j].direct[k] = -1;
                            }
                            file_storage[cdx][sdx].inode_block.inode[j].indirect = -1;
                        }
                    }
                    //init root dir block
                    int root_cdx = spb->data_block_begin_location / SECTOR_PER_CYLINDER;
                    int root_sdx = spb->data_block_begin_location % SECTOR_PER_CYLINDER;
                    file_storage[root_cdx][root_sdx].dir_block.child_num = 1; //.
                    for (int i = 0; i < MAX_CHILD_NUM; i++) {
                        file_storage[root_cdx][root_sdx].dir_block.child_location[i] = -1;
                    }
                    strcpy(file_storage[root_cdx][root_sdx].dir_block.child_name[0], ".");
                    file_storage[root_cdx][root_sdx].dir_block.child_location[0] = 0; //inode 0
                    file_storage[root_cdx][root_sdx].dir_block.parent = -1;
                    cur_dir_inode_virtual_location = 0;

                    file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap[0] = true;
                    //set dir inode
                    int inode_block_cdx = spb->inode_block_begin_location / SECTOR_PER_CYLINDER;
                    int inode_block_sdx = spb->inode_block_begin_location % SECTOR_PER_CYLINDER;
                    file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[0].type = 1;   //dir
                    file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[0].direct[0] = BASIC_BLOCK_NUM - 1;   //dir block
                    file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[0].filename[0] = '/';

                    inited = true;

                    //update disk
                    int total = CYLINDER_NUM * SECTOR_PER_CYLINDER;
                    for (int i = 0; i < total; i++) {
                        data_block_in_memory[i] = true;
                        sector_write_pre(i);
                    }
                    fprintf(log, "YES: file system initialized.\n");
                    strcpy(msg, "Success: file system initialized.\n");
                    send(newsockfd, msg, MAX_CMD_LEN, 0);
                    file_in_memory[0] = true;   //root dir
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "r") == 0) {
                    //get cur dir
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    //find file
                    int file_inode_virtual_location = -1;
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                            file_inode_virtual_location = cur_dir_data_block->child_location[i];
                            if(!file_in_memory[file_inode_virtual_location]) {
                                transfer(file_inode_virtual_location, file_storage, spb);
                                break;
                            }
                            break;
                        }
                    }
                    if (file_inode_virtual_location == -1) {
                        fprintf(log, "NO: file not found.\n");
                        strcpy(msg, "Error: file not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //get file inode
                    struct inode_struct *file_inode = get_inode(file_inode_virtual_location, file_storage, spb);
                    //resize
                    if (!is_nature_number(cur_argv[2])) {
                        fprintf(log, "NO: this argument must be a non-negative integer.\n");
                        strcpy(msg, "Error: this argument must be a non-negative integer.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    int target = stoN(cur_argv[2]);
                    file_resize(file_inode, target, file_storage);
                    if (update_sector_num > 0) update_disk(file_storage);
                    fprintf(log, "YES: file resized.\n");
                    strcpy(msg, "");
                    send(newsockfd, msg, MAX_CMD_LEN, 0);
                }
                else if (strcmp(cur_argv[0], "d") == 0) {
                    //check: d filename offset length
                    if (cur_argc != 4) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //find file
                    //get cur dir
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage,
                                                                                     spb);
                    //find file
                    _Bool found = false;
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
                        if (child_inode->type == 1) {
                            continue;
                        }
                        if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                            if(!file_in_memory[cur_dir_data_block->child_location[i]]) {
                                transfer(cur_dir_data_block->child_location[i], file_storage, spb);
                            }
                            found = true;
                            if (!is_nature_number(cur_argv[2])) {
                                fprintf(log, "NO: this argument must be a non-negative integer.\n");
                                strcpy(msg, "Error: this argument must be a non-negative integer.\n");
                                send(newsockfd, msg, MAX_CMD_LEN, 0);
                                break;
                            }
                            if (!is_nature_number(cur_argv[3])) {
                                fprintf(log, "NO: this argument must be a non-negative integer.\n");
                                strcpy(msg, "Error: this argument must be a non-negative integer.\n");
                                send(newsockfd, msg, MAX_CMD_LEN, 0);
                                break;
                            }
                            //init offset & len
                            int offset = stoN(cur_argv[2]);
                            int len = stoN(cur_argv[3]);
                            if (offset > get_content_size(child_inode, file_storage, spb)) {
                                //no need to delete
                                break;
                            }
                            //set len
                            if (offset + len > get_content_size(child_inode, file_storage, spb)) {
                                len = get_content_size(child_inode, file_storage, spb) - offset;
                            }
                            //set file ptr
                            struct file_ptr fp, fp2, end;
                            make_file_ptr(&fp, child_inode, file_storage, 1);
                            make_file_ptr(&fp2, child_inode, file_storage, 1);
                            make_file_ptr(&end, child_inode, file_storage, 1);
                            //delete
                            for (int j = 0; j < get_content_size(child_inode, file_storage, spb) - len; j++) {
                                file_ptr_next(&end, file_storage);
                            }
                            for (int j = 0; j < offset; j++) {
                                file_ptr_next(&fp, file_storage);
                            }
                            for (int j = 0; j < len; j++) {
                                *fp.ptr = '\0';
                                file_ptr_next(&fp, file_storage);
                            }
                            //move
                            fp2 = fp;
                            make_file_ptr(&fp, child_inode, file_storage, 1);
                            for (int j = 0; j < offset; j++) {
                                file_ptr_next(&fp, file_storage);
                            }
                            do {
                                *fp.ptr = *fp2.ptr;
                                file_ptr_next(&fp, file_storage);
                                file_ptr_next(&fp2, file_storage);
                            } while (*fp2.ptr != '\0');
                            //clean remain space
                            do {
                                *end.ptr = '\0';
                                file_ptr_next(&end, file_storage);
                            } while (*end.ptr != '\0');
                            //resize
                            file_resize(child_inode, get_content_size(child_inode, file_storage, spb), file_storage);
                            //update LTU
                            inode_LUT_update(child_inode);
                            inode_LUT_update(cur_dir_inode);
                            //send msg
                            fprintf(log, "YES: delete success.\n");
                            strcpy(msg, "");
                            send(newsockfd, msg, MAX_CMD_LEN, 0);
                        }
                    }
                    if (!found) {
                        fprintf(log, "NO: file not found.\n");
                        strcpy(msg, "Error: file not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "w") == 0) {
                    //check: w filename offset content
                    if (cur_argc < 4) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //find file
                    //get cur dir
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    //find file
                    _Bool found = false;
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i],file_storage, spb);
                        if (child_inode->type == 1) {
                            continue;
                        }
                        if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                            if(!file_in_memory[cur_dir_data_block->child_location[i]]) {
                                transfer(cur_dir_data_block->child_location[i], file_storage, spb);
                            }
                            found = true;
                            if (!is_nature_number(cur_argv[2])) {
                                fprintf(log, "NO: this argument must be a non-negative integer.\n");
                                strcpy(msg, "Error: this argument must be a non-negative integer.\n");
                                send(newsockfd, msg, MAX_CMD_LEN, 0);
                                break;
                            }
                            //set len
                            int len = stoN(cur_argv[2]);
                            //set write content
                            char content[MAX_CMD_LEN] = "";
                            for (int j = 3; j < cur_argc; j++) {
                                strcat(content, cur_argv[j]);
                                if (j != cur_argc - 1) {
                                    strcat(content, " ");
                                }
                            }
                            //handle "\n" to '\n'
                            for (int j = 0; j < strlen(content); j++) {
                                if (content[j] == '\\' && content[j + 1] == 'n') {
                                    content[j] = '\n';
                                    for (int k = j + 1; k < strlen(content); k++) {
                                        content[k] = content[k + 1];
                                    }
                                }
                            }
                            //check len and length of content
                            if (len < strlen(content)) {
                                content[len] = '\0';
                            } else if (len > strlen(content)) {
                                len = (int) strlen(content);
                            }
                            //resize
                            file_resize(child_inode, 0, file_storage);  //clean file
                            if (!file_resize(child_inode, (int) strlen(content), file_storage)) {
                                fprintf(log, "NO: no enough space.\n");
                                strcpy(msg, "Error: no enough space.\n");
                                send(newsockfd, msg, MAX_CMD_LEN, 0);
                                break;
                            }
                            char *content_ptr = content;
                            //set file ptr
                            struct file_ptr fp;
                            make_file_ptr(&fp, child_inode, file_storage, 1);
                            //write
                            while (*content_ptr != '\0') {
                                *fp.ptr = *content_ptr;
                                fp.ptr++;
                                content_ptr++;
                            }
                            inode_LUT_update(child_inode);
                            fprintf(log, "YES: write success.\n");
                            strcpy(msg, "");
                            send(newsockfd, msg, MAX_CMD_LEN, 0);
                        }
                    }
                    if (!found) {
                        fprintf(log, "NO: file not found.\n");
                        strcpy(msg, "Error: file not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "cat") == 0) {
                    //check
                    if (cur_argc != 2) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //find file
                    //get cur dir
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    //find file
                    _Bool found = false;
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i], file_storage, spb);
                        if (child_inode->type == 1) {
                            continue;
                        }
                        if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                            if(!file_in_memory[cur_dir_data_block->child_location[i]]) {
                                transfer(cur_dir_data_block->child_location[i], file_storage, spb);
                            }
                            found = true;
                            fprintf(log, "YES: ");
                            struct file_ptr fp;
                            char buf[MAX_CMD_LEN] = "";
                            int buf_len = 0;
                            make_file_ptr(&fp, child_inode, file_storage, 0);
                            do {
                                if (fp.ptr == NULL) break;
                                buf[buf_len] = *fp.ptr;
                                buf_len++;
                                fprintf(log, "%c", *fp.ptr);
                            } while (file_ptr_next(&fp, file_storage));
                            buf[buf_len++] = '\n';
                            buf[buf_len] = '\0';
                            send(newsockfd, buf, MAX_CMD_LEN, 0);
                            fprintf(log, "\n");
                        }
                    }
                    if (!found) {
                        fprintf(log, "NO: file not found.\n");
                        strcpy(msg, "Error: file not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "i") == 0) {
                    //check: i filename offset len content
                    if (cur_argc < 5) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //find file
                    //get cur dir
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    //find file
                    _Bool found = false;
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i],file_storage,spb);
                        if (child_inode->type == 1) {
                            continue;
                        }
                        if (strcmp(cur_dir_data_block->child_name[i], cur_argv[1]) == 0) {
                            if(!file_in_memory[cur_dir_data_block->child_location[i]]) {
                                transfer(cur_dir_data_block->child_location[i], file_storage, spb);
                            }
                            found = true;
                            int cur_file_max_length = get_file_size(child_inode, file_storage);
                            if(cur_file_max_length == 0) {
                                if(!file_resize(child_inode, SECTOR_SIZE, file_storage)) {
                                    fprintf(log, "NO: no enough space\n");
                                    strcpy(msg, "Error: no enough space.\n");
                                    send(newsockfd, msg, MAX_CMD_LEN, 0);
                                    break;
                                }
                            }
                            int cur_file_content_length = get_content_size(child_inode, file_storage, spb);
                            if (!is_nature_number(cur_argv[2])) {
                                fprintf(log, "NO: this argument must be a non-negative integer.\n");
                                strcpy(msg, "Error: this argument must be a non-negative integer.\n");
                                send(newsockfd, msg, MAX_CMD_LEN, 0);
                                break;
                            }
                            if (!is_nature_number(cur_argv[3])) {
                                fprintf(log, "NO: this argument must be a non-negative integer.\n");
                                strcpy(msg, "Error: this argument must be a non-negative integer.\n");
                                send(newsockfd, msg, MAX_CMD_LEN, 0);
                                break;
                            }
                            //set offset
                            int offset = stoN(cur_argv[2]);
                            if (offset > cur_file_content_length) {
                                //change offset to append content
                                offset = cur_file_content_length;
                            }
                            //set write content
                            char content[MAX_CMD_LEN] = "";
                            for (int j = 4; j < cur_argc; j++) {
                                strcat(content, cur_argv[j]);
                                if (j != cur_argc - 1) {
                                    strcat(content, " ");
                                }
                            }
                            //set len
                            int len = stoN(cur_argv[3]);
                            if (len > strlen(content)) {
                                len = (int) strlen(content);
                            } else if (len < strlen(content)) {
                                for(int j = len; j < strlen(content); j++) {
                                    content[j] = '\0';
                                }
                            }
                            //resize
                            if (strlen(content) + cur_file_content_length > cur_file_max_length) {
                                if (!file_resize(child_inode, (int) strlen(content) + cur_file_content_length, file_storage)) {
                                    fprintf(log, "NO\n");
                                    break;
                                }
                            }
                            char *content_ptr = content;
                            //set file ptr
                            struct file_ptr fp;
                            make_file_ptr(&fp, child_inode, file_storage, 1);
                            if (offset == cur_file_content_length) {
                                //append
                                if (cur_file_content_length != 0) while (file_ptr_next(&fp, file_storage));
                                while (*content_ptr != '\0') {
                                    *fp.ptr = *content_ptr;
                                    content_ptr++;
                                    file_ptr_next(&fp, file_storage);
                                }
                            } else {
                                //insert
                                //move content to get enough space
                                for (int j = 0; j < offset; j++) {
                                    file_ptr_next(&fp, file_storage);
                                }
                                //temporary store content after offset
                                char *cache = (char *) malloc(sizeof(char) * (cur_file_content_length - offset));
                                memset(cache, 0, sizeof(char) * (cur_file_content_length - offset));
                                char *cache_ptr = cache;
                                do {
                                    *cache_ptr = *fp.ptr;
                                    cache_ptr++;
                                } while (file_ptr_next(&fp, file_storage));
                                //write content
                                make_file_ptr(&fp, child_inode, file_storage, 1);
                                for (int j = 0; j < offset; j++) {
                                    file_ptr_next(&fp, file_storage);
                                }
                                while (*content_ptr != '\0') {
                                    *fp.ptr = *content_ptr;
                                    content_ptr++;
                                    file_ptr_next(&fp, file_storage);
                                }
                                //write cache
                                make_file_ptr(&fp, child_inode, file_storage, 1);

                                for (int j = 0; j < offset + strlen(content); j++) {
                                    file_ptr_next(&fp, file_storage);
                                }
                                cache_ptr = cache;
                                for (int j = 0; j < cur_file_content_length - offset; j++) {
                                    //handle '\n'
                                    if (*cache_ptr == '\\') {
                                        cache_ptr++;
                                        if (*cache_ptr == 'n') {
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
                            //send success message
                            fprintf(log, "YES: insert success.\n");
                            strcpy(msg, "");
                            send(newsockfd, msg, MAX_CMD_LEN, 0);
                        }
                    }
                    if (!found) {
                        fprintf(log, "NO: file not found.\n");
                        strcpy(msg, "Error: file not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "cd") == 0) {
                    if (cur_argc != 2) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    char *path[MAX_CMD_LEN];
                    //parse path
                    int path_len = split(cur_argv[1], (char **) path, "/");
                    //if absolute path
                    if (cur_argv[1][0] == '/') {
                        cur_dir_inode_virtual_location = 0;
                        if(path_len == 0) {
                            printf("found\n");
                            fprintf(log, "YES: cd success.\n");
                            strcpy(msg, "");
                            send(newsockfd, msg, MAX_CMD_LEN, 0);
                            break;
                        }
                    }
                    _Bool found = false;
                    //try to find dir
                    for (int j = 0; j < path_len; j++) {
                        //get cur dir
                        if(!file_in_memory[cur_dir_inode_virtual_location]) {
                            transfer(cur_dir_inode_virtual_location, file_storage, spb);
                        }
                        struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage,spb);
                        struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage,
                                                                                         spb);
                        for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                            struct inode_struct *child_inode = get_inode(cur_dir_data_block->child_location[i],file_storage, spb);
                            if (child_inode->type == 0) {
                                continue;
                            }
                            if (strcmp(cur_dir_data_block->child_name[i], path[j]) == 0) {
                                if(!file_in_memory[cur_dir_data_block->child_location[i]]) {
                                    transfer(cur_dir_data_block->child_location[i], file_storage, spb);
                                }
                                cur_dir_inode_virtual_location = cur_dir_data_block->child_location[i];
                                if(j == path_len - 1) {
                                    found = true;
                                }
                                break;
                            }
                        }
                    }
                    if(found) {
                        fprintf(log, "YES: cd success.\n");
                        printf("found\n");
                        strcpy(msg, "");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    } else {
                        printf("not found\n");
                        fprintf(log, "NO: dir not found.\n");
                        strcpy(msg, "Error: dir not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "rm") == 0) {
                    if(cur_argc != 2){
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    _Bool found = file_delete(cur_argv[1], cur_dir_inode_virtual_location, file_storage, spb);
                    if (!found) {
                        fprintf(log, "NO: file not found.\n");
                        strcpy(msg, "Error: file not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    } else {
                        fprintf(log, "YES: file delete success.\n");
                        strcpy(msg, "");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "rmdir") == 0) {
                    if(cur_argc != 2){
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    _Bool found = dir_delete(cur_argv[1], cur_dir_inode_virtual_location, file_storage, spb);
                    if (!found) {
                        fprintf(log, "NO: dir not found.\n");
                        strcpy(msg, "Error: dir not found.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    } else {
                        fprintf(log, "YES: dir delete success.\n");
                        strcpy(msg, "");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "mk") == 0) {
                    //get cur dir
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    sector_write_pre(cur_dir_inode->direct[0]);
                    //check
                    if (cur_argc != 2) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check name
                    if (strlen(cur_argv[1]) > MAX_FILE_NAME_LEN - 1) {
                        fprintf(log, "NO: file name too long.\n");
                        strcpy(msg, "Error: file name too long.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if name exists
                    _Bool name_exists = false;
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        if (strcmp(cur_argv[1], cur_dir_data_block->child_name[i]) == 0) {
                            name_exists = true;
                            break;
                        }
                    }
                    if (name_exists) {
                        fprintf(log, "NO: file name exists.\n");
                        strcpy(msg, "Error: file name exists.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if inode bitmap is full
                    int available_inode_location;
                    if (!bitmap_check(file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap, 32, 1,&available_inode_location)) {
                        fprintf(log, "NO: inode bitmap is full.\n");
                        strcpy(msg, "Error: inode bitmap is full.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if data block bitmap is full
                    int available_data_block_location;
                    if (!bitmap_check(file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap, CYLINDER_NUM * SECTOR_PER_CYLINDER, 1,&available_data_block_location)) {
                        fprintf(log, "NO: data block bitmap is full.\n");
                        strcpy(msg, "Error: data block bitmap is full.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if dir data block is full
                    if (cur_dir_data_block->child_num >= MAX_CHILD_NUM) {
                        fprintf(log, "NO: dir data block is full.\n");
                        strcpy(msg, "Error: dir data block is full.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //temp variables to increase readability
                    int inode_block_idx = spb->inode_block_begin_location + available_inode_location / INODE_BLOCK_INODE_NUM;
                    int inode_idx = available_inode_location % INODE_BLOCK_INODE_NUM;
                    int data_block_idx = available_data_block_location;
                    sector_write_pre(data_block_idx);
                    //create
                    //update inode bitmap
                    file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap[available_inode_location] = true;
                    //update data block bitmap
                    file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[available_data_block_location] = true;
                    //update inode
                    int inode_block_cdx = inode_block_idx / SECTOR_PER_CYLINDER;
                    int inode_block_sdx = inode_block_idx % SECTOR_PER_CYLINDER;
                    file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx].type = 0; //file
                    inode_LUT_update(&file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx]);   //update LUT
                    file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx].direct[0] = data_block_idx;
                    strcpy(file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx].filename, cur_argv[1]);
                    //update dir block
                    strcpy(cur_dir_data_block->child_name[cur_dir_data_block->child_num], cur_argv[1]);
                    cur_dir_data_block->child_location[cur_dir_data_block->child_num] = available_inode_location;
                    cur_dir_data_block->child_num++;
                    //clean new allocated data block
                    struct data_block_struct *data_block = get_data_block(data_block_idx, file_storage, spb);
                    for (int i = 0; i < SECTOR_SIZE; i++) {
                        data_block->data[i] = '\0';
                    }
                    //write to file
                    fprintf(log, "YES: create file %s\n", cur_argv[1]);
                    strcpy(msg, "");
                    send(newsockfd, msg, MAX_CMD_LEN, 0);
                    file_in_memory[inode_block_idx] = true;
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "mkdir") == 0) {
                    //get cur dir
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir_data_block = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    sector_write_pre(cur_dir_inode->direct[0]);
                    //check
                    if (cur_argc != 2) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check name
                    if (strlen(cur_argv[1]) > MAX_FILE_NAME_LEN - 1) {
                        fprintf(log, "NO: file name too long.\n");
                        strcpy(msg, "Error: file name too long.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if name exists
                    _Bool name_exists = false;
                    for (int i = 0; i < cur_dir_data_block->child_num; i++) {
                        if (strcmp(cur_argv[1], cur_dir_data_block->child_name[i]) == 0) {
                            name_exists = true;
                            break;
                        }
                    }
                    if (name_exists) {
                        fprintf(log, "NO: dir name exists.\n");
                        strcpy(msg, "Error: dir name exists.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if inode bitmap is full
                    int available_inode_location;
                    if (!bitmap_check(file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap, 32, 1,&available_inode_location)) {
                        fprintf(log, "NO: inode bitmap is full.\n");
                        strcpy(msg, "Error: inode bitmap is full.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if data block bitmap is full
                    int available_data_block_location;
                    if (!bitmap_check(file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap, CYLINDER_NUM * SECTOR_PER_CYLINDER, 1,&available_data_block_location)) {
                        fprintf(log, "NO: data block bitmap is full.\n");
                        strcpy(msg, "Error: data block bitmap is full.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //check if dir data block is full
                    if (cur_dir_data_block->child_num >= MAX_CHILD_NUM) {
                        fprintf(log, "NO: dir data block is full.\n");
                        strcpy(msg, "Error: dir data block is full.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //temp variables to increase readability
                    int inode_block_idx =spb->inode_block_begin_location + available_inode_location / INODE_BLOCK_INODE_NUM;
                    int inode_idx = available_inode_location % INODE_BLOCK_INODE_NUM;
                    int data_block_idx = available_data_block_location;
                    sector_write_pre(data_block_idx);
                    //create
                    //update inode bitmap
                    file_storage[IBC][IBS].inode_bitmap_block.inode_bitmap[available_inode_location] = true;
                    //update data block bitmap
                    file_storage[DBC][DBS].data_bitmap_block.data_block_bitmap[available_data_block_location] = true;
                    //update inode
                    int inode_block_cdx = inode_block_idx / SECTOR_PER_CYLINDER;
                    int inode_block_sdx = inode_block_idx % SECTOR_PER_CYLINDER;
                    file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx].type = 1; //file
                    inode_LUT_update(&file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx]);   //update LUT
                    file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx].direct[0] = data_block_idx;
                    strcpy(file_storage[inode_block_cdx][inode_block_sdx].inode_block.inode[inode_idx].filename, cur_argv[1]);
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
                    strcpy(msg, "");
                    send(newsockfd, msg, MAX_CMD_LEN, 0);
                    file_in_memory[inode_block_idx] = true;
                    if (update_sector_num > 0) update_disk(file_storage);
                }
                else if (strcmp(cur_argv[0], "ls") == 0) {
                    //check and set ls_arg
                    if (cur_argc != 1) {
                        fprintf(log, "NO: invalid argument.\n");
                        strcpy(msg, "Error: invalid argument.\n");
                        send(newsockfd, msg, MAX_CMD_LEN, 0);
                        break;
                    }
                    //print
                    if(!file_in_memory[cur_dir_inode_virtual_location]) {
                        transfer(cur_dir_inode_virtual_location, file_storage, spb);
                    }
                    struct inode_struct *cur_dir_inode = get_inode(cur_dir_inode_virtual_location, file_storage, spb);
                    struct dir_block_struct *cur_dir = get_dir_data_block(cur_dir_inode, file_storage, spb);
                    //lexicographical order
                    struct ls_file_struct file_temp[32];
                    int file_temp_num = 0;
                    struct ls_file_struct dir_temp[32];
                    int dir_temp_num = 0;
                    //get name and type
                    for (int i = 0; i < cur_dir->child_num; i++) {
                        transfer(cur_dir->child_location[i], file_storage, spb);
                        int child_inode_block_idx = cur_dir->child_location[i] / INODE_BLOCK_INODE_NUM;
                        int child_inode_idx = cur_dir->child_location[i] % INODE_BLOCK_INODE_NUM;
                        int cdx = (spb->inode_block_begin_location + child_inode_block_idx) / SECTOR_PER_CYLINDER;
                        int sdx = (spb->inode_block_begin_location + child_inode_block_idx) % SECTOR_PER_CYLINDER;
                        struct inode_struct *child_inode = (struct inode_struct *) (struct inode *) &file_storage[cdx][sdx].inode_block.inode[child_inode_idx];
                        if (child_inode->type == 0) {
                            make_ls_struct(&file_temp[file_temp_num], child_inode, file_storage, spb);
                            file_temp_num++;
                        } else if (child_inode->type == 1) {
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
                    char buf[MAX_CMD_LEN] = {0};
                    char temp[MAX_CMD_LEN] = {0};
                    for (int i = 0; i < file_temp_num; i++) {
                        sprintf(temp, "%s  ", file_temp[i].type);
                        strcat(buf, temp);
                        sprintf(temp, "%s    ", file_temp[i].filename);
                        strcat(buf, temp);
                        sprintf(temp, "%d    ", file_temp[i].size);
                        strcat(buf, temp);
                        sprintf(temp, "Last Update Time: %s\n", file_temp[i].last_modified_time);
                        strcat(buf, temp);
                        fprintf(log, "%s  ", file_temp[i].type);
                        fprintf(log, "%s    ", file_temp[i].filename);
                        fprintf(log, "%d    ", file_temp[i].size);
                        fprintf(log, "Last Update Time: %s\n", file_temp[i].last_modified_time);
                    }
                    for (int i = 0; i < dir_temp_num; i++) {
                        sprintf(temp, "%s  ", dir_temp[i].type);
                        strcat(buf, temp);
                        sprintf(temp, "%s    ", dir_temp[i].filename);
                        strcat(buf, temp);
                        sprintf(temp, "%d    ", dir_temp[i].size);
                        strcat(buf, temp);
                        sprintf(temp, "Last Update Time: %s\n", dir_temp[i].last_modified_time);
                        strcat(buf, temp);
                        fprintf(log, "%s  ", dir_temp[i].type);
                        fprintf(log, "%s    ", dir_temp[i].filename);
                        fprintf(log, "%d    ", dir_temp[i].size);
                        fprintf(log, "Last Update Time: %s\n", dir_temp[i].last_modified_time);
                    }
                    send(newsockfd, buf, MAX_CMD_LEN, 0);
                }
                else if (strcmp(cur_argv[0], "e") == 0) {
                    //transfer initial 8 sectors from disk to file_storage
                    for (int i = 0; i < BASIC_BLOCK_NUM; i++) {
                        sector_write_pre(i);
                    }
                    if (update_sector_num > 0) update_disk(file_storage);
                    //send exit signal to disk
                    char exit_cmd[MAX_CMD_LEN];
                    strcpy(exit_cmd, "E");
                    send(sockfd_between_disk_and_fs, exit_cmd, MAX_CMD_LEN, 0);
                    close(sockfd_between_disk_and_fs);
                    fprintf(log, "Goodbye!\n");
                    fclose(log);
                    return 0;
                }
                else {
                    fprintf(log, "NO: unsupported command.\n");
                    strcpy(msg, "Error: unsupported command.\n");
                    send(newsockfd, msg, MAX_CMD_LEN, 0);
                }
            }
            if(client_exit_signal) {
                printf("Client exit.\n");
                break;
            }
        }
    }
}


void get_sector_from_disk(int sector_idx, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER]) {
    int cdx = sector_idx / SECTOR_PER_CYLINDER;
    int sdx = sector_idx % SECTOR_PER_CYLINDER;
    char buf[SECTOR_SIZE];
    memset(buf, 0, SECTOR_SIZE);
    //send & recv
    char op = 'R';
    send(sockfd_between_disk_and_fs, &op, 1, 0);
    send(sockfd_between_disk_and_fs, &cdx, sizeof(int), 0);
    send(sockfd_between_disk_and_fs, &sdx, sizeof(int), 0);
    recv(sockfd_between_disk_and_fs, buf, SECTOR_SIZE, 0);
    memcpy(file_storage[cdx][sdx].data_block.data, buf, SECTOR_SIZE);
    data_block_in_memory[sector_idx] = true;
    printf("get sector %d %d from disk\n", cdx, sdx);
}

//transfer file/dir from disk to fs
void transfer(int file_inode_idx, union Sector_union file_storage[CYLINDER_NUM][SECTOR_PER_CYLINDER], struct super_block_struct *spb) {
    struct inode_struct *file = get_inode(file_inode_idx, file_storage, spb);
    for(int i = 0; i < INODE_DIRECT_NUM; i++) {
        if(file->direct[i] != -1) {
            if(!data_block_in_memory[file->direct[i]]) {
                get_sector_from_disk(file->direct[i], file_storage);
            }
        } else {
            break;
        }
        if(file->indirect != -1) {
            if(!data_block_in_memory[file->indirect]) {
                get_sector_from_disk(file->indirect, file_storage);
            }
            struct indirect_block_struct *indirect_block = (struct indirect_block_struct *)file_storage[file->indirect / SECTOR_PER_CYLINDER][file->indirect % SECTOR_PER_CYLINDER].data_block.data;
            for(int j = 0; j < MAX_INDIRECT_BLOCK_INODE_NUM; j++) {
                if(indirect_block->direct[j] != -1) {
                    if(!data_block_in_memory[indirect_block->direct[j]]) {
                        get_sector_from_disk(indirect_block->direct[j], file_storage);
                    }
                } else {
                    break;
                }
            }
        }
    }
    file_in_memory[file_inode_idx] = true;
}