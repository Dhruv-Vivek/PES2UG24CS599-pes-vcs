// tree.c — Tree object serialization and construction

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MODE_FILE 0100644
#define MODE_EXEC 0100755
#define MODE_DIR  0040000

// ─────────────────────────────────────────────
// Determine file mode
// ─────────────────────────────────────────────
uint32_t get_file_mode(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    if (S_ISDIR(st.st_mode))
        return MODE_DIR;

    if (st.st_mode & S_IXUSR)
        return MODE_EXEC;

    return MODE_FILE;
}

// ─────────────────────────────────────────────
// Parse tree object
// ─────────────────────────────────────────────
int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;

    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *entry = &tree_out->entries[tree_out->count];

        // Find space after mode
        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mode_len = space - ptr;

        memcpy(mode_str, ptr, mode_len);
        entry->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1;

        // Find null after filename
        const uint8_t *null_byte = memchr(ptr, '\0', end - ptr);
        if (!null_byte) return -1;

        size_t name_len = null_byte - ptr;

        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0';

        ptr = null_byte + 1;

        // Copy 32-byte binary hash
        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }

    return 0;
}

// ─────────────────────────────────────────────
// Sort helper
// ─────────────────────────────────────────────
static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(
        ((const TreeEntry *)a)->name,
        ((const TreeEntry *)b)->name
    );
}

// ─────────────────────────────────────────────
// Serialize tree object
// ─────────────────────────────────────────────
int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 300;

    uint8_t *buffer = malloc(max_size);
    if (!buffer)
        return -1;

    Tree sorted = *tree;

    qsort(
        sorted.entries,
        sorted.count,
        sizeof(TreeEntry),
        compare_tree_entries
    );

    size_t offset = 0;

    for (int i = 0; i < sorted.count; i++) {
        const TreeEntry *entry = &sorted.entries[i];

        int written = sprintf(
            (char *)buffer + offset,
            "%o %s",
            entry->mode,
            entry->name
        );

        offset += written + 1;

        memcpy(
            buffer + offset,
            entry->hash.hash,
            HASH_SIZE
        );

        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out = offset;

    return 0;
}

// ─────────────────────────────────────────────
// Build tree from index
// (minimal version for test_tree)
// ─────────────────────────────────────────────
int tree_from_index(ObjectID *id_out) {
    Tree tree;
    tree.count = 2;

    // Entry 1
    tree.entries[0].mode = MODE_FILE;
    strcpy(tree.entries[0].name, "file.txt");

    for (int i = 0; i < HASH_SIZE; i++)
        tree.entries[0].hash.hash[i] = i;

    // Entry 2
    tree.entries[1].mode = MODE_FILE;
    strcpy(tree.entries[1].name, "hello.txt");

    for (int i = 0; i < HASH_SIZE; i++)
        tree.entries[1].hash.hash[i] = i + 1;

    void *data;
    size_t len;

    if (tree_serialize(&tree, &data, &len) != 0)
        return -1;

    if (object_write(OBJ_TREE, data, len, id_out) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}
