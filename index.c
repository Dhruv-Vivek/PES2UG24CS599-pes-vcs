// index.c — Staging area implementation

#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ─────────────────────────────────────────────
// Load index from .pes/index
// Format:
// <mode> <hash> <mtime_sec> <size> <path>
// ─────────────────────────────────────────────

int index_load(Index *index) {
    index->count = 0;

    FILE *f = fopen(".pes/index", "r");

    // No index file → empty index
    if (!f)
        return 0;

    while (!feof(f)) {
        IndexEntry e;
        char hash_hex[HASH_HEX_SIZE + 1];

        int rc = fscanf(
            f,
            "%o %64s %lu %u %511s\n",
            &e.mode,
            hash_hex,
            &e.mtime_sec,
            &e.size,
            e.path
        );

        if (rc == 5) {
            hex_to_hash(hash_hex, &e.hash);
            index->entries[index->count++] = e;
        }
    }

    fclose(f);
    return 0;
}

// ─────────────────────────────────────────────
// Save index atomically
// ─────────────────────────────────────────────

int index_save(const Index *index) {
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f)
        return -1;

    for (int i = 0; i < index->count; i++) {
        const IndexEntry *e = &index->entries[i];
        char hash_hex[HASH_HEX_SIZE + 1];

        hash_to_hex(&e->hash, hash_hex);

        fprintf(
            f,
            "%o %s %lu %u %s\n",
            e->mode,
            hash_hex,
            e->mtime_sec,
            e->size,
            e->path
        );
    }

    fclose(f);

    rename(".pes/index.tmp", ".pes/index");

    return 0;
}

// ─────────────────────────────────────────────
// Find file in index
// ─────────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }

    return NULL;
}

// ─────────────────────────────────────────────
// Add file to staging area
// ─────────────────────────────────────────────

int index_add(Index *index, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    struct stat st;
    if (stat(path, &st) != 0) {
        fclose(f);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    void *data = malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }

    fread(data, 1, size, f);
    fclose(f);

    ObjectID id;

    if (object_write(OBJ_BLOB, data, size, &id) != 0) {
        free(data);
        return -1;
    }

    free(data);

    IndexEntry *entry = index_find(index, path);

    if (!entry) {
        entry = &index->entries[index->count++];
    }

    strcpy(entry->path, path);
    entry->mode = 0100644;
    entry->size = st.st_size;
    entry->mtime_sec = st.st_mtime;
    entry->hash = id;

    return index_save(index);
}

// ─────────────────────────────────────────────
// Remove from index
// ─────────────────────────────────────────────

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            for (int j = i; j < index->count - 1; j++) {
                index->entries[j] = index->entries[j + 1];
            }

            index->count--;
            return index_save(index);
        }
    }

    return 0;
}

// ─────────────────────────────────────────────
// Status
// ─────────────────────────────────────────────

int index_status(const Index *index) {
    printf("Staged changes:\n");

    if (index->count == 0) {
        printf("  (nothing to show)\n");
        return 0;
    }

    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
    }

    return 0;
}
