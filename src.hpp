
// Copyright (c) 2024 ACM Class, SJTU

namespace sjtu {

class BuddyAllocator {
public:
  /**
   * @brief Construct a new Buddy Allocator object with the given RAM size and
   * minimum block size.
   *
   * @param ram_size Size of the RAM. The address space is 0 ~ ram_size - 1.
   * @param min_block_size Minimum size of a block. The block size is 2^k where
   * k >= min_block_size.
   */
  BuddyAllocator(int ram_size, int min_block_size) {
    this->ram_size = ram_size;
    this->min_block_size = min_block_size;
    
    // Calculate number of layers needed
    max_layer = 0;
    int current_size = min_block_size;
    while (current_size < ram_size) {
      current_size *= 2;
      max_layer++;
    }
    
    // Initialize free lists for each layer
    for (int i = 0; i <= max_layer; i++) {
      free_lists[i] = nullptr;
    }
    
    // Initially, the entire RAM is one free block at the highest layer
    free_lists[max_layer] = new Block(0, ram_size);
  }

  /**
   * @brief Allocate a block with the given size at the minimum available
   * address.
   *
   * @param size The size of the block.
   * @return int The address of the block. Return -1 if the block cannot be
   * allocated.
   */
  int malloc(int size) {
    // Find the appropriate layer for this size
    int layer = size_to_layer(size);
    if (layer == -1) return -1;
    
    // Find the block with minimum address that can be split to this size
    int min_addr = -1;
    Block* best_block = nullptr;
    int best_layer = -1;
    
    // Check all layers from target to max
    for (int current_layer = layer; current_layer <= max_layer; current_layer++) {
      Block* current = free_lists[current_layer];
      while (current) {
        if (min_addr == -1 || current->addr < min_addr) {
          min_addr = current->addr;
          best_block = current;
          best_layer = current_layer;
        }
        current = current->next;
      }
    }
    
    if (!best_block) return -1;
    
    // Remove the best block from its free list
    remove_from_free_list(best_block, best_layer);
    
    // If it's at a higher layer, split it down
    if (best_layer > layer) {
      Block* target = split_to_target(best_block, best_layer, layer, best_block->addr);
      return target->addr;
    }
    
    return best_block->addr;
  }

  /**
   * @brief Allocate a block with the given size at the given address.
   *
   * @param addr The address of the block.
   * @param size The size of the block.
   * @return int The address of the block. Return -1 if the block cannot be
   * allocated.
   */
  int malloc_at(int addr, int size) {
    // Find the appropriate layer for this size
    int layer = size_to_layer(size);
    if (layer == -1) return -1;
    
    // Check if this exact block is free
    Block* block = find_block_at_addr(addr, size, layer);
    if (block) {
      // Remove from free list
      remove_from_free_list(block, layer);
      return addr;
    }
    
    // Need to split larger blocks to get the exact address
    // Try to find and split from higher layers
    for (int current_layer = layer + 1; current_layer <= max_layer; current_layer++) {
      Block* larger = find_block_containing_addr(addr, size, current_layer);
      if (larger) {
        // Remove the larger block from free list
        remove_from_free_list(larger, current_layer);
        
        // Split it down to the target layer
        Block* target = split_to_target(larger, current_layer, layer, addr);
        if (target) {
          return target->addr;
        } else {
          // Failed to split, put the larger block back
          add_to_free_list(larger, current_layer);
          return -1;
        }
      }
    }
    
    return -1;
  }

  /**
   * @brief Deallocate a block with the given size at the given address.
   *
   * @param addr The address of the block. It is ensured that the block is
   * allocated before.
   * @param size The size of the block.
   */
  void free_at(int addr, int size) {
    int layer = size_to_layer(size);
    if (layer == -1) return;
    
    // Create a new free block
    Block* block = new Block(addr, size);
    
    // Try to merge with buddy blocks
    while (layer < max_layer) {
      Block* buddy = find_buddy(block, layer);
      if (!buddy) break;
      
      // Remove buddy from free list
      remove_from_free_list(buddy, layer);
      
      // Create merged block
      int merged_addr = (block->addr < buddy->addr) ? block->addr : buddy->addr;
      int merged_size = block->size * 2;
      
      delete block;
      delete buddy;
      
      block = new Block(merged_addr, merged_size);
      layer++;
    }
    
    // Add to free list at final layer
    add_to_free_list(block, layer);
  }

private:
  struct Block {
    int addr;
    int size;
    Block* next;
    
