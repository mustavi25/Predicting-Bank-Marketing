#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <getopt.h>
#include <time.h>
#include <inttypes.h>

#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;               // 0x4D565346 "MVSF"
    uint32_t version;             // 1
    uint32_t block_size;          // 4096
    uint64_t total_blocks;        // total blocks in the image
    uint64_t inode_count;         // total inodes
    uint64_t inode_bitmap_start;
    uint64_t inode_bitmap_blocks;
    uint64_t data_bitmap_start;
    uint64_t data_bitmap_blocks;
    uint64_t inode_table_start;
    uint64_t inode_table_blocks;
    uint64_t data_region_start;
    uint64_t data_region_blocks;
    uint64_t root_inode;          // 1
    uint64_t mtime_epoch;         // build time
    uint32_t flags;               // 0
    uint32_t checksum;            // crc32(superblock[0..4091])
} superblock_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint16_t mode;          // file/dir mode bits (type encoded: 0100000 file, 0040000 dir)
    uint16_t links;         // link count
    uint32_t uid;           // owner uid
    uint32_t gid;           // owner gid
    uint64_t size_bytes;    // logical size
    uint64_t atime;         // access time (epoch)
    uint64_t mtime;         // modification time (epoch)
    uint64_t ctime;         // change time (epoch)
    uint32_t direct[12];    // absolute data block numbers
    uint32_t reserved_0;    // 0
    uint32_t reserved_1;    // 0
    uint32_t reserved_2;    // 0
    uint32_t proj_id;       // group/project id (set to gid or 0)
    uint32_t uid16_gid16;   // 0
    uint64_t xattr_ptr;     // 0
    uint64_t inode_crc;     // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0
} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t inode_no;      // 0 if free
    uint8_t  type;          // 1=file, 2=dir
    char     name[58];      // NUL-terminated if shorter
    uint8_t  checksum;      // XOR of bytes 0..62
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");

// ==========================DO NOT CHANGE THIS PORTION=========================
// These functions are there for your help. You should refer to the specifications to see how you can use them.
// ====================================CRC32====================================
uint32_t CRC32_TAB[256];
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
// ====================================CRC32====================================

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER INODE ELEMENTS HAVE BEEN FINALIZED
void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE]; 
    memcpy(tmp, ino, INODE_SIZE);
    // zero crc area before computing
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c; // low 4 bytes carry the crc
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER DIRENT ELEMENTS HAVE BEEN FINALIZED
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];   // covers ino(4) + type(1) + name(58)
    de->checksum = x;
}

