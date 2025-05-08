#include "memory_pool/memory_pool.hpp"
#include <vector>

using namespace yq_utils;

void test_seg_list_base()
{
	seg_list_allocator<int> alloc;
	int* p = alloc.allocate(1);
	*p = 1;
	alloc.deallocate(p, 1);
}

void test_seg_list_alloc_de_int(std::size_t scale)
{
	seg_list_allocator<int> alloc;
	int* p = alloc.allocate(scale);

	for (std::size_t i = 0; i < scale; ++i)
		p[i] = i;
	// 防止编译器优化
	for (int i = scale-1; i >= 0; --i)
	{
		assert(p[i] == i);
	}

	alloc.deallocate(p, scale);
}

void test_multiple_alloc_dealloc(std::size_t scale)
{
	seg_list_allocator<int> alloc;
	std::vector<int*> ptrs;
	for (std::size_t i = 0; i < scale; ++i)
	{
		ptrs.push_back(alloc.allocate(10));
	}
	for (auto p : ptrs)
	{
		alloc.deallocate(p, 10);
	}
}

auto main() -> int
{
	test_seg_list_base();
	for (std::size_t i = 2; i < 100'000; i *= 2)
	{
		test_seg_list_alloc_de_int(i);
	}

	for (std::size_t i = 2; i <= 2; ++i)
		test_multiple_alloc_dealloc(i);
}
