#include "memory_pool/block_storage.hpp"
#include <cassert>
#include <new>
#include <vector>
#include <array>

using namespace yq_utils::allocator_block;

void test_linked_list_storage_create()
{
	{
		linked_list_storage bm;
		assert(bm.empty());
		static_assert(bm.least_size() == 8);
	}
}

void test_linked_list_storage_push_pop_base()
{
	linked_list_storage bm;
	uint8_t buf[bm.least_size()];
	bm.push(buf);
	assert(bm.pop() == buf);
	assert(bm.empty());
}

void test_linked_list_storage_push_pop(std::size_t scale,
									   std::size_t block_size)
{
	linked_list_storage bm;
	std::vector<void*> mems;
	for (std::size_t i = 0; i < scale; ++i)
	{
		void* buf = ::operator new(block_size);
		mems.push_back(buf);
		bm.push(buf);
	}

	for (std::size_t i = 0; i < scale; ++i)
	{
		void* p = bm.pop();
		assert(p == mems[i]);
		::operator delete(p);
	}
	assert(bm.empty());
}

void test_ref_cnt_chunk_alloc(std::size_t malloc_size, std::size_t block_size)
{
	ref_cnt_chunk_manager manager;
	auto vec = manager.malloc(malloc_size, block_size);
	assert(vec.size() == malloc_size / block_size);

	for (void* p : vec)
		manager.logout_block(p, block_size);
}

void test_ref_cnt_chunk_one_side()
{
	for (std::size_t base = 8; base <= 512; base *= 2)
	{
		for (std::size_t malloc_size = base; malloc_size <= base * 512; malloc_size *= 2)
		{
			test_ref_cnt_chunk_alloc(malloc_size, base);
		}
	}
}

void test_ref_cnt_chunk_both_sides()
{
	ref_cnt_chunk_manager manager;
	std::vector<std::vector<std::pair<std::vector<void*>, std::size_t>>> vec3d;
	for (std::size_t base = 8; base <= 512; base *= 2)
	{
		std::vector<std::pair<std::vector<void*>, std::size_t>> vecs;
		for (std::size_t malloc_size = base; malloc_size <= base * 512; malloc_size *= 2)
		{
			auto void_vec = manager.malloc(malloc_size, base);
			assert(void_vec.size() == malloc_size / base);
			vecs.emplace_back(void_vec, base);
		}
		vec3d.push_back(vecs);
	}

	for (const auto& vecs : vec3d)
	{
		for (const auto& [vec, base] : vecs)
		{
			for (std::size_t i = 0; i < vec.size()-1; ++i)
			{
				assert(!manager.logout_block(vec[i], base));
			}
			assert(manager.logout_block(vec.back(), base));
		}
	}
	assert(manager.empty());
}

auto main() -> int
{
	test_linked_list_storage_create();
	test_linked_list_storage_push_pop_base();
	for (std::size_t i = 2; i < (1 << 10); i *= 2)
	{
		for (std::size_t block_size = 8; block_size <= 512; block_size *= 2)
			test_linked_list_storage_push_pop(i, block_size);
	}

	test_ref_cnt_chunk_alloc(16, 16);
	test_ref_cnt_chunk_one_side();
	test_ref_cnt_chunk_both_sides();
}
