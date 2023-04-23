/* ԭ���Ĵ����ļ�����https://github.com/arjun024/memalloc/blob/master/memalloc.c */

/* malloc */
#include <pthread.h>// winƽ̨��Ҫpthread-win32
#include <stdio.h>
// ����һ�� char ���飬����Ϊ 16�������ڴ����
typedef char ALIGN[16];

// ����һ�������壬����һ���ṹ���һ�� char ����
// �ṹ������ڴ���С���Ƿ���к���һ���ڴ���ָ��
// char ���������ڴ����
union header {
	struct {
		size_t size; //��¼������ڴ��С
		unsigned is_free; //��¼�Ƿ�������룬�����ͷ��ڴ棬1���ǿ����ã�0���ǲ�����
		struct header_t* next; //���ǲ�����ȫȷ�� malloc ������ڴ���������ġ�Ϊ�˸���malloc������ڴ棬���ǽ����Ƿ��������С�
	} s;
	ALIGN stub;
};
// ����һ�� header_t ���͵�������
typedef union header header_t;

// ����ͷβָ�룬���ڸ����ڴ���б�
header_t* head, * tail;

// ����һ��ȫ���������ڷ�ֹ����߳�ͬʱ�����ڴ�
pthread_mutex_t global_malloc_lock;

// ��������ͷ�ṹ���СΪ 16 �ֽڵĴ������һ���װ�� a �С�
// ��ʹ�ñ�ͷ����λ���� 16 ���ֽڶ�����ڴ��ַ�ϡ�
// ����һ�£�����Ĺ�ģ�����Ա�Ľϴ��ģ��
// ��ˣ����ϱ�֤��ͷ��ĩβ���ڴ����ġ�
// ��ͷ��ĩβ��ʵ���ڴ�鿪ʼ��λ�ã���˷������ṩ�����÷����ڴ潫�� 16 ���ֽڶ��롣

// get_free_block() �����Ķ���
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
	// �������Ĵ�С�Ƿ�Ϊ�㡣����ǣ���ô���Ƿ���.
	if (!size)
		return NULL;
	// ������Ч�Ĵ�С���������Ȼ�ȡ����
	pthread_mutex_lock(&global_malloc_lock);
	// ���ǵ��� get_free_block() - �����������鿴�Ƿ��Ѿ����ڱ��Ϊ���в��ҿ������ɸ�����С���ڴ�顣
	// ��������ǲ������з�����������
	header_t* header = get_free_block(size);
	// ����ҵ��㹻��Ŀ��п飬���ǽ��򵥵ؽ��ÿ���Ϊ�����ɿ飬�ͷ�ȫ������Ȼ�󷵻�ָ��ÿ��ָ�롣
	// ����������£���ͷָ�뽫�������Ǹո�ͨ�������б��ҵ����ڴ��ı�ͷ���֡�
	// ���ס�����Ǳ������ⲿ�����ر�ͷ�Ĵ��ڡ�������������ʱ����ָ���ͷĩβ֮����ֽڡ�
	// ˳��˵һ�£���Ҳ��ʵ���ڴ��ĵ�һ���ֽڣ������÷�����Ȥ���Ǹ��ֽڡ��ⱻǿ��ת��Ϊ�����ء�
	if (header) {
		header->s.is_free = 0;
		pthread_mutex_unlock(&global_malloc_lock);
		return (void*)(header + 1);
	}
	// �������û���ҵ��㹻��Ŀ��п飬��ô���Ǳ���ͨ������ sbrk���� ����չ�ѡ�
	// �ѱ�����չ�ʺ������С�Ĵ�С�Լ���ͷ��Ϊ�ˣ��������ȼ����ܴ�С��
	size_t total_size = sizeof(header_t) + size;
	// ���ڣ������������ϵͳ���ӳ����жϣ�
	void* block = sbrk(total_size);
	// �������ϵͳ�޷��������ǵ������� sbrk���� ������ -1��
	if (block == (void*)-1) {
		pthread_mutex_unlock(&global_malloc_lock);
		return NULL;
	}
	// �ڴӲ���ϵͳ��õ��ڴ��У���������Ϊ��ͷ�ڳ��ռ䡣
	// �� C �У�����Ҫ�� a ת��Ϊ�κ�����ָ�����ͣ���ʼ���ǰ�ȫ�������������Ϊʲô����û����ȷ��������
	header = (header_t*)block;
	// ����������Ĵ�С���������ܴ�С����������ͷ����������Ϊ�����ɡ�
	header->s.size = size;
	header->s.is_free = 0;
	header->s.next = NULL;
	// ���Ǹ�����һ��ָ�롢ͷ��β���Է�ӳ�������״̬��
	if (!head)
		head = header;
	if (tail)
		tail->s.next = header;
	tail = header;
	// ��ǰ���������ǶԵ��÷����ر�ͷ����˷��ء�
	// ����ȷ������Ҳ�ͷ�ȫ������
	pthread_mutex_unlock(&global_malloc_lock);
	return (void*)(header + 1);
}

