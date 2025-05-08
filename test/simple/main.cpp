#include "memory_pool/memory_pool.hpp"
#include <vector>
#include <print>
#include <random>

auto main() -> int
{
	std::mt19937 gen { std::random_device{}()};
	{
		std::vector<int, yq_utils::default_allocator<int>> vec;
		std::uniform_int_distribution<std::size_t> dist {1000, 2000};
		std::size_t N = dist(gen);
		for (std::size_t i = 0; i < N; ++i)
			vec.push_back(i);
		std::println("{} {}", N, vec.size());
	}
	{
		std::vector<int, yq_utils::seg_list_allocator<int>> vec;
		//std::uniform_int_distribution<std::size_t> dist {};
		std::size_t N = 100'000;
		for (std::size_t i = 0; i < N; ++i)
			vec.push_back(i);
		std::println("{} {}", N, vec.size());
		
	}
}