    Block(int a, int s) : addr(a), size(s), next(nullptr) {}
  };
  
  int ram_size;
  int min_block_size;
  int max_layer;
  Block* free_lists[32];  // Support up to 2^32 * min_block_size
  
  // Convert size to layer number
  int size_to_layer(int size) {
    if (size < min_block_size || (size % min_block_size) != 0) return -1;
    
    int layer = 0;
    int current_size = min_block_size;
    while (current_size < size) {
      current_size *= 2;
      layer++;
    }
    
    if (current_size != size) return -1;
    return layer;
  }
  
  // Find a free block at the given layer, splitting larger blocks if needed
  Block* find_free_block(int layer) {
    // If we have a free block at this layer, return the first one
    if (free_lists[layer]) {
      Block* block = free_lists[layer];
      free_lists[layer] = block->next;
      return block;
    }
    
    // Try to split a larger block
    if (layer < max_layer) {
      Block* larger = find_free_block(layer + 1);
      if (larger) {
        // Split the larger block into two
        int half_size = larger->size / 2;
        Block* left = new Block(larger->addr, half_size);
        Block* right = new Block(larger->addr + half_size, half_size);
        
        delete larger;
        
        // Add right half to free list, return left half
        add_to_free_list(right, layer);
        return left;
      }
    }
    
    return nullptr;
  }
  
  // Find a specific block at given address and size in free list
  Block* find_block_at_addr(int addr, int size, int layer) {
    Block* current = free_lists[layer];
    while (current) {
      if (current->addr == addr && current->size == size) {
        return current;
      }
      current = current->next;
    }
    return nullptr;
  }
  
  // Find a block that contains the given address range
  Block* find_block_containing_addr(int addr, int size, int layer) {
    Block* current = free_lists[layer];
    while (current) {
      if (current->addr <= addr && (current->addr + current->size) >= (addr + size)) {
        return current;
      }
      current = current->next;
    }
    return nullptr;
  }
  
  // Split a block down to target layer, returning the block at target address
  Block* split_to_target(Block* block, int from_layer, int to_layer, int target_addr) {
    Block* current = block;
    
    for (int layer = from_layer; layer > to_layer; layer--) {
      int half_size = current->size / 2;
      Block* left = new Block(current->addr, half_size);
      Block* right = new Block(current->addr + half_size, half_size);
      
      // Determine which half contains our target address
      Block* target_half = nullptr;
      Block* other_half = nullptr;
      
      if (left->addr <= target_addr && (left->addr + left->size) > target_addr) {
        target_half = left;
        other_half = right;
      } else {
        target_half = right;
        other_half = left;
      }
      
      // Add the other half to the free list at this layer
      add_to_free_list(other_half, layer - 1);
      
      // Delete current block and continue with target half
      delete current;
      current = target_half;
    }
    
    return current;
  }
  
  // Find buddy block for merging
  Block* find_buddy(Block* block, int layer) {
    int buddy_addr = block->addr ^ block->size;  // XOR with size to get buddy address
    return find_block_at_addr(buddy_addr, block->size, layer);
  }
  
  // Add block to free list (maintains sorted order by address)
  void add_to_free_list(Block* block, int layer) {
    if (!free_lists[layer] || block->addr < free_lists[layer]->addr) {
      block->next = free_lists[layer];
      free_lists[layer] = block;
      return;
    }
    
    Block* current = free_lists[layer];
    while (current->next && current->next->addr < block->addr) {
      current = current->next;
    }
    
    block->next = current->next;
    current->next = block;
  }
  
  // Remove block from free list
  void remove_from_free_list(Block* block, int layer) {
    if (!free_lists[layer]) return;
    
    if (free_lists[layer] == block) {
      free_lists[layer] = block->next;
      return;
    }
    
    Block* current = free_lists[layer];
    while (current->next && current->next != block) {
      current = current->next;
    }
    
    if (current->next == block) {
      current->next = block->next;
    }
  }
};

} // namespace sjtu