/* free() */
//����, ���ǽ�����free()Ӧ����ʲô�� free()��������ȷ�����ͷŵĿ��Ƿ�λ�ڶѵ�ĩβ�������, ���ǿ��Խ��䷢��������ϵͳ�� ����, ����������ֻ�ǽ�����Ϊ����ѡ�, ϣ���Ժ��ظ�ʹ������
void free(void* block)
{
	header_t* header, * tmp;//����header��tmpָ��  
	void* programbreak;//����programbreakָ��
	if (!block)//���blockΪ���򷵻�
		return;
	pthread_mutex_lock(&global_malloc_lock);//����
	header = (header_t*)block - 1; //�õ�����Ҫ�ͷŵĿ�ı�ͷ��������Ҫ���ľ��ǻ�ȡһ��ָ��,��ָ��λ�ڿ����ľ�����ڱ�ͷ�Ĵ�С�����,���ǽ���ת��Ϊ��ͷָ������,����������ƶ� 1 ����λ��
	programbreak = sbrk(0);//sbrk(0)���������жϵĵ�ǰֵ��
	if ((char*)block + header->s.size == programbreak) {//Ҫ���Ҫ�ͷŵĿ��Ƿ��ڶѵ�ĩβ,���������ҵ���ǰ���ĩβ���������Լ���Ϊ ��Ȼ����������жϽ��бȽϡ� 
		if (head == tail) {//�����ʵ�����������,��ô���ǿ�����С�ѵĴ�С�����ڴ��ͷŵ�����ϵͳ�����������������ǵ�ͷ��βָ��,�Է�ӳ���һ�������ʧ�� 
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
		sbrk(0 - sizeof(header_t) - header->s.size);//Ȼ�����Ҫ�ͷŵ��ڴ�����Ϊ���ͷ���ô���ڴ�,���������ֵ�ĸ������� sbrk()��
		pthread_mutex_unlock(&global_malloc_lock);//���� 
		return;
	}
	header->s.is_free = 1; //����鲻�������е����һ��,����ֻ������������is_free�ֶΡ����� get_free_block() �� malloc() ��ʵ�ʵ��� sbrk() ֮ǰ�����ֶΡ� 
	pthread_mutex_unlock(&global_malloc_lock);//����
}

/* calloc() */
// Ϊÿ��Ԫ�ص� nsize �ֽڵ� num Ԫ����������ڴ棬������ָ���ѷ����ڴ��ָ�롣���⣬�ڴ�ȫ������Ϊ�㡣
void* calloc(size_t num, size_t nsize)
{
	size_t size;
	void* block;
	// ���num��nsizeΪ0���򷵻�NULL
	if (!num || !nsize)
		return NULL;
	size = num * nsize;
	// ���˷����
	if (nsize != size / num)
		return NULL;
	// ����malloc()�����ڴ�
	block = malloc(size);
	if (!block)
		return NULL;
	// ʹ��memset()��������ڴ����Ϊ0
	memset(block, 0, size);
	return block;
}

/* realloc() */
// �ú����������ڴ��Ĵ�С����Ϊ�����Ĵ�С��
void* realloc(void* block, size_t size)
{
	// ��ȡ��ı�ͷ
	header_t* header;
	// ����ֵ
	void* ret;
	// ��� block �� size Ϊ 0���򷵻� malloc(size)
	if (!block || !size)
		return malloc(size);
	// ��ȡ��ı�ͷ
	header = (header_t*)block - 1;
	// ������Ѿ��������������С�Ĵ�С���򷵻� block
	if (header->s.size >= size)
		return block;
	// ���� malloc() ����ȡ�����С�Ŀ�
	ret = malloc(size);
	// ��� ret ��Ϊ NULL�����������¶�λ���µĸ���Ŀ���
	if (ret) {
		// ���ɿ�����ݸ��Ƶ��¿���
		memcpy(ret, block, header->s.size);
		// �ͷž��ڴ��
		free(block);
	}
	// �����¿�
	return ret;
}

