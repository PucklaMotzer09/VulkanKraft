#include "world_generation.hpp"

namespace world_gen {
WorldGeneration::WorldGeneration(const size_t seed_value) {
  if (seed_value != 0) {
    m_noise.seed(seed_value);
  }
}

void WorldGeneration::seed(const size_t seed_value) {
  m_noise.seed(seed_value);
}

void WorldGeneration::generate(const glm::ivec2 &chunk_pos,
                               chunk::BlockArray &block_array) const {
  for (size_t x = 0; x < chunk::block_width; x++) {
    for (size_t z = 0; z < chunk::block_depth; z++) {
      const glm::vec2 block_pos(
          static_cast<float>(chunk_pos.x) + static_cast<float>(x),
          static_cast<float>(chunk_pos.y) + static_cast<float>(z));

      auto noise_value{m_noise.get(block_pos.x, block_pos.y)};
      noise_value += 1.0f;
      noise_value /= 2.0f;
      noise_value *= static_cast<float>(chunk::block_height);

      const auto max_height{static_cast<size_t>(noise_value)};
      if (max_height != 0) {
        for (size_t y = 0; y < max_height - 1; y++) {
          block_array.set(x, y, z, block::Type::DIRT);
        }
        block_array.set(x, max_height - 1, z, block::Type::GRASS);
      }
      for (size_t y = max_height; y < chunk::block_height; y++) {
        block_array.set(x, y, z, block::Type::AIR);
      }
    }
  }
}
} // namespace world_gen
