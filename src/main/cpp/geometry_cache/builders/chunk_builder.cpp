/*!
 * \brief Provides definitions for all the functions needed to build geometry for a chunk
 *
 * \author ddubois 
 * \date 02-Mar-17.
 */

#include "chunk_builder.h"
#include "../../utils/utils.h"
#include "../../utils/io.h"
#include "../../render/nova_renderer.h"

#include <iostream>
#include <easylogging++.h>
#include <string.h>

namespace nova {
    std::vector<glm::ivec3> chunk_builder::get_blocks_that_match_filter(const mc_chunk &chunk, const std::shared_ptr<igeometry_filter> filter) {
        auto blocks_that_match_filter = std::vector<glm::ivec3>{};
        for(int z = 0; z < CHUNK_WIDTH; z++) {
            for(int y = 0; y < CHUNK_HEIGHT; y++) {
                for(int x = 0; x < CHUNK_DEPTH; x++) {
                    int i = x + y * CHUNK_WIDTH + z * CHUNK_WIDTH * CHUNK_HEIGHT;
                    auto cur_block = chunk.blocks[i];
                    auto cur_block_definition = block_definitions[cur_block.id];
                    if(filter->matches(cur_block_definition)) {
                        auto pos = glm::ivec3(x, y, z);
                        blocks_that_match_filter.push_back(pos);
                    }
                }
            }
        }
        return blocks_that_match_filter;
    }

    mesh_definition chunk_builder::make_mesh_for_blocks(const std::vector<glm::ivec3>& blocks, const mc_chunk& chunk) {
        auto mesh = mesh_definition{};
        mesh.vertex_format = format::POS_UV_LIGHTMAPUV_NORMAL_TANGENT;

		auto vertices = std::vector<float>{};
		auto indices = std::vector<unsigned int>{};
		auto cur_index = 0;

        for(const auto& block_pos : blocks) {
            auto block_offset = glm::vec3{block_pos};
            auto block_idx = pos_to_idx(block_pos);
            auto block = block_definitions[chunk.blocks[block_idx].id];

			// Get the geometry for the block
			std::vector<block_face> faces_for_block;
			if(!!block.is_cube) {
				faces_for_block = make_geometry_for_block(block_pos, chunk, block.texture_name);
			} else {
				// Use the block model registry
				LOG(WARNING) << "Block models not implemented. Fix it.";
			}

			// Put the geometry into our buffer
			for(auto& face : faces_for_block) {
				for(int vert_idx = 0; vert_idx < 4; vert_idx ++) {
                    face.vertices[vert_idx].position += block_offset;
					vertices.insert(vertices.end(), &face.vertices[vert_idx].position.x, &face.vertices[vert_idx].position.x + (sizeof(block_vertex) / sizeof(float)));
				}
				indices.push_back(0 + cur_index);
				indices.push_back(1 + cur_index);
				indices.push_back(2 + cur_index);
				indices.push_back(1 + cur_index);
				indices.push_back(2 + cur_index);
				indices.push_back(3 + cur_index);

				cur_index += 4;
			}
        }

		mesh.vertex_data = vertices;
		mesh.indices = indices;
        mesh.position = {chunk.x, 0, chunk.z};

		return mesh;
    }

