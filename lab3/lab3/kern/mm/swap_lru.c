//2211999 code challenge

#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>

extern list_entry_t pra_list_head, *curr_ptr;

/*
 * (2) _lru_init_mm: 初始化pra_list_head并让mm->sm_priv指向它
 */
static int
_lru_init_mm(struct mm_struct *mm)
{
    list_init(&pra_list_head);      // 初始化全局链表头       用于存储页面，保持 LRU 顺序
    curr_ptr = &pra_list_head;      // 设置当前指针为链表头
    mm->sm_priv = &pra_list_head;   // 将链表头赋值给 mm->sm_priv
    return 0;
}

/*
 * (3) _lru_map_swappable: 按照LRU算法将最最近访问的页面添加到队列尾部
 */
static int
_lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *entry = &(page->pra_page_link);// 获取页面的链表条目

    assert(entry != NULL && curr_ptr != NULL);// 确保 entry 和 curr_ptr 都不是 NULL

    // 如果页面已经在链表中，先将它从链表中删除
    if (entry->prev != NULL || entry->next != NULL) {
        list_del(entry);
    }

    list_add_before((list_entry_t*)mm->sm_priv, entry);// 将页面添加到链表的头部
    page->visited = 1;// 标记页面已被访问

    // 打印调试信息，确保与Clock算法一致
    cprintf("map_swappable: Added page at addr 0x%lx to the swap queue\n", addr);

    return 0;
}

/*
 * (4) _lru_swap_out_victim: 按照LRU算法选择一个待换出的页面
 */
static int
_lru_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    list_entry_t *head = (list_entry_t*)mm->sm_priv;// 获取链表头
    assert(head != NULL);
    assert(in_tick == 0);

    while (1) {
        curr_ptr = list_next(curr_ptr);// 获取链表的下一个节点
        if (curr_ptr == head) {
            curr_ptr = list_next(curr_ptr);// 如果当前指针回到了链表头，继续前进
            if (curr_ptr == head) {//如果当前节点回到了链表的头部，表示已经遍历了一圈。
                *ptr_page = NULL;// 如果遍历一圈都没有符合条件的页面，返回 NULL
                return 0;
            }
        }

        struct Page *page = le2page(curr_ptr, pra_page_link); // 获取当前页面

        // 如果页面没有被访问过，说明是最久未访问的页面，选中它并从队列中删除
        if (!page->visited) {
            *ptr_page = page;// 设置返回页面为当前页面
            list_del(curr_ptr);// 从链表中删除当前页面

            // 打印调试信息，确保与Clock算法一致
            cprintf("curr_ptr %p\n", curr_ptr);
            cprintf("swap_out: i 0, store page in vaddr 0x%lx to disk swap entry 2\n", (uintptr_t)page);

            return 0;
        } else {
            // 如果页面被访问过，重置访问标志，表示该页面已经被再次访问
            page->visited = 0;
        }
    }

    return 0;
}

/*
 * (5) _lru_check_swap: 检查交换的正确性
 */
static int
_lru_check_swap(void)
{
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 4);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 7);
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 8);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 9);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 10);
    cprintf("write Virt Page a in fifo_check_swap\n");
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 11);
    return 0;
}

/*
 * (6) 其他函数 (初始化，设置不可交换，时钟事件)
 */
static int
_lru_init(void)
{
    return 0;
}

static int
_lru_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_lru_tick_event(struct mm_struct *mm)
{
    return 0;
}

/*
 * LRU Swap Manager 实现
 */
struct swap_manager swap_manager_lru =
{
    .name            = "lru swap manager",
    .init            = &_lru_init,
    .init_mm         = &_lru_init_mm,
    .tick_event      = &_lru_tick_event,
    .map_swappable   = &_lru_map_swappable,
    .set_unswappable = &_lru_set_unswappable,
    .swap_out_victim = &_lru_swap_out_victim,
    .check_swap      = &_lru_check_swap,
};
