/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "scm.h"
#define checksum 1111

void *VIRT_ADDR = (void *)0x600000000000;

struct memory_block {
    void *ptr;
    size_t size;
    int is_allocated;
};

struct scm {
    int fd;
    void *mapped_addr;
    void *base;
    int size;
    size_t utilized;
    size_t capacity;
    struct memory_block *blocks;
    size_t block_count;
};

struct scm *scm_open(const char *pathname, int truncate) {
    struct stat st;
    int size;
    int* pointer;
    struct scm *scm = (struct scm *)malloc(sizeof(struct scm));
    scm->blocks = NULL;
    scm->block_count=0;
    if (!scm) {
        perror("Failed to allocate memory for SCM");
        return NULL;
    }

    scm->fd = open(pathname, O_RDWR, S_IRUSR | S_IWUSR);
    if (scm->fd == -1) {
        perror("Failed to open SCM file");
        free(scm);
        return NULL;
    }

    if (fstat(scm->fd, &st) == -1) {
        perror("Failed to get file stats");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "SCM file is not a regular file\n");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    scm->capacity = st.st_size;

    scm->mapped_addr = mmap((void*)VIRT_ADDR, scm->capacity, PROT_READ | PROT_WRITE, MAP_SHARED, scm->fd, 0);
    if (scm->mapped_addr == MAP_FAILED) {
        perror("Failed to map SCM file");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    pointer = (int *)scm->mapped_addr;
    if ((pointer[0]!=checksum) | truncate){
        pointer[0]=checksum;
        pointer[1]=0;
        size=pointer[1];
    }
    else{
        size = pointer[1];
    }
            
    if (truncate) {
        scm->utilized = 0;
    } else {
        scm->utilized = size;
    }

    scm->base = (char *)scm->mapped_addr+sizeof(size_t);

    return scm;
}

void scm_close(struct scm *scm) {
    int* pointer;
    if (scm) {
        if (scm_mbase(scm)) {
            pointer = (int *)scm->mapped_addr;
            pointer[0]=checksum;
            pointer[1]=scm_utilized(scm);
            msync(scm->mapped_addr, scm_capacity(scm), MS_SYNC);
            munmap(scm->mapped_addr, scm_capacity(scm));
        }
        if (scm->fd != -1) {
            close(scm->fd);
        }

        if (scm->blocks) {
            free(scm->blocks);
        }

        free(scm);
    }
}

void *scm_malloc(struct scm *scm, size_t n) {
    void *ptr;
    if (!scm) {
        return NULL;
    }

    if (scm_utilized(scm) + n > scm_capacity(scm)) {
        fprintf(stderr, "SCM is full, cannot allocate %lu bytes\n", (unsigned long)n);
        return NULL;
    }

    ptr = (void *)((char *)scm->base + scm_utilized(scm));
    scm->utilized += n;

    if (scm->blocks) {
        scm->blocks = (struct memory_block *)realloc(scm->blocks, (scm->block_count + 1) * sizeof(struct memory_block));
    } else {
        scm->blocks = (struct memory_block *)malloc(sizeof(struct memory_block));
    }

    if (scm->blocks) {
        scm->blocks[scm->block_count].ptr = ptr;
        scm->blocks[scm->block_count].size = n;
        scm->blocks[scm->block_count].is_allocated = 1;
        scm->block_count++;
    }

    return ptr;
}

char *scm_strdup(struct scm *scm, const char *s) {
    size_t len;
    char *ptr;
    if (!scm || !s) {
        return NULL;
    }

    len = strlen(s) + 1;
    ptr = (char *)scm_malloc(scm, len);
    if (ptr) {
        strcpy(ptr, s);
    }
    return ptr;
}

void scm_free(struct scm *scm, void *p) {
    size_t i;
    if (!scm || !p) {
        return;
    }

    for (i = 0; i < scm->block_count; i++) {
        if (scm->blocks[i].ptr == p && scm->blocks[i].is_allocated) {
            scm->blocks[i].is_allocated = 0;
            msync(scm->base, scm->utilized, MS_SYNC);
            scm->utilized -= scm->blocks[i].size;
            return;
        }
    }
}

size_t scm_utilized(const struct scm *scm) {
    if (scm) {
        return scm->utilized;
    }
    return 0;
}

size_t scm_capacity(const struct scm *scm) {
    if (scm) {
        return scm->capacity;
    }
    return 0;
}

void *scm_mbase(struct scm *scm) {
    if (scm) {
        return scm->base;
    }
    return NULL;
}