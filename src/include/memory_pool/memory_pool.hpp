#pragma once
#include <bit>
#include <type_traits>
#include <concepts>
#include <limits>
#include <stdexcept>
#include <array>
#include <cstdint>
#include <algorithm>
#include <cassert>
#include "block_storage.hpp"

namespace yq_utils
{

template<typename Ty, typename Derived>
class allocator_named_req
{
public:
	using value_type = Ty;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using propagate_on_container_move_assignment = std::true_type;

	// Constructor, Destructor
	constexpr allocator_named_req() noexcept = default;
	constexpr ~allocator_named_req() = default;
	constexpr allocator_named_req(const allocator_named_req& rhs) = default;
	template <typename RhsTy, typename RhsDrived>
	constexpr allocator_named_req(
		const allocator_named_req<RhsTy, RhsDrived>&){};

	// 子类可以覆盖
	auto operator=(const allocator_named_req& rhs)
		-> allocator_named_req& = default;

	[[nodiscard]]
	constexpr auto allocate(std::size_t n) -> value_type*
	{
		return static_cast<Derived*>(this)->allocate_impl(n);
	}

	[[nodiscard]]
	constexpr auto allocate_at_least(std::size_t n) -> value_type*
	{
		return static_cast<Derived*>(this)->allocate_at_least_impl(n);
	}
	
	constexpr void deallocate(value_type* p, std::size_t n)
	{
		return static_cast<Derived*>(this)->deallocate_impl(p, n);
	}

	template <std::integral int_type>
	constexpr static
	auto safe_multiply(int_type left, int_type right) -> bool
	{
		if (left > std::numeric_limits<int_type>::max() / right)
			return false;
		return true;
	}
};

template <typename Ty>
class default_allocator : public allocator_named_req<Ty, default_allocator<Ty>>
{
	using base_req = allocator_named_req<Ty, default_allocator<Ty>>;
	friend base_req;
public:
	using typename base_req::value_type;
	using typename base_req::size_type;
	using typename base_req::difference_type;
	using typename base_req::propagate_on_container_move_assignment;

	using base_req::base_req;
private:
	constexpr auto allocate_impl(std::size_t n) -> value_type*
	{
		if (!base_req::safe_multiply(n, sizeof(value_type)))
			throw std::bad_array_new_length();

		return reinterpret_cast<value_type*>(
			::operator new(n * sizeof(value_type)));
	}

	constexpr auto allocate_at_least_impl(std::size_t n) -> value_type*
	{
		return allocate_impl(n);
	}

	constexpr void deallocate_impl(value_type* p, [[maybe_unused]] std::size_t n)
	{
		::operator delete(p);
	}
};

template <typename Ty>
class seg_list_allocator
	: public allocator_named_req<Ty, seg_list_allocator<Ty>>
{
	using base_req =
		allocator_named_req<Ty, seg_list_allocator<Ty>>;
	friend base_req;
	// block只具体的小块
	using block_manager = allocator_block::linked_list_storage;
	using chunk_manager = allocator_block::ref_cnt_chunk_manager;

	inline static constexpr std::size_t BASE_CLASSES = 32; // 0 ~ 31
	inline static constexpr std::size_t MIN_BLOCK_SIZE =
		std::max<std::size_t>(block_manager::least_size(), std::bit_ceil(sizeof(Ty)));
	inline static constexpr std::size_t MALLOC_SIZE_FALC = 4096;

public:
	using typename base_req::value_type;
	using typename base_req::size_type;
	using typename base_req::difference_type;
	using typename base_req::propagate_on_container_move_assignment;

	constexpr seg_list_allocator() noexcept
	{
	}

	constexpr ~seg_list_allocator()
	{
	}

	constexpr seg_list_allocator(const seg_list_allocator& )
	{
	}

	constexpr
	auto operator=(const seg_list_allocator&) -> seg_list_allocator&
	{
		return *this;
	}

	template<typename RhsTy>
	constexpr seg_list_allocator(const seg_list_allocator<RhsTy>& )
	{
	}

private:
	constexpr auto allocate_impl(std::size_t n) -> value_type*
	{
		auto ceil_block_size = block_size_ceil(allocate_size2ceil_block_size(n));
		auto idx = ceil_block_size2array_idx(ceil_block_size);

		return reinterpret_cast<value_type*>(block_allocate(
			m_seg_list[idx], std::max(ceil_block_size, MALLOC_SIZE_FALC), ceil_block_size));
	}

	constexpr auto allocate_at_least_impl(std::size_t n) -> value_type*
	{
		return allocate_impl(n);
	}

	constexpr void deallocate_impl(value_type* p, std::size_t n)
	{
		auto ceil_block_size =
			block_size_ceil(allocate_size2ceil_block_size(n));

		block_dealloc(reinterpret_cast<void*>(p),
					  ceil_block_size);
	}

private:
	constexpr static
	auto allocate_size2ceil_block_size(std::size_t n) -> std::size_t
	{
		if (!base_req::safe_multiply(n, sizeof(value_type)))
			throw std::bad_array_new_length();
		return block_size_ceil(n * sizeof(value_type));
	}

	constexpr static
	auto block_size_ceil(std::size_t size) -> std::size_t
	{
		return std::max(std::bit_ceil(size), MIN_BLOCK_SIZE);
	}

	constexpr static
	auto ceil_block_size2array_idx(std::size_t block_size) -> std::size_t
	{
		// 暂时不做偏移运算, 让array的低位留空
		return chunk_manager::block_size2num_digits(block_size);
	}

	constexpr auto block_allocate(block_manager& manager,
										 std::size_t malloc_size,
										 std::size_t block_size) -> void*
	{
		if (manager.empty())
		{
			auto new_block_list = m_chunk_manager.malloc(malloc_size, block_size);
			for (auto* p : new_block_list)
				manager.push(p);
		}

		return manager.pop();
	}

	constexpr void block_dealloc(void* p, std::size_t block_size)
	{
		m_chunk_manager.logout_block(p, block_size);
	}
	
private:
	// 分段空闲链表
	std::array<block_manager, BASE_CLASSES> m_seg_list;
	// 分配块管理，计数释放内存
	chunk_manager m_chunk_manager;
};

} // namespace yq_utils
