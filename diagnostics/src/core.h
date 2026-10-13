#ifndef RE7_DIAG_CORE_H
#define RE7_DIAG_CORE_H
/* Portable, runtime-independent byte scanning. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
typedef __SIZE_TYPE__ usize;
#define DIAG_MAX_PATTERN 256
#define DIAG_MAX_MATCHES 8
#define DIAG_MAX_SECTIONS 96
#define DIAG_MAX_IMAGE (512u * 1024u * 1024u)
typedef struct { u8 bytes[DIAG_MAX_PATTERN], mask[DIAG_MAX_PATTERN]; u32 n, anchor, anchor_n; u16 shift[256]; } Pattern;
typedef struct { u64 count; u32 rva[DIAG_MAX_MATCHES]; } Matches;
typedef struct { char name[9]; u32 va, virtual_size, raw, raw_size, flags; } Section;
typedef struct { u32 timestamp, image_size, header_size, count, checksum; u64 preferred_base; Section sections[DIAG_MAX_SECTIONS]; } PE;
typedef struct { u32 h[8]; u64 total; u32 used; u8 block[64]; } Sha256;
void diag_zero(void*, usize);
void diag_copy(void*, const void*, usize);
int diag_equal(const void*, const void*, usize);
u16 diag_u16(const u8*);
u32 diag_u32(const u8*);
u64 diag_u64(const u8*);
int diag_pattern(Pattern*, const char*);
void diag_scan(const u8*, usize, u32, usize, const Pattern*, Matches*);
int diag_patch_rva(u32, int, u32, u32, u32*);
int diag_parse_pe(const u8*, usize, u64, PE*);
int diag_rva_raw(const PE*, u32, u32, u64*);
void diag_sha_init(Sha256*);
void diag_sha_update(Sha256*, const u8*, usize);
void diag_sha_final(Sha256*, u8[32]);
#endif
