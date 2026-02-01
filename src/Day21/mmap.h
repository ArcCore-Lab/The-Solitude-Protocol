#ifndef MMAP_H
#define MMAP_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct MmapHeap *MmapHeapHandle;

typedef int (*MmapHeapCompare)(const void *a, const void *b);

typedef struct {
    size_t capacity;
    size_t size;
    size_t elem_size;
} MmapHeapMeta;

struct MmapHeap {
    int fd;
    void *mapped_region;
    size_t mapped_size;
    MmapHeapMeta *meta;
    void *heap_data;
    MmapHeapCompare cmp;
};

static int default_compare(const void *a, const void *b) {
    return memcmp(a, b, 1);
}

static inline size_t parent(size_t i) {
    return (i - 1) / 2;
}

static inline size_t left_child(size_t i) {
    return 2 * i + 1;
}

static inline size_t right_child(size_t i) {
    return 2 * i + 2;
}

static inline void *get_elem(struct MmapHeap *heap, size_t i) {
    return (char *)heap->heap_data + i * heap->meta->elem_size;
}

static void swap_elem(struct MmapHeap *heap, size_t i, size_t j) {
    void *tmp = malloc(heap->meta->elem_size);
    memcpy(tmp, get_elem(heap, i), heap->meta->elem_size);
    memcpy(get_elem(heap, i), get_elem(heap, j), heap->meta->elem_size);
    memcpy(get_elem(heap, j), tmp, heap->meta->elem_size);
    free(tmp);
}

static void sift_up(struct MmapHeap *heap, size_t i) {
    while (i > 0) {
        size_t p = parent(i);
        if (heap->cmp(get_elem(heap, i), get_elem(heap, p)) < 0) {
            swap_elem(heap, i, p);
            i = p;
        } else {
            break;
        }
    }
}

static void sift_down(struct MmapHeap *heap, size_t i) {
    size_t size = heap->meta->size;
    while (1) {
        size_t samllest = i;
        size_t left = left_child(i);
        size_t right = right_child(i);

        if (size > left && heap->cmp(get_elem(heap, left), get_elem(heap, samllest)) < 0) {
            samllest = left;
        }
        if (size > right && heap->cmp(get_elem(heap, right), get_elem(heap, samllest)) < 0) {
            samllest = right;
        }

        if (samllest != i) {
            swap_elem(heap, i, samllest);
            i = samllest;
        } else {
            break;
        }
    }
}

MmapHeapHandle mmap_heap_create(const char * filepath, size_t capacity, size_t elem_size, MmapHeapCompare cmp) {
    if (!filepath || capacity == 0 || elem_size == 0)
        return NULL;

    struct MmapHeap *heap = (struct MmapHeap *)malloc(sizeof(struct MmapHeap));
    if (!heap) return NULL;

    size_t meta_size = sizeof(MmapHeapMeta);
    size_t data_size = capacity * elem_size;
    size_t total_size = meta_size + data_size;

    int fd;
    if ((fd = open(filepath, O_CREAT | O_RDWR, 0644)) == -1) {
        free(heap);
        return NULL;
    }

    if (lseek(fd, total_size - 1, SEEK_SET) == -1) {
        close(fd);
        free(heap);
        return NULL;
    }
    if (write(fd, "", 1) == -1) {
        close(fd);
        free(heap);
        return NULL;
    }

    void *mapped = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        free(heap);
        return NULL;
    }

    heap->fd = fd;
    heap->mapped_region = mapped;
    heap->mapped_size = total_size;
    heap->meta = (MmapHeapMeta *)mapped;
    heap->heap_data = (char *)mapped + meta_size;
    heap->cmp = cmp ? cmp : default_compare;

    heap->meta->capacity = capacity;
    heap->meta->size = 0;
    heap->meta->elem_size = elem_size;

    return heap;
}

int mmap_heap_destory(MmapHeapHandle handle) {
    if (!handle) return -2;

    struct MmapHeap *heap = (struct MmapHeap *)handle;

    if (munmap(heap->mapped_region, heap->mapped_size) == -1)
        return -1;

    if (close(heap->fd) == -1)
        return -1;

    free(heap);
    return 0;
}

int mmap_heap_insert(MmapHeapHandle handle, const void *elem) {
    if (!handle || !elem) return -2;

    struct MmapHeap *heap = (struct MmapHeap *)handle;

    if (heap->meta->size >= heap->meta->capacity)
        return -1;

    size_t pos = heap->meta->size;
    memcpy(get_elem(heap, pos), elem, heap->meta->elem_size);
    heap->meta->size++;

    sift_up(heap, pos);

    return 0;
}

int mmap_heap_extract_min(MmapHeapHandle handle, void *out_elem) {
    if (!handle || !out_elem) return -2;

    struct MmapHeap *heap = (struct MmapHeap *)handle;

    if (heap->meta->size == 0) {
        return -1;
    }

    memcpy(out_elem, get_elem(heap, 0), heap->meta->elem_size);

    heap->meta->size--;
    if (heap->meta->size > 0) {
        memcpy(get_elem(heap, 0), get_elem(heap, heap->meta->size), heap->meta->elem_size);
        sift_down(heap, 0);
    }

    msync(heap->mapped_region, heap->mapped_size, MS_SYNC);

    return 0;
}

int mmap_heap_peek_min(MmapHeapHandle handle, void *out_elem) {
    if (!handle || !out_elem) return -2;

    struct MmapHeap *heap = (struct MmapHeap *)handle;

    if (heap->meta->size == 0) {
        return -1;
    }

    memcpy(out_elem, get_elem(heap, 0), heap->meta->elem_size);

    return 0;
}

size_t mmap_heap_size(MmapHeapHandle handle) {
    if (!handle) return 0;

    struct MmapHeap *heap = (struct MmapHeap *)handle;
    return heap->meta->size;
}

int mmap_heap_is_empty(MmapHeapHandle handle) {
    if (!handle) return -1;

    struct MmapHeap *heap = (struct MmapHeap *)handle;
    return heap->meta->size == 0 ? 1 : 0;
}

int mmap_heap_sync(MmapHeapHandle handle) {
    if (!handle) return -1;

    struct MmapHeap *heap = (struct MmapHeap *)handle;

    if (msync(heap->mapped_region, heap->mapped_size, MS_SYNC) == -1) {
        return -1;
    }

    return 0;
}

#endif /*MMAP_H*/