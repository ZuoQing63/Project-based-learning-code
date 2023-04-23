/* 原本的代码文件见：https://github.com/arjun024/memalloc/blob/master/memalloc.c */

/* malloc */
#include <pthread.h>// win平台需要pthread-win32
#include <stdio.h>
// 定义一个 char 数组，长度为 16，用于内存对齐
typedef char ALIGN[16];

// 定义一个联合体，包含一个结构体和一个 char 数组
// 结构体包含内存块大小、是否空闲和下一个内存块的指针
// char 数组用于内存对齐
union header {
	struct {
		size_t size; //记录申请的内存大小
		unsigned is_free; //记录是否可以申请，用于释放内存，1就是可以用，0就是不能用
		struct header_t* next; //我们不能完全确定 malloc 分配的内存块是连续的。为了跟踪malloc分配的内存，我们将它们放在链表中。
	} s;
	ALIGN stub;
};
// 定义一个 header_t 类型的联合体
typedef union header header_t;

// 定义头尾指针，用于跟踪内存块列表
header_t* head, * tail;

// 定义一个全局锁，用于防止多个线程同时访问内存
pthread_mutex_t global_malloc_lock;

// 将整个标头结构与大小为 16 字节的存根变量一起包装在 a 中。
// 这使得标头最终位于与 16 个字节对齐的内存地址上。
// 回想一下，工会的规模是其成员的较大规模。
// 因此，联合保证标头的末尾是内存对齐的。
// 标头的末尾是实际内存块开始的位置，因此分配器提供给调用方的内存将与 16 个字节对齐。

// get_free_block() 函数的定义
header_t* get_free_block(size_t size)
{
	header_t* curr = head;
	while (curr) {
		if (curr->s.is_free && curr->s.size >= size)
			return curr;
		curr = curr->s.next;
	}
	return NULL;
}

void* malloc(size_t size)
{
	// 检查请求的大小是否为零。如果是，那么我们返回.
	if (!size)
		return NULL;
	// 对于有效的大小，我们首先获取锁。
	pthread_mutex_lock(&global_malloc_lock);
	// 我们调用 get_free_block() - 它遍历链表，查看是否已经存在标记为空闲并且可以容纳给定大小的内存块。
	// 在这里，我们采用先行方法搜索链表。
	header_t* header = get_free_block(size);
	// 如果找到足够大的空闲块，我们将简单地将该块标记为非自由块，释放全局锁，然后返回指向该块的指针。
	// 在这种情况下，标头指针将引用我们刚刚通过遍历列表找到的内存块的标头部分。
	// 请记住，我们必须向外部方隐藏标头的存在。当我们这样做时，它指向标头末尾之后的字节。
	// 顺便说一下，这也是实际内存块的第一个字节，即调用方感兴趣的那个字节。这被强制转换为并返回。
	if (header) {
		header->s.is_free = 0;
		pthread_mutex_unlock(&global_malloc_lock);
		return (void*)(header + 1);
	}
	// 如果我们没有找到足够大的空闲块，那么我们必须通过调用 sbrk（） 来扩展堆。
	// 堆必须扩展适合请求大小的大小以及标头。为此，我们首先计算总大小：
	size_t total_size = sizeof(header_t) + size;
	// 现在，我们请求操作系统增加程序中断：
	void* block = sbrk(total_size);
	// 如果操作系统无法满足我们的请求，则 sbrk（） 将返回 -1。
	if (block == (void*)-1) {
		pthread_mutex_unlock(&global_malloc_lock);
		return NULL;
	}
	// 在从操作系统获得的内存中，我们首先为标头腾出空间。
	// 在 C 中，不需要将 a 转换为任何其他指针类型，它始终是安全的提升。这就是为什么我们没有明确这样做：
	header = (header_t*)block;
	// 我们用请求的大小（而不是总大小）填充这个标头，并将其标记为非自由。
	header->s.size = size;
	header->s.is_free = 0;
	header->s.next = NULL;
	// 我们更新下一个指针、头和尾，以反映链表的新状态。
	if (!head)
		head = header;
	if (tail)
		tail->s.next = header;
	tail = header;
	// 如前所述，我们对调用方隐藏标头，因此返回。
	// 我们确保我们也释放全局锁。
	pthread_mutex_unlock(&global_malloc_lock);
	return (void*)(header + 1);
}