	std::vector<block_face> chunk_builder::make_geometry_for_block(const glm::ivec3& block_pos, const mc_chunk& chunk, const char * texture_name) {
        auto faces_to_make = std::vector<face_id>{};
        if(!block_at_pos_is_opaque(block_pos + glm::ivec3(0, 1, 0), chunk) &&
           !block_at_offset_is_same(block_pos, glm::ivec3(0, 1, 0), chunk)) {
            faces_to_make.push_back(face_id::TOP);
        }
        if(!block_at_pos_is_opaque(block_pos + glm::ivec3(0, -1, 0), chunk) &&
           !block_at_offset_is_same(block_pos, glm::ivec3(0, -1, 0), chunk)) {
            faces_to_make.push_back(face_id::BOTTOM);
        }
        if(!block_at_pos_is_opaque(block_pos + glm::ivec3(1, 0, 0), chunk) &&
           !block_at_offset_is_same(block_pos, glm::ivec3(1, 0, 0), chunk)) {
            faces_to_make.push_back(face_id::RIGHT);
        }
        if(!block_at_pos_is_opaque(block_pos + glm::ivec3(-1, 0, 0), chunk) &&
           !block_at_offset_is_same(block_pos, glm::ivec3(-1, 0, 0), chunk)) {
            faces_to_make.push_back(face_id::LEFT);
        }
        if(!block_at_pos_is_opaque(block_pos + glm::ivec3(0, 0, 1), chunk) &&
           !block_at_offset_is_same(block_pos, glm::ivec3(0, 0, 1), chunk)) {
            faces_to_make.push_back(face_id::FRONT);
        }
        if(!block_at_pos_is_opaque(block_pos + glm::ivec3(0, 0, -1), chunk) &&
           !block_at_offset_is_same(block_pos, glm::ivec3(0, 0, -1), chunk)) {
            faces_to_make.push_back(face_id::BACK);
        }

        const auto &tex_location = nova_renderer::instance->get_texture_manager().get_texture_location(std::string(texture_name));

        auto quads = std::vector<block_face>{};
        for(auto &face : faces_to_make) {
            auto ao = get_ao_in_direction(block_pos, face, chunk);
            quads.push_back(make_quad(face, 1, tex_location));
        }

        return quads;
    }

    bool chunk_builder::block_at_pos_is_opaque(glm::ivec3 block_pos, const mc_chunk& chunk) {
        // A separate check for each direction to increase code readability and debuggability
        if(block_pos.x < 0 || block_pos.x >= CHUNK_WIDTH) {
            return false;
        }

        if(block_pos.y < 0 || block_pos.y >= CHUNK_HEIGHT) {
            return false;
        }

        if(block_pos.z < 0 || block_pos.z >= CHUNK_DEPTH) {
            return false;
        }

        auto block_idx = pos_to_idx(block_pos);
        auto block = chunk.blocks[block_idx];

        return !block_definitions[block.id].is_transparent();
    }

    bool chunk_builder::block_at_offset_is_same(glm::ivec3 block_pos, glm::ivec3 offset, const mc_chunk& chunk) {
        // A separate check for each direction to increase code readability and debuggability
        if(block_pos.x+offset.x < 0 || block_pos.x+offset.x >= CHUNK_WIDTH) {
            return false;
        }

        if(block_pos.y+offset.y < 0 || block_pos.y+offset.y >= CHUNK_HEIGHT) {
            return false;
        }

        if(block_pos.z+offset.z < 0 || block_pos.z+offset.z >= CHUNK_DEPTH) {
            return false;
        }

        auto block_idx = pos_to_idx(block_pos);
        auto block = block_definitions[chunk.blocks[block_idx].id];
        auto block_idx2 = pos_to_idx(block_pos+offset);
        auto block2 = block_definitions[chunk.blocks[block_idx2].id];

        return strcmp(block.name, block2.name) == 0;
    }

    int chunk_builder::pos_to_idx(const glm::ivec3& pos) {
        return pos.x + pos.y * CHUNK_WIDTH + pos.z * CHUNK_WIDTH * CHUNK_HEIGHT;
    }

