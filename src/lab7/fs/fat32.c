#include <fat32.h>
#include <printk.h>
#include <virtio.h>
#include <string.h>
#include <mbr.h>
#include <mm.h>

struct fat32_bpb fat32_header;

struct fat32_volume fat32_volume;

uint8_t fat32_buf[VIRTIO_BLK_SECTOR_SIZE];
uint8_t fat32_table_buf[VIRTIO_BLK_SECTOR_SIZE];

uint64_t cluster_to_sector(uint64_t cluster) {
    return (cluster - 2) * fat32_volume.sec_per_cluster + fat32_volume.first_data_sec;
}

uint32_t sector_to_cluster(uint64_t sector) {
    return (sector - fat32_volume.first_data_sec) / fat32_volume.sec_per_cluster + 2;
}

uint32_t next_cluster(uint64_t cluster) {
    uint64_t fat_offset = cluster * 4;
    uint64_t fat_sector = fat32_volume.first_fat_sec + fat_offset / VIRTIO_BLK_SECTOR_SIZE;
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    int index_in_sector = fat_offset % (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
    return *(uint32_t*)(fat32_table_buf + index_in_sector);
}

void fat32_init(uint64_t lba, uint64_t size) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    fat32_volume.first_fat_sec = lba + fat32_header.rsvd_sec_cnt;
    fat32_volume.sec_per_cluster = fat32_header.sec_per_clus;
    fat32_volume.first_data_sec = fat32_volume.first_fat_sec + fat32_header.fat_sz32 * fat32_header.num_fats;
    fat32_volume.fat_sz = fat32_header.fat_sz32;

    printk("fat32_volume.first_fat_sec: %d\n", fat32_volume.first_fat_sec);
    printk("fat32_volume.sec_per_cluster: %d\n", fat32_volume.sec_per_cluster);
    printk("fat32_volume.first_data_sec: %d\n", fat32_volume.first_data_sec);
    printk("fat32_volume.fat_sz: %d\n", fat32_volume.fat_sz);
}

int is_fat32(uint64_t lba) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    if (fat32_header.boot_sector_signature != 0xaa55) {
        return 0;
    }
    return 1;
}

int next_slash(const char* path) {
    int i = 0;
    while (path[i] != '\0' && path[i] != '/') {
        i++;
    }
    if (path[i] == '\0') {
        return -1;
    }
    return i;
}

void to_upper_case(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 32;
        }
    }
}

uint32_t count_clusters(uint32_t first_cluster) {
    uint32_t count = 0;

    while (first_cluster < 0x0FFFFFF8) {
        count++;
        first_cluster = next_cluster(first_cluster);
    }

    return count;
}

struct fat32_file fat32_open_file(const char *path) {
    struct fat32_file file;
    char path_copy[11];
    memset(path_copy, ' ', 11);

    size_t len = strlen(path);
    memcpy(path_copy, path+7, min(len-7,11));
    to_upper_case(path_copy);

    uint32_t sector = fat32_volume.first_data_sec;
    uint32_t cluster = sector_to_cluster(sector);
    uint32_t count = count_clusters(cluster);

    virtio_blk_read_sector(sector, fat32_buf);
    struct fat32_dir_entry *entry = (struct fat32_dir_entry *)fat32_buf;

    for(int i=0;i<fat32_volume.sec_per_cluster*count;i++){
        for(int j=0; j < FAT32_ENTRY_PER_SECTOR; j++){
            char name[11];
            memcpy(name, entry->name, 11);
            to_upper_case(name);

            if(memcmp(path_copy, entry->name, 11) == 0){
                file.cluster = (uint32_t)entry->starthi << 16 | entry->startlow;
                file.dir.index = j;
                file.dir.cluster = cluster;
                printk("[S] open file ");
                printk(path);
                return file;
            }
            entry++;
        }
        sector++;
        virtio_blk_read_sector(sector, fat32_buf);
    }
    printk("[S] file not found\n");
    memset(&file, 0, sizeof(struct fat32_file));
    return file;
}

uint32_t get_filesz(struct file* file){
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;
    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    return ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
}

int64_t fat32_lseek(struct file* file, int64_t offset, uint64_t whence) {
    if (whence == SEEK_SET) {
        file->cfo = offset;
    } else if (whence == SEEK_CUR) {
        file->cfo = file->cfo + offset;
    } else if (whence == SEEK_END) {
        /* Calculate file length */
        file->cfo = get_filesz(file) + offset;
    } else {
        printk("fat32_lseek: whence not implemented\n");
        while (1);
    }
    return file->cfo;
}

