#pragma once
#include <cstddef>
#include <cassert>
#include <new>
#include <bit>
#include <cstdint>
#include <vector>
#include <set>
#include <limits>

namespace yq_utils
{

namespace allocator_block
{

template<typename Derived>
struct block_manager
{
	constexpr block_manager() noexcept
	{}

	constexpr auto empty() const noexcept -> bool
	{
		return static_cast<const Derived*>(this)->empty_impl();
	}

	constexpr void push(void* mem) noexcept
	{
		static_cast<Derived*>(this)->push_impl(mem);
	}

	constexpr auto pop() noexcept -> void* 
	{
		return static_cast<Derived*>(this)->pop_impl();
	}

	constexpr static
	auto least_size() noexcept -> std::size_t
	{
		return Derived::least_size_impl();
	}
};


class linked_list_storage
	: public block_manager<linked_list_storage>
{
	friend block_manager;
	struct block_list
	{
		block_list* next;
	};

public:
	constexpr linked_list_storage() noexcept:
		block_manager<linked_list_storage>{},
		m_dummy_head { .next = nullptr },
		m_tail { &m_dummy_head }
	{}

	linked_list_storage(const linked_list_storage& rhs) = delete;
		
private:
	constexpr auto empty_impl() const noexcept -> bool
	{
		return m_dummy_head.next == nullptr;
	}

	constexpr void push_impl(void* mem) noexcept
	{
		block_list* new_value =
			new (mem) block_list{.next = nullptr};

		m_tail->next = new_value;
		m_tail = new_value;
	}

	constexpr auto pop_impl() noexcept -> void*
	{
		auto* result = m_dummy_head.next;
		assert(result != nullptr);
		m_dummy_head.next = result->next;
		if (m_dummy_head.next == nullptr)
			m_tail = &m_dummy_head;
		return result;
	}

	constexpr static
	auto least_size_impl() noexcept -> std::size_t
	{
		return std::bit_ceil(sizeof(block_list));
	}

private:
	block_list m_dummy_head;
	block_list* m_tail;
};


template<typename Derived>
struct chunk_manager
{
	/**
	 * @return 根据block_size分隔开来的块
	 */
	[[nodiscard]]
	constexpr auto malloc(std::size_t malloc_size,
						  std::size_t block_size) noexcept -> std::vector<void*>
	{
		return static_cast<Derived*>(this)->malloc_impl(malloc_size, block_size);
	}

	/**
	 * @return 如果触发删除操作，返回 true, 否则返回 false
	 */
	constexpr
	auto logout_block(void* p_block, std::size_t block_size) noexcept -> bool
	{
		return static_cast<Derived*>(this)->logout_block_impl(p_block, block_size);
	}
};

class ref_cnt_chunk_manager:
	public chunk_manager<ref_cnt_chunk_manager>
{
	friend chunk_manager<ref_cnt_chunk_manager>;
public:
	struct chunk
	{
		void* memory;
		std::size_t block_size;
		// 防止lower_bound查找返回const_iterator, 无法进行修改
		mutable std::size_t used_cnt;
		std::size_t block_cnt;
	};

	struct chunk_cmp
	{
		// 提示编译器接受 void* 参加lower_bound重载决议，避免构造
		// chunk再进行比较的开销
		using is_transparent = void;
		auto operator()(const chunk& lhs, const chunk& rhs) const -> bool
		{
			return lhs.memory > rhs.memory;
		}

		auto operator()(const chunk& ck, const void* p) const -> bool
		{
			return ck.memory > p;
		}

		auto operator()(const void* p, const chunk& ck) const -> bool
		{
			return p > ck.memory;
		}
	};

	constexpr static
	auto block_size2num_digits(std::size_t block_size) -> std::size_t
	{
		assert(std::has_single_bit(block_size));
		return std::numeric_limits<std::size_t>::digits - 1 -
			   std::countl_zero(block_size);
	}
	
	constexpr static
	auto num_digits2block_size(std::size_t digits) noexcept -> std::size_t
	{
		assert(digits <= 31);
		return (1 << digits);
	}

	~ref_cnt_chunk_manager()
	{
		for (auto& set : m_chunks)
		{
			for (auto& chunk : set)
			{
				::operator delete(chunk.memory);
			}
		}
	}

	/**
	 * @note 一般只提供给单元测试使用，非常数时间开销
	 */
	auto empty() const noexcept -> bool
	{
		for (const auto& set : m_chunks)
		{
			if (!set.empty())
				return false;
		}
		return true;
	}

private:
	constexpr
	auto malloc_impl(std::size_t malloc_size, std::size_t block_size)
		-> std::vector<void*>
	{
		assert(std::has_single_bit(malloc_size));
		assert(std::has_single_bit(block_size));
		// MALLOC_SIZE >= BLOCK_SIZE 
		// 并且MALLOC_SIZE是BLOCK_SIZE的整数倍，所以不会越界或出现空余
		assert(malloc_size >= block_size);
		
		// 异常抛出后分配器状态不变
		uint8_t* new_mem = reinterpret_cast<uint8_t*>(::operator new(malloc_size));
		std::vector<void*> result;
		result.reserve(malloc_size / block_size);
		
		uint8_t* end_mem = new_mem + malloc_size;
		for (uint8_t* cur = new_mem; cur != end_mem; cur += block_size)
		{
			
			result.push_back(cur);
		}

		std::size_t chunk_idx = block_size2num_digits(block_size);
		if (m_chunks.size() <= chunk_idx)
				m_chunks.resize(chunk_idx+1);

		m_chunks[chunk_idx].emplace(reinterpret_cast<void*>(new_mem),
									block_size, 0, result.size());

		return result;
	}

	constexpr auto logout_block_impl(void* p_block,
									 std::size_t block_size) noexcept -> bool
	{
		const std::size_t chunk_idx = block_size2num_digits(block_size);
		// 查找 <= p_block的最近的分配内存对应的chunk,

		auto itr = m_chunks[chunk_idx].lower_bound(p_block);
		assert(itr != m_chunks[chunk_idx].end() &&
			   "logout_block cannot find chunk");

		++itr->used_cnt;
		assert(itr->used_cnt <= itr->block_cnt);
		if (itr->used_cnt == itr->block_cnt)
		{
			void* mem = itr->memory;
			m_chunks[chunk_idx].erase(itr);
			::operator delete(mem);
			return true;
		}
		return false;
	}

	// 对chunk按照block_size进行分区，按照 log(block_size)进行区分
	std::vector<std::set<chunk, chunk_cmp>> m_chunks;
};


} 	//namespace allocator_block

}	//namespace yq_utils