// Helper functions
static inline uint64_t div_round_up_u64(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

static inline void set_bit(uint8_t *bitmap, uint64_t bit_index) {
    bitmap[bit_index >> 3u] |= (uint8_t)(1u << (bit_index & 7u));
}

static inline int test_bit(uint8_t *bitmap, uint64_t bit_index) {
    return (bitmap[bit_index >> 3u] & (1u << (bit_index & 7u))) != 0;
}

// Find first free inode in bitmap
uint32_t find_free_inode(uint8_t *inode_bitmap, uint64_t max_inodes) {
    for (uint64_t i = 1; i < max_inodes; i++) { // skip inode 0 (start from 1)
        if (!test_bit(inode_bitmap, i - 1)) { // bitmap bit i-1 for inode i
            return i;
        }
    }
    return 0; // no free inode
}

// Find first free data block in bitmap
uint32_t find_free_data_block(uint8_t *data_bitmap, uint64_t max_blocks) {
    for (uint64_t i = 0; i < max_blocks; i++) {
        if (!test_bit(data_bitmap, i)) {
            return i;
        }
    }
    return 0xFFFFFFFF; // no free block
}

// Add directory entry to root directory
int add_dirent_to_root(int fd, superblock_t *sb, uint32_t inode_no, const char *name, uint8_t type) {
    // Read root inode
    off_t root_inode_offset = sb->inode_table_start * BS; // root is first inode (index 0)
    if (lseek(fd, root_inode_offset, SEEK_SET) == -1) {
        perror("lseek root inode");
        return -1;
    }
    
    inode_t root_inode;
    if (read(fd, &root_inode, sizeof(root_inode)) != sizeof(root_inode)) {
        perror("read root inode");
        return -1;
    }
    
    // Read root directory data block
    if (root_inode.direct[0] == 0) {
        fprintf(stderr, "Root directory has no data block\n");
        return -1;
    }
    
    off_t root_data_offset = (off_t)root_inode.direct[0] * BS;
    if (lseek(fd, root_data_offset, SEEK_SET) == -1) {
        perror("lseek root data");
        return -1;
    }
    
    uint8_t data_block[BS];
    if (read(fd, data_block, BS) != BS) {
        perror("read root data");
        return -1;
    }
    
    // Find free directory entry slot
    dirent64_t *entries = (dirent64_t*)data_block;
    int entries_per_block = BS / sizeof(dirent64_t);
    int free_slot = -1;
    
    for (int i = 0; i < entries_per_block; i++) {
        if (entries[i].inode_no == 0) {
            free_slot = i;
            break;
        }
        // Check if file already exists
        if (entries[i].inode_no != 0 && strcmp(entries[i].name, name) == 0) {
            fprintf(stderr, "File '%s' already exists\n", name);
            return -1;
        }
    }
    
    if (free_slot == -1) {
        fprintf(stderr, "Root directory is full\n");
        return -1;
    }
    
    // Create new directory entry
    entries[free_slot].inode_no = inode_no;
    entries[free_slot].type = type;
    memset(entries[free_slot].name, 0, sizeof(entries[free_slot].name));
    strncpy(entries[free_slot].name, name, sizeof(entries[free_slot].name) - 1);
    dirent_checksum_finalize(&entries[free_slot]);
    
    // Write back the data block
    if (lseek(fd, root_data_offset, SEEK_SET) == -1) {
        perror("lseek root data write");
        return -1;
    }
    
    if (write(fd, data_block, BS) != BS) {
        perror("write root data");
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    char *image_file = NULL;
    char *source_file = NULL;
    char *dest_name = NULL;
    
    static struct option long_options[] = {
        {"image", required_argument, 0, 'i'},
        {"source", required_argument, 0, 's'},
        {"dest", required_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i': image_file = optarg; break;
            case 's': source_file = optarg; break;
            case 'd': dest_name = optarg; break;
            default:
                fprintf(stderr, "Usage: %s --image <fs_image> --source <file_to_add> --dest <name_in_fs>\n", argv[0]);
                return 1;
        }
    }
    
    if (!image_file || !source_file || !dest_name) {
        fprintf(stderr, "Usage: %s --image <fs_image> --source <file_to_add> --dest <name_in_fs>\n", argv[0]);
        return 1;
    }
    
    // Validate destination name length
    if (strlen(dest_name) >= 58) {
        fprintf(stderr, "Destination name too long (max 57 characters)\n");
        return 1;
    }
    
    crc32_init();
    
    // Open filesystem image
    int fs_fd = open(image_file, O_RDWR);
    if (fs_fd < 0) {
        perror("open filesystem image");
        return 1;
    }
    
    // Read superblock
    superblock_t sb;
    if (read(fs_fd, &sb, sizeof(sb)) != sizeof(sb)) {
        perror("read superblock");
        close(fs_fd);
        return 1;
    }
    
    // Verify magic number
    if (sb.magic != 0x4D565346u) {
        fprintf(stderr, "Invalid filesystem magic number\n");
        close(fs_fd);
        return 1;
    }
    
    // Open source file
    int src_fd = open(source_file, O_RDONLY);
    if (src_fd < 0) {
        perror("open source file");
        close(fs_fd);
        return 1;
    }
    
    // Get source file size
    struct stat src_stat;
    if (fstat(src_fd, &src_stat) < 0) {
        perror("stat source file");
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    uint64_t file_size = src_stat.st_size;
    uint64_t blocks_needed = div_round_up_u64(file_size, BS);
    
    if (blocks_needed > DIRECT_MAX) {
        fprintf(stderr, "File too large (max %u blocks supported)\n", DIRECT_MAX);
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    // Read inode bitmap
    off_t inode_bitmap_offset = sb.inode_bitmap_start * BS;
    if (lseek(fs_fd, inode_bitmap_offset, SEEK_SET) == -1) {
        perror("lseek inode bitmap");
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    uint8_t *inode_bitmap = malloc(sb.inode_bitmap_blocks * BS);
    if (!inode_bitmap) {
        perror("malloc inode bitmap");
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    if (read(fs_fd, inode_bitmap, sb.inode_bitmap_blocks * BS) != (ssize_t)(sb.inode_bitmap_blocks * BS)) {
        perror("read inode bitmap");
        free(inode_bitmap);
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    // Find free inode
    uint32_t new_inode = find_free_inode(inode_bitmap, sb.inode_count);
    if (new_inode == 0) {
        fprintf(stderr, "No free inodes available\n");
        free(inode_bitmap);
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    // Mark inode as allocated
    set_bit(inode_bitmap, new_inode - 1);
    
    // Read data bitmap
    off_t data_bitmap_offset = sb.data_bitmap_start * BS;
    if (lseek(fs_fd, data_bitmap_offset, SEEK_SET) == -1) {
        perror("lseek data bitmap");
        free(inode_bitmap);
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    uint8_t *data_bitmap = malloc(sb.data_bitmap_blocks * BS);
    if (!data_bitmap) {
        perror("malloc data bitmap");
        free(inode_bitmap);
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    if (read(fs_fd, data_bitmap, sb.data_bitmap_blocks * BS) != (ssize_t)(sb.data_bitmap_blocks * BS)) {
        perror("read data bitmap");
        free(data_bitmap);
        free(inode_bitmap);
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    // Find free data blocks
    uint32_t data_blocks[DIRECT_MAX];
    for (uint64_t i = 0; i < blocks_needed; i++) {
        data_blocks[i] = find_free_data_block(data_bitmap, sb.data_region_blocks);
        if (data_blocks[i] == 0xFFFFFFFF) {
            fprintf(stderr, "No free data blocks available\n");
            free(data_bitmap);
            free(inode_bitmap);
            close(src_fd);
            close(fs_fd);
            return 1;
        }
        set_bit(data_bitmap, data_blocks[i]);
    }
    
    // Create new inode
    inode_t new_inode_data;
    memset(&new_inode_data, 0, sizeof(new_inode_data));
    new_inode_data.mode = 0100000; // regular file
    new_inode_data.links = 1;
    new_inode_data.uid = 0;
    new_inode_data.gid = 0;
    new_inode_data.size_bytes = file_size;
    uint64_t now = time(NULL);
    new_inode_data.atime = now;
    new_inode_data.mtime = now;
    new_inode_data.ctime = now;
    
    // Set direct block pointers (convert relative to absolute)
    for (uint64_t i = 0; i < blocks_needed; i++) {
        new_inode_data.direct[i] = sb.data_region_start + data_blocks[i];
    }
    
    inode_crc_finalize(&new_inode_data);
    
    // Write file data to allocated blocks
    uint8_t *block_buffer = malloc(BS);
    if (!block_buffer) {
        perror("malloc block buffer");
        free(data_bitmap);
        free(inode_bitmap);
        close(src_fd);
        close(fs_fd);
        return 1;
    }
    
    for (uint64_t i = 0; i < blocks_needed; i++) {
        memset(block_buffer, 0, BS);
        
        ssize_t bytes_to_read = (file_size > BS) ? BS : file_size;
        ssize_t bytes_read = read(src_fd, block_buffer, bytes_to_read);
        
        if (bytes_read < 0) {
            perror("read source file data");
            free(block_buffer);
            free(data_bitmap);
            free(inode_bitmap);
            close(src_fd);
            close(fs_fd);
            return 1;
        }
        
        file_size -= bytes_read;
        
        // Write to filesystem
        off_t block_offset = (off_t)new_inode_data.direct[i] * BS;
        if (lseek(fs_fd, block_offset, SEEK_SET) == -1) {
            perror("lseek data block");
            free(block_buffer);
            free(data_bitmap);
            free(inode_bitmap);
            close(src_fd);
            close(fs_fd);
            return 1;
        }
        
        if (write(fs_fd, block_buffer, BS) != BS) {
            perror("write data block");
            free(block_buffer);
            free(data_bitmap);
            free(inode_bitmap);
            close(src_fd);
            close(fs_fd);
            return 1;
        }
    }
    
    free(block_buffer);
    close(src_fd);
    
    // Write new inode to inode table
    off_t inode_offset = sb.inode_table_start * BS + (new_inode - 1) * INODE_SIZE;
    if (lseek(fs_fd, inode_offset, SEEK_SET) == -1) {
        perror("lseek inode");
        free(data_bitmap);
        free(inode_bitmap);
        close(fs_fd);
        return 1;
    }
    
    if (write(fs_fd, &new_inode_data, sizeof(new_inode_data)) != sizeof(new_inode_data)) {
        perror("write inode");
        free(data_bitmap);
        free(inode_bitmap);
        close(fs_fd);
        return 1;
    }
    
    // Add directory entry to root
    if (add_dirent_to_root(fs_fd, &sb, new_inode, dest_name, 1) < 0) {
        free(data_bitmap);
        free(inode_bitmap);
        close(fs_fd);
        return 1;
    }
    
    // Write back updated bitmaps
    if (lseek(fs_fd, inode_bitmap_offset, SEEK_SET) == -1) {
        perror("lseek inode bitmap write");
        free(data_bitmap);
        free(inode_bitmap);
        close(fs_fd);
        return 1;
    }
    
    if (write(fs_fd, inode_bitmap, sb.inode_bitmap_blocks * BS) != (ssize_t)(sb.inode_bitmap_blocks * BS)) {
        perror("write inode bitmap");
        free(data_bitmap);
        free(inode_bitmap);
        close(fs_fd);
        return 1;
    }
    
    if (lseek(fs_fd, data_bitmap_offset, SEEK_SET) == -1) {
        perror("lseek data bitmap write");
        free(data_bitmap);
        free(inode_bitmap);
        close(fs_fd);
        return 1;
    }
    
    if (write(fs_fd, data_bitmap, sb.data_bitmap_blocks * BS) != (ssize_t)(sb.data_bitmap_blocks * BS)) {
        perror("write data bitmap");
        free(data_bitmap);
        free(inode_bitmap);
        close(fs_fd);
        return 1;
    }
    
    free(data_bitmap);
    free(inode_bitmap);
    close(fs_fd);
    
    printf("File '%s' added to filesystem as '%s' (inode %u)\n", source_file, dest_name, new_inode);
    return 0;
}