uint64_t fat32_table_sector_of_cluster(uint32_t cluster) {
    return fat32_volume.first_fat_sec + cluster / (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
}

int64_t fat32_extend_filesz(struct file* file, uint64_t new_size) {
    uint64_t sector = cluster_to_sector(file->fat32_file.dir.cluster) + file->fat32_file.dir.index / FAT32_ENTRY_PER_SECTOR;

    virtio_blk_read_sector(sector, fat32_table_buf);
    uint32_t index = file->fat32_file.dir.index % FAT32_ENTRY_PER_SECTOR;
    uint32_t original_file_len = ((struct fat32_dir_entry *)fat32_table_buf)[index].size;
    ((struct fat32_dir_entry *)fat32_table_buf)[index].size = new_size;

    virtio_blk_write_sector(sector, fat32_table_buf);

    uint32_t clusters_required = new_size / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t clusters_original = original_file_len / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    uint32_t new_clusters = clusters_required - clusters_original;

    uint32_t cluster = file->fat32_file.cluster;
    while (1) {
        uint32_t next_cluster_number = next_cluster(cluster);
        if (next_cluster_number >= 0x0ffffff8) {
            break;
        }
        cluster = next_cluster_number;
    }

    for (int i = 0; i < new_clusters; i++) {
        uint32_t cluster_to_append;
        for (int j = 2; j < fat32_volume.fat_sz * VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t); j++) {
            if (next_cluster(j) == 0) {
                cluster_to_append = j;
                break;
            }
        }
        uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
        virtio_blk_read_sector(fat_sector, fat32_table_buf);
        uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
        *(uint32_t*)(fat32_table_buf + index_in_sector) = cluster_to_append;
        virtio_blk_write_sector(fat_sector, fat32_table_buf);
        cluster = cluster_to_append;
    }

    uint64_t fat_sector = fat32_table_sector_of_cluster(cluster);
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    uint32_t index_in_sector = cluster * 4 % VIRTIO_BLK_SECTOR_SIZE;
    *(uint32_t*)(fat32_table_buf + index_in_sector) = 0x0fffffff;
    virtio_blk_write_sector(fat_sector, fat32_table_buf);

    return 0;
}

uint32_t find_cluster(uint32_t cluster, int64_t cfo){
    uint32_t cluster_offset = cfo / (fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE);
    for(int i=0;i<cluster_offset && cluster < 0xffffff8;i++){
        cluster = next_cluster(cluster);
    }
    return cluster;
}

int64_t fat32_read(struct file* file, void* buf, uint64_t len) {
    uint32_t filesz = get_filesz(file);

    uint64_t read_len = 0;
    while (read_len < len && file->cfo < filesz) {
        //计算当前簇
        uint32_t cluster = find_cluster(file->fat32_file.cluster, file->cfo);
        //计算当前扇区
        uint64_t sector = cluster_to_sector(cluster);
        //计算当前扇区内偏移
        uint64_t offset_in_sector = file->cfo % VIRTIO_BLK_SECTOR_SIZE;
        //计算当前扇区内剩余可读字节数
        uint64_t read_size = VIRTIO_BLK_SECTOR_SIZE - offset_in_sector;
        read_size = min(read_size, len - read_len);
        read_size = min(read_size, filesz - file->cfo);

        //读取扇区
        virtio_blk_read_sector(sector, fat32_buf);
        memcpy(buf, fat32_buf + offset_in_sector, read_size);

        file->cfo += read_size;
        buf += read_size;
        read_len += read_size;
    }

//    printk("\n[S] expect %d, read %d\n", len, read_len);

    return read_len;
}

int64_t fat32_write(struct file* file, const void* buf, uint64_t len) {
    uint64_t write_len = 0;
    while (len > 0) {
        //计算当前簇
        uint32_t cluster = find_cluster(file->fat32_file.cluster, file->cfo);
        //计算当前扇区
        uint64_t sector = cluster_to_sector(cluster);
        //计算当前扇区内偏移
        uint64_t offset_in_sector = file->cfo % VIRTIO_BLK_SECTOR_SIZE;
        //计算当前扇区内剩余可写字节数
        uint64_t write_size = VIRTIO_BLK_SECTOR_SIZE - offset_in_sector;
        if (write_size > len) {
            write_size = len;
        }

        //读取扇区
        virtio_blk_read_sector(sector, fat32_buf);
        memcpy(fat32_buf + offset_in_sector, buf, write_size);
        virtio_blk_write_sector(sector, fat32_buf);

        file->cfo += write_size;
        buf += write_size;
        len -= write_size;
        write_len += write_size;
    }
    return write_len;
}