# seg_list_allocator

The `seg_list_allocator` is a custom memory allocator designed to efficiently manage memory using segmented free lists in C++20.
It is built to handle allocations of different sizes, aligning memory blocks to the smallest power of two that can hold the requested size.
Using block counter to free memory before destructor, the memory usage is always in a reasonable range.

--- 

# Usage
### Specify container template allocator argument 
``` cpp
#include <vector>
#include "memory_pool/memory_pool.hpp"

auto main() -> int
{
    std::vector<int, yq_utils::seg_list_allocator> vec;
    for (std::size_t i = 0; i < 100'000; ++i)
        vec.push_back(i);
}
```

---

# ISSUES
- Upper boundary has not been tested.
- Alternating allocate of different `n` (block size) has not been tested.
- Performance analysis is lacking.
- Constructs, copy/move operator may cause unexpected behavior.