/* free() */
//现在, 我们将看看free()应该做什么。 free()必须首先确定待释放的块是否位于堆的末尾。如果是, 我们可以将其发布到操作系统。 否则, 我们所做的只是将其标记为“免费”, 希望以后重复使用它。
void free(void* block)
{
	header_t* header, * tmp;//声明header和tmp指针  
	void* programbreak;//声明programbreak指针
	if (!block)//如果block为空则返回
		return;
	pthread_mutex_lock(&global_malloc_lock);//加锁
	header = (header_t*)block - 1; //得到我们要释放的块的标头。我们需要做的就是获取一个指针,该指针位于块后面的距离等于标头的大小。因此,我们将块转换为标头指针类型,并将其向后移动 1 个单位。
	programbreak = sbrk(0);//sbrk(0)给出程序中断的当前值。
	if ((char*)block + header->s.size == programbreak) {//要检查要释放的块是否在堆的末尾,我们首先找到当前块的末尾。结束可以计算为 。然后将其与程序中断进行比较。 
		if (head == tail) {//如果它实际上是在最后,那么我们可以缩小堆的大小并将内存释放到操作系统。我们首先重置我们的头和尾指针,以反映最后一个块的损失。 
			head = tail = NULL;
		}
		else {
			tmp = head;
			while (tmp) {
				if (tmp->s.next == tail) {
					tmp->s.next = NULL;
					tail = tmp;
				}
				tmp = tmp->s.next;
			}
		}
		sbrk(0 - sizeof(header_t) - header->s.size);//然后计算要释放的内存量。为了释放这么多内存,我们用这个值的负数调用 sbrk()。
		pthread_mutex_unlock(&global_malloc_lock);//解锁 
		return;
	}
	header->s.is_free = 1; //如果块不是链表中的最后一个,我们只需设置其标题的is_free字段。这是 get_free_block() 在 malloc() 上实际调用 sbrk() 之前检查的字段。 
	pthread_mutex_unlock(&global_malloc_lock);//解锁
}

/* calloc() */
// 为每个元素的 nsize 字节的 num 元素数组分配内存，并返回指向已分配内存的指针。此外，内存全部设置为零。
void* calloc(size_t num, size_t nsize)
{
	size_t size;
	void* block;
	// 如果num或nsize为0，则返回NULL
	if (!num || !nsize)
		return NULL;
	size = num * nsize;
	// 检查乘法溢出
	if (nsize != size / num)
		return NULL;
	// 调用malloc()分配内存
	block = malloc(size);
	if (!block)
		return NULL;
	// 使用memset()将分配的内存清除为0
	memset(block, 0, size);
	return block;
}

/* realloc() */
// 该函数将给定内存块的大小更改为给定的大小。
void* realloc(void* block, size_t size)
{
	// 获取块的标头
	header_t* header;
	// 返回值
	void* ret;
	// 如果 block 或 size 为 0，则返回 malloc(size)
	if (!block || !size)
		return malloc(size);
	// 获取块的标头
	header = (header_t*)block - 1;
	// 如果块已经具有容纳请求大小的大小，则返回 block
	if (header->s.size >= size)
		return block;
	// 调用 malloc() 来获取请求大小的块
	ret = malloc(size);
	// 如果 ret 不为 NULL，则将内容重新定位到新的更大的块中
	if (ret) {
		// 将旧块的内容复制到新块中
		memcpy(ret, block, header->s.size);
		// 释放旧内存块
		free(block);
	}
	// 返回新块
	return ret;
}

