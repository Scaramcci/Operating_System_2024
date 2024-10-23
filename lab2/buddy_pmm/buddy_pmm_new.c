//2211999

#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>
#include <memlayout.h>

#define MAX_ORDER 10  // 伙伴系统最大支持的阶次为 10

free_area_t free_area[MAX_ORDER + 1];  // 每阶次的空闲块列表

// 初始化伙伴系统
static void buddy_init(void) {
    cprintf("Initializing buddy system...\n");
    for (int i = 0; i <= MAX_ORDER; i++) {
        list_init(&free_area[i].free_list);
        free_area[i].nr_free = 0;
    }
}

// 初始化内存映射
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    int order = MAX_ORDER;
    while ((1 << order) > n) {
        order--;
    }
    base->property = (1 << order);  // 设置当前页块的大小
    SetPageProperty(base);  // 设置页属性
    free_area[order].nr_free++;  // 增加空闲页数量
    list_add(&free_area[order].free_list, &(base->page_link));  // 插入到空闲链表中

    // 调试信息
    cprintf("Mapped %lu pages to order %d block, address=0x%p, nr_free=%d\n", n, order, base, free_area[order].nr_free);
}

// 分配指定大小的内存页
static struct Page *buddy_alloc_pages(size_t n) {
    int order = 0;
    while ((1 << order) < n) {
        order++;
    }
    if (order > MAX_ORDER) {
        return NULL;  // 请求的块太大
    }

    // 查找合适的块
    for (int current_order = order; current_order <= MAX_ORDER; current_order++) {
        if (!list_empty(&free_area[current_order].free_list)) {
            // 找到合适的块
            list_entry_t *le = list_next(&free_area[current_order].free_list);
            struct Page *page = le2page(le, page_link);
            list_del(le);  // 从空闲列表中删除
            free_area[current_order].nr_free--;

            // 逐级拆分大块，直到得到合适大小的块
            while (current_order > order) {
                current_order--;
                struct Page *buddy = page + (1 << current_order);
                buddy->property = (1 << current_order);  // 设置伙伴块大小
                SetPageProperty(buddy);  // 设置伙伴块属性
                free_area[current_order].nr_free++;
                list_add(&free_area[current_order].free_list, &(buddy->page_link));

                // 调试信息
                cprintf("Splitting block: new buddy address=0x%p, property=%d, new nr_free=%d\n", buddy, buddy->property, free_area[current_order].nr_free);
            }
            ClearPageProperty(page);  // 清除页属性
            page->property = n;
            cprintf("Allocating %lu pages, remaining nr_free=%d, page address: 0x%p\n", n, free_area[order].nr_free, page);
            return page;
        }
    }
    return NULL;  // 没有合适的块
}

// 释放已分配的内存页
static void buddy_free_pages(struct Page *base, size_t n) {
    int order = 0;
    while ((1 << order) < n) {
        order++;
    }

    if (PageProperty(base)) {
        cprintf("Error: Page at %p has already been freed!\n", base);
        return;
    }

    base->property = (1 << order);
    SetPageProperty(base);

    cprintf("Freeing %lu pages, new nr_free: %d, base address: 0x%p\n", n, free_area[order].nr_free + 1, base);

    // 尝试合并伙伴块
    while (order <= MAX_ORDER) {
        uintptr_t addr = page2pa(base);
        uintptr_t buddy_addr = addr ^ (1 << (PGSHIFT + order));
        struct Page *buddy = pa2page(buddy_addr);

        if (buddy_addr >= npage * PGSIZE || !PageProperty(buddy) || buddy->property != (1 << order)) {
            break;
        }

        // 合并块
        list_del(&(buddy->page_link));  // 从空闲链表中删除伙伴
        ClearPageProperty(buddy);  // 清除伙伴页的属性
        base = (base < buddy) ? base : buddy;  // 合并到较小地址的块
        order++;

        cprintf("Merging buddy with order %d, buddy property: %lu, new base address: 0x%p\n", order - 1, buddy->property, base);
    }

    list_add(&free_area[order].free_list, &(base->page_link));  // 插入合并后的空闲块
    free_area[order].nr_free++;  // 更新空闲块数量
}

// 统计当前的空闲页数量
static size_t buddy_nr_free_pages(void) {
    size_t total = 0;
    for (int i = 0; i <= MAX_ORDER; i++) {
        total += free_area[i].nr_free * (1 << i);
    }
    cprintf("Total free pages from buddy_nr_free_pages(): %lu\n", total);
    return total;
}

// 测试内存分配和释放
static void basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    list_entry_t free_list_store[MAX_ORDER + 1];
    unsigned int nr_free_store[MAX_ORDER + 1];

    for (int order = 0; order <= MAX_ORDER; order++) {
        free_list_store[order] = free_area[order].free_list;
        nr_free_store[order] = free_area[order].nr_free;
        list_init(&free_area[order].free_list);
        free_area[order].nr_free = 0;
    }

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);

    int total_free = 0;
    for (int order = 0; order <= MAX_ORDER; order++) {
        total_free += free_area[order].nr_free;
    }
    assert(total_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
}

// 定义伙伴系统内存管理器
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = basic_check,
};