    block_face chunk_builder::make_quad(const face_id which_face, const float size, const texture_manager::texture_location& tex_location) {
        const auto tex_extents = tex_location.max - tex_location.min;
        glm::vec3 positions[4];
        glm::vec2 uvs[4];
        glm::vec3 normal;
        glm::vec3 tangent;
        if(which_face == face_id::LEFT) {
            // x = 0
            positions[0]    = {0, 0, 0};
            uvs[0]          = tex_location.min;
            positions[1]    = {0, 0, size};
            uvs[1]          = tex_location.min + glm::vec2{0, tex_extents.y};
            positions[2]    = {0, size, 0};
            uvs[2]          = tex_location.min + glm::vec2{tex_extents.x, 0};
            positions[3]    = {0, size, size};
            uvs[3]          = tex_location.max;

            normal = glm::vec3{-1, 0, 0};
            tangent = glm::vec3{0, 0, 1};

        } else if(which_face == face_id::RIGHT) {
            // x = 0
            positions[0]    = {size, 0, 0};
            uvs[0]          = tex_location.min;
            positions[1]    = {size, 0, size};
            uvs[1]          = tex_location.min + glm::vec2{0, tex_extents.y};
            positions[2]    = {size, size, 0};
            uvs[2]          = tex_location.min + glm::vec2{tex_extents.x, 0};
            positions[3]    = {size, size, size};
            uvs[3]          = tex_location.max;

            normal = glm::vec3{1, 0, 0};
            tangent = glm::vec3{0, 0, -1};

        } else if(which_face == face_id::BOTTOM) {
            // y = 0
            positions[0]    = {0, 0, 0};
            uvs[0]          = tex_location.min;
            positions[1]    = {0, 0, size};
            uvs[1]          = tex_location.min + glm::vec2{0, tex_extents.y};
            positions[2]    = {size, 0, 0};
            uvs[2]          = tex_location.min + glm::vec2{tex_extents.x, 0};
            positions[3]    = {size, 0, size};
            uvs[3]          = tex_location.max;

            normal = glm::vec3{0, -1, 0};
            tangent = glm::vec3{-1, 0, 0};

        } else if(which_face == face_id::TOP) {
            // y = 0
            positions[0]    = {0, size, 0};
            uvs[0]          = tex_location.min;
            positions[1]    = {0, size, size};
            uvs[1]          = tex_location.min + glm::vec2{0, tex_extents.y};
            positions[2]    = {size, size, 0};
            uvs[2]          = tex_location.min + glm::vec2{tex_extents.x, 0};
            positions[3]    = {size, size, size};
            uvs[3]          = tex_location.max;

            normal = glm::vec3{0, 1, 0};
            tangent = glm::vec3{1, 0, 0};

        } else if(which_face == face_id::BACK) {
            // z = 0
            positions[0]    = {0, 0, 0};
            uvs[0]          = tex_location.min;
            positions[1]    = {0, size, 0};
            uvs[1]          = tex_location.min + glm::vec2{0, tex_extents.y};
            positions[2]    = {size, 0, 0};
            uvs[2]          = tex_location.min + glm::vec2{tex_extents.x, 0};
            positions[3]    = {size, size, 0};
            uvs[3]          = tex_location.max;

            normal = glm::vec3{0, 0, -1};
            tangent = glm::vec3{-1, 0, 0};

        } else if(which_face == face_id::FRONT) {
            // z = 0
            positions[0]    = {0, 0, size};
            uvs[0]          = tex_location.min;
            positions[1]    = {0, size, size};
            uvs[1]          = tex_location.min + glm::vec2{0, tex_extents.y};
            positions[2]    = {size, 0, size};
            uvs[2]          = tex_location.min + glm::vec2{tex_extents.x, 0};
            positions[3]    = {size, size, size};
            uvs[3]          = tex_location.max;

            normal = glm::vec3{0, 0, 1};
            tangent = glm::vec3{1, 0, 0};
        }

        auto face = block_face{};
        for(int i = 0; i < 4; i++) {
            face.vertices[i].position   = positions[i];
            face.vertices[i].uv         = uvs[i];
            face.vertices[i].normal     = normal;
            face.vertices[i].tangent    = tangent;
        }

        return face;
    }

	bool chunk_builder::is_cube(const glm::ivec3 pos, const mc_chunk& chunk) {
		return true;
	}

	float chunk_builder::get_ao_in_direction(const glm::vec3 position, const face_id face_to_check, const mc_chunk& chunk) {
		return 0;
	}

    std::unordered_map<int, mc_block_definition>& chunk_builder::get_block_definitions() {
        return block_definitions;
    };

    el::base::Writer &operator<<(el::base::Writer &out, const block_vertex& vert) {
        out << "block_vertex { position=" << vert.position << ", uv=" << vert.uv << ", lightmap_uv="
            << vert.lightmap_uv << ", normal=" << vert.normal << ", tangent=" << vert.tangent << "}";

        return out;
    }
}
