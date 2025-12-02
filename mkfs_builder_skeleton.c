// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_minivsfs_final.c -o mkfs_minivsfs
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#define BS 4096u               // block size (fixed to 4096)
#define INODE_SIZE 128u
#define ROOT_INO 1u

// =============================== CRC32 (as provided) ===============================
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
// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
// This function expects that `sb` points to a full 4096-byte block image whose first
// bytes contain the superblock struct and the rest are zero.
#pragma pack(push,1)
typedef struct {
    uint32_t magic;             // 0x4D565346 "MVSF"
    uint32_t version;           // 1
    uint32_t block_size;        // 4096
    uint64_t total_blocks;      // total blocks in the image
    uint64_t inode_count;       // total inodes
    uint64_t inode_bitmap_start;
    uint64_t inode_bitmap_blocks;
    uint64_t data_bitmap_start;
    uint64_t data_bitmap_blocks;
    uint64_t inode_table_start;
    uint64_t inode_table_blocks;
    uint64_t data_region_start;
    uint64_t data_region_blocks;
    uint64_t root_inode;        // 1
    uint64_t mtime_epoch;       // build time
    uint32_t flags;             // 0
    uint32_t checksum;          // CRC32 over first 4092 bytes of the block
} superblock_t;
#pragma pack(pop)

static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}

// ====================== Inode (128 bytes) ======================
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
    uint64_t inode_crc;     // 8 bytes, low 4 bytes carry CRC
} inode_t;
#pragma pack(pop)

_Static_assert(sizeof(inode_t) == INODE_SIZE, "inode size must be 128 bytes");

// inode CRC finalize (as provided)
void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE]; memcpy(tmp, ino, INODE_SIZE);
    // zero crc area before computing
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c; // low 4 bytes carry the crc
}

// ====================== Dirent (64 bytes) ======================
#pragma pack(push,1)
typedef struct {
    uint32_t inode_no;      // 0 if free
    uint8_t  type;          // 1=file, 2=dir
    char     name[58];      // NUL-terminated if shorter
    uint8_t  checksum;      // XOR of first 63 bytes
} dirent64_t;
#pragma pack(pop)

_Static_assert(sizeof(dirent64_t) == 64, "dirent64_t must be 64 bytes");

// dirent checksum finalize (as provided)
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];   // covers ino(4) + type(1) + name(58)
    de->checksum = x;
}

