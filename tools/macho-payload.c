#include <errno.h>
#include <fcntl.h>
#include <mach-o/loader.h>
#include <mach/machine.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static _Noreturn void fail(const char *message)
{
    fprintf(stderr, "macho-payload: %s\n", message);
    exit(65);
}

static _Noreturn void fail_errno(const char *operation)
{
    fprintf(stderr, "macho-payload: %s: %s\n", operation, strerror(errno));
    exit(65);
}

static void write_all(const uint8_t *bytes, size_t length)
{
    while (length > 0) {
        ssize_t written = write(STDOUT_FILENO, bytes, length);
        if (written < 0) {
            fail_errno("write");
        }
        if (written == 0) {
            fail("write made no progress");
        }
        bytes += (size_t)written;
        length -= (size_t)written;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s thin-mach-o\n", argv[0]);
        return 64;
    }

    int descriptor = open(argv[1], O_RDONLY);
    if (descriptor < 0) {
        fail_errno("open");
    }

    struct stat status;
    if (fstat(descriptor, &status) != 0) {
        fail_errno("fstat");
    }
    if (status.st_size <= 0 || (uint64_t)status.st_size > SIZE_MAX) {
        fail("invalid file size");
    }

    size_t file_size = (size_t)status.st_size;
    uint8_t *bytes = malloc(file_size);
    if (bytes == NULL) {
        fail_errno("malloc");
    }

    size_t total_read = 0;
    while (total_read < file_size) {
        ssize_t amount = read(descriptor, bytes + total_read, file_size - total_read);
        if (amount < 0) {
            fail_errno("read");
        }
        if (amount == 0) {
            fail("unexpected end of file");
        }
        total_read += (size_t)amount;
    }
    if (close(descriptor) != 0) {
        fail_errno("close");
    }

    if (file_size < sizeof(struct mach_header_64)) {
        fail("file is too small for a 64-bit Mach-O header");
    }

    struct mach_header_64 *header = (struct mach_header_64 *)(void *)bytes;
    if (header->magic != MH_MAGIC_64) {
        fail("expected a little-endian 64-bit thin Mach-O file");
    }

    uint64_t commands_end = sizeof(*header) + (uint64_t)header->sizeofcmds;
    if (commands_end > file_size) {
        fail("load commands extend beyond the file");
    }

    uint8_t *cursor = bytes + sizeof(*header);
    uint8_t *commands_limit = bytes + commands_end;
    struct segment_command_64 *linkedit = NULL;
    struct linkedit_data_command *signature = NULL;

    for (uint32_t index = 0; index < header->ncmds; ++index) {
        if ((size_t)(commands_limit - cursor) < sizeof(struct load_command)) {
            fail("truncated load command");
        }

        struct load_command *command = (struct load_command *)(void *)cursor;
        if (command->cmdsize < sizeof(*command) || command->cmdsize > (uint32_t)(commands_limit - cursor)) {
            fail("invalid load-command size");
        }

        if (command->cmd == LC_SEGMENT_64) {
            if (command->cmdsize < sizeof(struct segment_command_64)) {
                fail("truncated 64-bit segment command");
            }
            struct segment_command_64 *segment = (struct segment_command_64 *)(void *)cursor;
            if (strncmp(segment->segname, SEG_LINKEDIT, sizeof(segment->segname)) == 0) {
                if (linkedit != NULL) {
                    fail("multiple __LINKEDIT segments");
                }
                linkedit = segment;
            }
        } else if (command->cmd == LC_CODE_SIGNATURE) {
            if (command->cmdsize < sizeof(struct linkedit_data_command)) {
                fail("truncated code-signature command");
            }
            if (signature != NULL) {
                fail("multiple code-signature commands");
            }
            signature = (struct linkedit_data_command *)(void *)cursor;
        }

        cursor += command->cmdsize;
    }

    if (cursor != commands_limit) {
        fail("load-command count and size disagree");
    }
    if (linkedit == NULL || signature == NULL) {
        fail("missing __LINKEDIT segment or LC_CODE_SIGNATURE");
    }

    uint64_t signature_end = (uint64_t)signature->dataoff + signature->datasize;
    if (signature->dataoff < commands_end || signature_end != file_size) {
        fail("code signature is not the final, bounded file region");
    }
    if (linkedit->fileoff > signature->dataoff) {
        fail("invalid __LINKEDIT and code-signature offsets");
    }
    if (linkedit->fileoff > UINT64_MAX - linkedit->filesize ||
        linkedit->fileoff + linkedit->filesize != file_size) {
        fail("__LINKEDIT does not end at the signed file boundary");
    }

    if (header->cputype != CPU_TYPE_X86_64 && header->cputype != CPU_TYPE_ARM64) {
        fail("unsupported CPU architecture");
    }
    /* Both slices in the pinned universal binaries use 16 KiB segments. */
    const uint64_t page_size = 16384;
    if (linkedit->filesize > UINT64_MAX - (page_size - 1)) {
        fail("__LINKEDIT size overflows page rounding");
    }
    uint64_t rounded_linkedit_size =
        (linkedit->filesize + page_size - 1) & ~(page_size - 1);
    if (linkedit->vmsize != rounded_linkedit_size) {
        fail("__LINKEDIT virtual size is inconsistent with its file size");
    }

    /*
     * codesign changes only the signature blob and the three length fields
     * describing it. Canonicalize those lengths while retaining the signature
     * offset, every other header/load-command byte, and every non-signature
     * byte in the file. The caller hashes this canonical byte stream.
     */
    uint64_t unsigned_linkedit_size = (uint64_t)signature->dataoff - linkedit->fileoff;
    linkedit->filesize = unsigned_linkedit_size;
    linkedit->vmsize = unsigned_linkedit_size;
    signature->datasize = 0;

    write_all(bytes, signature->dataoff);
    free(bytes);
    return 0;
}
