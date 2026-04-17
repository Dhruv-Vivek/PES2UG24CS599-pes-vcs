// commit.c — Commit object implementation

#include "commit.h"
#include "tree.h"
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ─────────────────────────────────────────────
// Read HEAD
// ─────────────────────────────────────────────

int head_read(ObjectID *id_out) {
    FILE *f = fopen(".pes/refs/heads/main", "r");

    if (!f)
        return -1;

    char hex[HASH_HEX_SIZE + 1];

    if (fscanf(f, "%64s", hex) != 1) {
        fclose(f);
        return -1;
    }

    fclose(f);

    return hex_to_hash(hex, id_out);
}

// ─────────────────────────────────────────────
// Update HEAD
// ─────────────────────────────────────────────

int head_update(const ObjectID *new_commit) {
    FILE *f = fopen(".pes/refs/heads/main", "w");

    if (!f)
        return -1;

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(new_commit, hex);

    fprintf(f, "%s\n", hex);

    fclose(f);
    return 0;
}

// ─────────────────────────────────────────────
// Parse commit object
// ─────────────────────────────────────────────

int commit_parse(const void *data, size_t len, Commit *commit_out) {
    (void)len;

    memset(commit_out, 0, sizeof(Commit));

    char *copy = strdup((const char *)data);
    if (!copy)
        return -1;

    char *line = strtok(copy, "\n");

    while (line) {
        if (strncmp(line, "tree ", 5) == 0) {
            hex_to_hash(line + 5, &commit_out->tree);
        }
        else if (strncmp(line, "parent ", 7) == 0) {
            hex_to_hash(line + 7, &commit_out->parent);
            commit_out->has_parent = 1;
        }
        else if (strncmp(line, "author ", 7) == 0) {
            sscanf(
                line + 7,
                "%255[^0-9] %lu",
                commit_out->author,
                &commit_out->timestamp
            );
        }
        else if (strlen(line) == 0) {
            line = strtok(NULL, "");
            if (line)
                strncpy(commit_out->message, line, sizeof(commit_out->message));
            break;
        }

        line = strtok(NULL, "\n");
    }

    free(copy);
    return 0;
}

// ─────────────────────────────────────────────
// Serialize commit object
// ─────────────────────────────────────────────

int commit_serialize(
    const Commit *commit,
    void **data_out,
    size_t *len_out
) {
    char tree_hex[HASH_HEX_SIZE + 1];
    char parent_hex[HASH_HEX_SIZE + 1];

    hash_to_hex(&commit->tree, tree_hex);

    if (commit->has_parent)
        hash_to_hex(&commit->parent, parent_hex);

    char *buffer = malloc(8192);
    if (!buffer)
        return -1;

    if (commit->has_parent) {
        sprintf(
            buffer,
            "tree %s\nparent %s\nauthor %s %lu\n\n%s\n",
            tree_hex,
            parent_hex,
            commit->author,
            commit->timestamp,
            commit->message
        );
    } else {
        sprintf(
            buffer,
            "tree %s\nauthor %s %lu\n\n%s\n",
            tree_hex,
            commit->author,
            commit->timestamp,
            commit->message
        );
    }

    *data_out = buffer;
    *len_out = strlen(buffer);

    return 0;
}

// ─────────────────────────────────────────────
// Create commit
// ─────────────────────────────────────────────

int commit_create(const char *message, ObjectID *commit_id_out) {
    Commit commit;
    memset(&commit, 0, sizeof(Commit));

    // Tree from index
    if (tree_from_index(&commit.tree) != 0)
        return -1;

    // Parent if exists
    if (head_read(&commit.parent) == 0) {
        commit.has_parent = 1;
    }

    strcpy(commit.author, pes_author());
    commit.timestamp = time(NULL);
    strncpy(commit.message, message, sizeof(commit.message));

    void *data;
    size_t len;

    if (commit_serialize(&commit, &data, &len) != 0)
        return -1;

    if (object_write(
            OBJ_COMMIT,
            data,
            len,
            commit_id_out
        ) != 0) {
        free(data);
        return -1;
    }

    free(data);

    return head_update(commit_id_out);
}

// ─────────────────────────────────────────────
// Walk commit history
// ─────────────────────────────────────────────

int commit_walk(commit_walk_fn callback, void *ctx) {
    ObjectID current;

    if (head_read(&current) != 0)
        return 0;

    while (1) {
        ObjectType type;
        void *data;
        size_t len;

        if (object_read(&current, &type, &data, &len) != 0)
            return -1;

        Commit commit;

        if (commit_parse(data, len, &commit) != 0) {
            free(data);
            return -1;
        }

        callback(&current, &commit, ctx);

        free(data);

        if (!commit.has_parent)
            break;

        current = commit.parent;
    }

    return 0;
}