// ====================== Helpers ======================
static inline uint64_t div_round_up_u64(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

static inline void set_bit(uint8_t *bitmap, uint64_t bit_index) {
    bitmap[bit_index >> 3u] |= (uint8_t)(1u << (bit_index & 7u));
}

// ====================== Main ======================
int main(int argc, char *argv[]) {
    char *output_file = NULL;
    uint64_t size_kib = 0;
    uint64_t num_inodes = 0;

    static struct option long_options[] = {
        {"image", required_argument, 0, 'i'},
        {"size-kib", required_argument, 0, 's'},
        {"inodes", required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i': output_file = optarg; break;
            case 's': size_kib = strtoull(optarg, NULL, 10); break;
            case 'n': num_inodes = strtoull(optarg, NULL, 10); break;
            default:
                fprintf(stderr, "Usage: %s --image <output_file> --size-kib <size> --inodes <count>\n", argv[0]);
                return 1;
        }
    }

    if (!output_file || size_kib == 0 || num_inodes == 0) {
        fprintf(stderr, "Usage: %s --image <output_file> --size-kib <size> --inodes <count>\n", argv[0]);
        return 1;
    }

    // Derived values
    uint64_t total_blocks = (size_kib * 1024u) / BS;
    if (total_blocks < 8) {
        fprintf(stderr, "Image too small; need at least 8 blocks.\n");
        return 1;
    }

    // Layout calculation
    const uint64_t bits_per_block = BS * 8u;
    const uint64_t inodes_per_block = BS / INODE_SIZE;

    uint64_t inode_bitmap_blocks = div_round_up_u64(num_inodes, bits_per_block);
    if (inode_bitmap_blocks == 0) inode_bitmap_blocks = 1; // allocate at least one block

    uint64_t inode_table_blocks = div_round_up_u64(num_inodes, inodes_per_block);

    // data bitmap depends on data region size; iterate to convergence
    uint64_t data_bitmap_blocks = 1; // start with 1 block
    for (int iter = 0; iter < 8; ++iter) {
        uint64_t used_except_data = 1 /*superblock*/ + inode_bitmap_blocks + inode_table_blocks;
        uint64_t data_region_blocks = (total_blocks > (used_except_data + data_bitmap_blocks))
                                        ? (total_blocks - used_except_data - data_bitmap_blocks)
                                        : 0;
        uint64_t need_dbm = div_round_up_u64(data_region_blocks, bits_per_block);
        if (need_dbm == 0) need_dbm = 1;
        if (need_dbm == data_bitmap_blocks) break;
        data_bitmap_blocks = need_dbm;
    }

    // Now finalize region sizes
    uint64_t used_except_data = 1 + inode_bitmap_blocks + inode_table_blocks + data_bitmap_blocks;
    if (total_blocks <= used_except_data) {
        fprintf(stderr, "Image too small for metadata with given inode count.\n");
        return 1;
    }
    uint64_t data_region_blocks = total_blocks - used_except_data;

    // Region starts
    uint64_t inode_bitmap_start = 1;
    uint64_t data_bitmap_start  = inode_bitmap_start + inode_bitmap_blocks;
    uint64_t inode_table_start  = data_bitmap_start + data_bitmap_blocks;
    uint64_t data_region_start  = inode_table_start + inode_table_blocks;

    // Prepare superblock block
    uint8_t sb_block[BS];
    memset(sb_block, 0, sizeof(sb_block));
    superblock_t *sb = (superblock_t*)sb_block;
    sb->magic = 0x4D565346u;
    sb->version = 1;
    sb->block_size = BS;
    sb->total_blocks = total_blocks;
    sb->inode_count = num_inodes;
    sb->inode_bitmap_start = inode_bitmap_start;
    sb->inode_bitmap_blocks = inode_bitmap_blocks;
    sb->data_bitmap_start = data_bitmap_start;
    sb->data_bitmap_blocks = data_bitmap_blocks;
    sb->inode_table_start = inode_table_start;
    sb->inode_table_blocks = inode_table_blocks;
    sb->data_region_start = data_region_start;
    sb->data_region_blocks = data_region_blocks;
    sb->root_inode = ROOT_INO;
    sb->mtime_epoch = (uint64_t)time(NULL);
    sb->flags = 0;
    crc32_init();
    superblock_crc_finalize(sb);

    // Open output
    int fd = open(output_file, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) { perror("open"); return 1; }

    // Write superblock block
    if (write(fd, sb_block, BS) != (ssize_t)BS) { perror("write superblock"); close(fd); return 1; }

    // Write inode bitmap blocks
    uint8_t *inode_bm = (uint8_t*)calloc(inode_bitmap_blocks, BS);
    if (!inode_bm) { perror("calloc inode_bm"); close(fd); return 1; }
    // Mark inode #1 allocated (bit 0)
    set_bit(inode_bm, 0);
    if (write(fd, inode_bm, inode_bitmap_blocks * BS) != (ssize_t)(inode_bitmap_blocks * BS)) {
        perror("write inode bitmap"); free(inode_bm); close(fd); return 1;
    }
    free(inode_bm);

    // Write data bitmap blocks
    uint8_t *data_bm = (uint8_t*)calloc(data_bitmap_blocks, BS);
    if (!data_bm) { perror("calloc data_bm"); close(fd); return 1; }
    // We will allocate one data block for root directory: this is the first data block
    set_bit(data_bm, 0); // bit 0 of data region
    if (write(fd, data_bm, data_bitmap_blocks * BS) != (ssize_t)(data_bitmap_blocks * BS)) {
        perror("write data bitmap"); free(data_bm); close(fd); return 1;
    }
    free(data_bm);

    // Write inode table
    uint64_t now = (uint64_t)time(NULL);
    inode_t zino; memset(&zino, 0, sizeof(zino));

    // root inode (index 0 => ino 1)
    inode_t root = zino;
    root.mode  = 040000;      // directory
    root.links = 2;           // '.' and '..'
    root.uid   = 0;
    root.gid   = 0;
    root.size_bytes = BS;     // one block
    root.atime = now;
    root.mtime = now;
    root.ctime = now;
    // Direct block pointers are absolute block numbers in the image
    root.direct[0] = (uint32_t)data_region_start; // first data block
    root.reserved_0 = 0;
    root.reserved_1 = 0;
    root.reserved_2 = 0;
    root.proj_id = 0;
    root.uid16_gid16 = 0;
    root.xattr_ptr = 0;
    inode_crc_finalize(&root);

    // Write entire inode table
    for (uint64_t i = 0; i < num_inodes; ++i) {
        if (i == 0) {
            if (write(fd, &root, sizeof(root)) != (ssize_t)sizeof(root)) { perror("write root inode"); close(fd); return 1; }
        } else {
            inode_t e = zino;
            inode_crc_finalize(&e);
            if (write(fd, &e, sizeof(e)) != (ssize_t)sizeof(e)) { perror("write empty inode"); close(fd); return 1; }
        }
    }

    // Write data region: first block = root directory with "." and ".."
    // Position the file to start of data region
    off_t data_start_off = (off_t)data_region_start * BS;
    if (lseek(fd, data_start_off, SEEK_SET) == (off_t)-1) { perror("lseek data start"); close(fd); return 1; }

    uint8_t data_block[BS]; memset(data_block, 0, sizeof(data_block));
    dirent64_t *de = (dirent64_t*)data_block;

    // "." entry
    de[0].inode_no = ROOT_INO;
    de[0].type = 2; // dir
    memset(de[0].name, 0, sizeof(de[0].name));
    strncpy(de[0].name, ".", sizeof(de[0].name)-1);
    dirent_checksum_finalize(&de[0]);

    // ".." entry (also root in a single-root FS)
    de[1].inode_no = ROOT_INO;
    de[1].type = 2;
    memset(de[1].name, 0, sizeof(de[1].name));
    strncpy(de[1].name, "..", sizeof(de[1].name)-1);
    dirent_checksum_finalize(&de[1]);

    if (write(fd, data_block, BS) != (ssize_t)BS) { perror("write root dir data"); close(fd); return 1; }

    // Pad file to requested size
    off_t target_size = (off_t)total_blocks * BS;
    if (ftruncate(fd, target_size) != 0) { perror("ftruncate"); close(fd); return 1; }

    close(fd);

    printf("MiniVSFS image '%s' created.\n", output_file);
    printf("  Blocks:          %" PRIu64 " (size: %" PRIu64 " KiB)\n", total_blocks, size_kib);
    printf("  Inodes:          %" PRIu64 "\n", num_inodes);
    printf("  Layout (blocks):\n");
    printf("    [0] superblock\n");
    printf("    [%" PRIu64 " .. %" PRIu64 "] inode bitmap (%" PRIu64 " blocks)\n",
       sb->inode_bitmap_start, sb->inode_bitmap_start + sb->inode_bitmap_blocks - 1,
       sb->inode_bitmap_blocks);

printf("    [%" PRIu64 " .. %" PRIu64 "] data bitmap (%" PRIu64 " blocks)\n",
       sb->data_bitmap_start, sb->data_bitmap_start + sb->data_bitmap_blocks - 1,
       sb->data_bitmap_blocks);

printf("    [%" PRIu64 " .. %" PRIu64 "] inode table (%" PRIu64 " blocks)\n",
       sb->inode_table_start, sb->inode_table_start + sb->inode_table_blocks - 1,
       sb->inode_table_blocks);

printf("    [%" PRIu64 " .. %" PRIu64 "] data region (%" PRIu64 " blocks)\n",
       sb->data_region_start, sb->data_region_start + sb->data_region_blocks - 1,
       sb->data_region_blocks);

    return 0;
}
