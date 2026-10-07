/*
 * sfx_format.h - On-disk layout of a Zipline self-extracting (SFX) executable.
 *
 * A Zipline SFX is a single Windows .exe built as:
 *
 *     [ native stub.exe (PE image) ][ payload blob ][ 32-byte footer ]
 *
 * The stub locates itself on disk, reads the trailing footer to find the
 * payload, decompresses/decrypts it and extracts the contained files, then
 * performs the configured post-extraction actions (open folder, run a file,
 * create shortcuts, PATH / registry integration, ...).
 *
 * This header is shared verbatim by the Windows stub (C) and the Zipline
 * engine (C++) so that both sides agree on the format byte-for-byte.
 * Everything is little-endian, which matches the x86-64 target.
 */
#ifndef ZIPLINE_SFX_FORMAT_H
#define ZIPLINE_SFX_FORMAT_H

#include <stdint.h>

/* Magic placed at the very start of the payload blob. */
#define SFX_PAYLOAD_MAGIC "ZSFXPLD1"
#define SFX_PAYLOAD_MAGIC_LEN 8

/* Magic stored in the 32-byte footer at the end of the file. */
#define SFX_FOOTER_MAGIC "ZIPLINE-SFX-v1\x00\x00"
#define SFX_FOOTER_MAGIC_LEN 16

/* Compression methods for the solid data stream. */
#define SFX_METHOD_STORE 0u
#define SFX_METHOD_LZMA2 1u

/* Footer: the last 32 bytes of the SFX executable. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[SFX_FOOTER_MAGIC_LEN]; /* SFX_FOOTER_MAGIC */
    uint64_t payload_offset;              /* byte offset of payload blob    */
    uint64_t payload_size;                /* length of payload blob         */
} SfxFooter;
#pragma pack(pop)

#define SFX_FOOTER_SIZE 32 /* 16 + 8 + 8 */

/*
 * Payload blob layout (variable length), in order:
 *
 *   char     magic[8]        = SFX_PAYLOAD_MAGIC
 *   uint32   settings_len
 *   char     settings[settings_len]   key\tvalue\n lines, '\n' in values
 *                                      escaped as the two chars '\' 'n'
 *   uint8    method                    SFX_METHOD_*
 *   uint8    encrypted                 0 / 1 (AES-256-CBC)
 *   uint8    salt[16]                  key-derivation salt (if encrypted)
 *   uint8    iv[16]                    AES IV (if encrypted)
 *   uint8    lzma_props                LZMA2 properties byte (dictionary)
 *   uint64   raw_size                  total size of all files concatenated
 *   uint64   comp_size                 size of the compressed stream
 *   uint64   stored_size               bytes actually stored (comp_size rounded
 *                                       up to a 16-byte AES block when encrypted)
 *   uint32   raw_crc32                 CRC-32 of the raw (uncompressed) stream
 *   uint32   file_count
 *   repeat file_count times:
 *       uint16  name_len
 *       char    name[name_len]         relative path, '/'-separated, UTF-8
 *       uint64  size                    uncompressed size of this entry
 *       uint32  attr                    reserved (0)
 *   uint8    data[stored_size]          solid stream (optionally AES-encrypted)
 */

#endif /* ZIPLINE_SFX_FORMAT_H */
