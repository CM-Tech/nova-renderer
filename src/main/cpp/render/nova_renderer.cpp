//
// Created by David on 25-Dec-15.
//

#include "nova_renderer.h"
#include "../utils/utils.h"
#include "../data_loading/loaders/loaders.h"
#include "../utils/profiler.h"
#include "render_graph.h"
#include "objects/opengl/enum_translation.h"

#include <easylogging++.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stack>

INITIALIZE_EASYLOGGINGPP

namespace nova {
    std::unique_ptr<nova_renderer> nova_renderer::instance;

    nova_renderer::nova_renderer() {
        game_window = std::make_unique<glfw_gl_window>();
        enable_debug();
        ubo_manager = std::make_unique<uniform_buffer_store>();
        textures = std::make_unique<texture_manager>();
        meshes = std::make_unique<mesh_store>();
        inputs = std::make_unique<input_handler>();
		render_settings->register_change_listener(ubo_manager.get());
		render_settings->register_change_listener(game_window.get());
        render_settings->register_change_listener(this);

        render_settings->update_config_loaded();
		render_settings->update_config_changed();

        LOG(INFO) << "Finished sending out initial config";

        init_opengl_state();
    }

    void nova_renderer::init_opengl_state() const {
        LOG(DEBUG) << "Initting OpenGL state";

        glClearColor(135 / 255.0f, 206 / 255.0f, 235 / 255.0f, 1.0);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClearDepth(1.0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        LOG(DEBUG) << "OpenGL state initialized";
    }

    nova_renderer::~nova_renderer() {
        inputs.reset();
        meshes.reset();
        textures.reset();
        ubo_manager.reset();
        game_window.reset();
    }

    void nova_renderer::render_frame() {
        glClear(GL_DEPTH_BUFFER_BIT);
        profiler::log_all_profiler_data();
        player_camera.recalculate_frustum();

        // Make geometry for any new chunks
        meshes->upload_new_geometry();

        update_per_frame_ubos();

        try {
            for(const auto &pass : passes_list) {
                execute_pass(pass);
            }
        } catch(std::exception& e) {
            LOG(ERROR) << "Error rendering: " << e.what();
        }

        game_window->end_frame();
    }

    bool nova_renderer::should_end() {
        // If the window wants to close, the user probably clicked on the "X" button
        return game_window->should_close();
    }

	std::unique_ptr<settings> nova_renderer::render_settings;

    void nova_renderer::init() {
		render_settings = std::make_unique<settings>("config/config.json");

		instance = std::make_unique<nova_renderer>();
    }

    void nova_renderer::execute_pass(const render_pass &pass) {
        if(materials_by_pass.find(pass.name) == materials_by_pass.end()) {
            LOG(WARNING) << "No materials found for pass " << pass.name;
            return;
        }

        const auto& materials = materials_by_pass.at(pass.name);
        for(const auto& mat : materials) {
            render_geometry_for_material(mat);
        }
    }

    void nova_renderer::render_geometry_for_material(const material &mat) {
        const auto& meshes_for_mat = meshes->get_meshes_for_shader(mat.name);

        if(!meshes_for_mat.empty()) {
            LOG(DEBUG) << "We have " << meshes_for_mat.size() << " objects for the material";
            set_opengl_state_for_material(mat);

            //glDisable(GL_DEPTH_TEST);
            //glDisable(GL_CULL_FACE);
            //glDisable(GL_BLEND);

            for(const auto &geom : meshes_for_mat) {
                upload_uniforms_for_object(geom, mat);

                if(geom.geometry->has_data()) {
                  switch(geom.type) {
                      case geometry_type::gui:

                          break;
                      default:
                            upload_model_matrix(geom, mat.program);
                  }
                    geom.geometry->set_active();
                    geom.geometry->draw();
                }
            }
        } else {
            LOG(WARNING) << "No meshes for material " << mat.name;
        }
    }

    void nova_renderer::upload_uniforms_for_object(const render_object &object, const material &mat) {
        switch(object.type) {
            case geometry_type::gui:
                upload_gui_model_matrix(*mat.program);
                break;
            default:
                LOG(WARNING) << "We don't handle geometry type " << geometry_type::to_string(object.type);
        }
    }

    void nova_renderer::set_opengl_state_for_material(const material &mat) {
        gl_context.set_default_state();

        if(mat.states) {
            for(const auto &state : mat.states.value()) {
                enable_state(state);
            }

            LOG(TRACE) << "Set states" << std::endl;
        }

        uint32_t stencil_ref = 0;
        if(mat.stencil_ref) {
            stencil_ref = mat.stencil_ref.value();

            LOG(TRACE) << "Got stencil ref" << std::endl;
        }

        if(mat.front_face) {
            gl_context.set_stencil_test_enabled(true);
            const auto& front_face_stencil = mat.front_face.value();
            set_up_stencil_test(GL_FRONT, front_face_stencil, stencil_ref);

            LOG(TRACE) << "Set stencil frontface operations" << std::endl;
        }

        if(mat.back_face) {
            gl_context.set_stencil_test_enabled(true);
            const auto& back_face_stencil = mat.back_face.value();
            set_up_stencil_test(GL_BACK, back_face_stencil, stencil_ref);

            LOG(TRACE) << "Set stencil backface operations" << std::endl;
        }

        if(mat.depth_func) {
            gl_context.set_depth_func(compare_op_to_gl(mat.depth_func.value()));

            LOG(TRACE) << "Set depth function" << std::endl;
        }

        if(mat.depth_bias || mat.slope_scaled_depth_bias) {
            float units = 0;
            float factor = 0;
            if(mat.depth_bias) {
                units = mat.depth_bias.value();
            }
            if(mat.slope_scaled_depth_bias) {
                factor = mat.slope_scaled_depth_bias.value();
            }
            gl_context.set_polygon_offset(factor, units);

            LOG(TRACE) << "Set polygon offset" << std::endl;
        }

        gl_context.set_shader(mat.program->gl_name);
        LOG(TRACE) << "Set shader" << std::endl;

        if(mat.input_textures) {
            for(const auto& texture_binding : mat.input_textures.value()) {
                const auto& texture = textures->get_texture(texture_binding.name);
                gl_context.bind_texture(texture, texture_binding.binding);
            }

            LOG(TRACE) << "Bound input textures" << std::endl;
        }

        if(mat.output_textures) {
            const auto &outputs = mat.output_textures.value();
            bool draws_to_backbuffer = false;
            for(const auto &output : outputs) {
                if(output.name == "Backbuffer") {
                    draws_to_backbuffer = true;
                }
            }
            if(draws_to_backbuffer) {
                gl_context.set_framebuffer(0);
            } else {
                if(framebuffers_by_material.find(mat.name) != framebuffers_by_material.end()) {
                    gl_context.set_framebuffer(framebuffers_by_material[mat.name].get_gl_name());
                } else {
                    LOG(WARNING) << "No framebuffer for material " << mat.name << std::endl;
                }
            }

            LOG(TRACE) << "Set output textures" << std::endl;
        }

        if(mat.source_blend_factor || mat.destination_blend_factor || mat.alpha_src || mat.alpha_dst) {
            set_up_blending(mat);

            LOG(TRACE) << "Set up blending" << std::endl;
        }

        gl_context.commit();
        LOG(TRACE) << "Committed OpenGL state" << std::endl;
    }

    void nova_renderer::set_up_stencil_test(const GLenum face, const stencil_op_state &front_face_stencil, const uint32_t ref) {
        auto fail_op = GL_KEEP;
        auto pass_op = GL_KEEP;
        auto depth_fail_op = GL_KEEP;

        if(front_face_stencil.fail_op) {
                fail_op = stencil_op_to_gl(front_face_stencil.fail_op.value());
            }
        if(front_face_stencil.pass_op) {
                pass_op = stencil_op_to_gl(front_face_stencil.pass_op.value());
            }
        if(front_face_stencil.depth_fail_op) {
                depth_fail_op = stencil_op_to_gl(front_face_stencil.depth_fail_op.value());
            }

        gl_context.set_stencil_op_separate(face, fail_op, pass_op, depth_fail_op);

        auto compare_op = GL_ALWAYS;
        auto stencil_mask = 0xFF;

        if(front_face_stencil.compare_op) {
                compare_op = compare_op_to_gl(front_face_stencil.compare_op.value());
            }
        if(front_face_stencil.compare_mask) {
                stencil_mask &= front_face_stencil.compare_mask.value();
            }
        if(front_face_stencil.write_mask) {
                stencil_mask &= front_face_stencil.write_mask.value();
            }

        gl_context.set_stencil_func_separate(face, compare_op, ref, stencil_mask);
    }

    void nova_renderer::set_up_blending(const material &mat) {
        GLenum src_color_factor = GL_ZERO;
        GLenum dst_color_factor = GL_ONE;
        GLenum src_alpha_factor = GL_ZERO;
        GLenum dst_alpha_factor = GL_ONE;

        if(mat.source_blend_factor) {
            src_color_factor = blend_factor_to_gl(mat.source_blend_factor.value());
        }
        if(mat.destination_blend_factor) {
            dst_color_factor = blend_factor_to_gl(mat.destination_blend_factor.value());
        }
        if(mat.alpha_src) {
            src_alpha_factor = blend_factor_to_gl(mat.alpha_src.value());
        }
        if(mat.alpha_dst) {
            dst_alpha_factor = blend_factor_to_gl(mat.alpha_dst.value());
        }

        gl_context.set_blending_params(src_color_factor, dst_color_factor, src_alpha_factor, dst_alpha_factor);
    }

    void nova_renderer::enable_state(const state_enum &state) {
        switch(state) {
            case state_enum::Blending:
                gl_context.set_blending_enabled(true);
                break;
            case state_enum::InvertCuling:
                gl_context.set_culling_mode(GL_FRONT);
                break;
            case state_enum::DisableCulling:
                gl_context.set_culling_enabled(false);
                break;
            case state_enum::DisableDepthWrite:
                gl_context.set_depth_write_enabled(false);
                break;
            case state_enum::DisableDepthTest:
                gl_context.set_depth_test_enabled(false);
                break;
            case state_enum::EnableStencilTest:
                gl_context.set_stencil_test_enabled(true);
                break;
            case state_enum::StencilWrite:
                gl_context.set_stencil_write_enabled(true);
                break;
            case state_enum::DisableColorWrite:
                gl_context.set_color_write_enabled(false);
                break;
            case state_enum::EnableAlphaToCoverage:
                gl_context.set_alpha_to_coverage_enabled(true);
                break;
            case state_enum::DisableAlphaWrite:
                gl_context.set_alpha_write_enabled(false);
                break;
            default:
                LOG(WARNING) << "State " << state_enum::to_string(state) << " not handled :(";
                break;
        }
    }

    void nova_renderer::on_config_change(nlohmann::json &new_config) {
		auto& shaderpack_name = new_config["loadedShaderpack"];
        load_new_shaderpack(shaderpack_name);

        LOG(DEBUG) << "Finished dealing with new shaderpack";
    }

    void nova_renderer::on_config_loaded(nlohmann::json &config) {
        // TODO: Probably want to do some setup here, don't need to do that now
    }

    settings &nova_renderer::get_render_settings() {
        return *render_settings;
    }

    texture_manager &nova_renderer::get_texture_manager() {
        return *textures;
    }

	glfw_gl_window &nova_renderer::get_game_window() {
		return *game_window;
	}

	input_handler &nova_renderer::get_input_handler() {
		return *inputs;
	}

    mesh_store &nova_renderer::get_mesh_store() {
        return *meshes;
    }

    const std::vector<const material *> nova_renderer::get_all_materials() {
        auto materials = std::vector<const material *>{};
        for(const auto& item : materials_by_pass) {
            for(const auto& mat : item.second) {
                materials.push_back(&mat);
            }
        }

        return materials;
    }

    void nova_renderer::load_new_shaderpack(const std::string &new_shaderpack_name) {
        /*
         * We need to:
         *  - Load the render passes. A render pass has a shader, so that's fine
         *  - Fill in the render passes from the values in the shader comments and shader.properties and whatever other
         *      datafiles Optifine uses :(
         *  - Load the resources file from the shaderpack (or use the default one if the shaderpack doesn't provide one)
         */

        /*
         * Load the render passes. The loading functions should fill in values from the shaderpack and load in shader
         * settings that the shader files are old enough to define
         *
         * TODO: Examine shaders for rendering parameters
         */
        LOG(INFO) << "Loading shaderpack " << new_shaderpack_name << "...";
        auto shaderpack = load_shaderpack(new_shaderpack_name);

        materials_by_pass = shaderpack.materials_by_pass;

        LOG(INFO) << "Compiling passes...";
        try {
            passes_list = compile_into_list(shaderpack.passes);
        } catch(render_graph_validation_error& e) {
            LOG(ERROR) << "Could not load shaderpack " << new_shaderpack_name << ": " << e.what();

            // TODO: Find a good way to propagate the error
            return;
        }

        LOG(INFO) << "Initializing framebuffer attachments...";
        textures->create_dynamic_textures(shaderpack.dynamic_textures, passes_list);

        LOG(INFO) << "Building framebuffers...";
        create_framebuffers_from_shaderpack();

        LOG(INFO) << "Wiring renderer together...";
        link_up_uniform_buffers(materials_by_pass, *ubo_manager);

        LOG(INFO) << "Loaded shaderpack " << new_shaderpack_name;
    }

    void nova_renderer::create_framebuffers_from_shaderpack() {
        for(const auto& pass_materials : materials_by_pass) {
            for(const auto& mat : pass_materials.second) {
                // A smart algorithm would look at each material any only create one framebuffer for each set of bound
                // textures... but I don't feel like being clever

                framebuffers_by_material[mat.name] = make_framebuffer_for_material(mat);
            }
        }
    }

    framebuffer nova_renderer::make_framebuffer_for_material(const material &mat) {
        auto fb = framebuffer();

        if(mat.output_textures) {
            for(const auto &output : mat.output_textures.value()) {
                if(output.name == "Backbuffer") {
                    // The backbuffer just exists,
                    continue;
                }
                const auto& output_tex = textures->get_texture(output.name);
                fb.add_color_attachment(output.binding, output_tex.get_gl_name());
            }
        }

        if(mat.depth_texture) {
            const auto& depth = mat.depth_texture.value();
            const auto& depth_tex = textures->get_texture(depth.name);
            fb.set_depth_buffer(depth_tex.get_gl_name());
        }

        return fb;
    }

    void nova_renderer::deinit() {
        instance.release();
    }

    inline void nova_renderer::upload_model_matrix(const render_object &geom, std::shared_ptr<gl_shader_program> program) const {
        glm::mat4 model_matrix = glm::translate(glm::mat4(1), geom.position);

        auto model_matrix_location = program->get_uniform_location("gbufferModel");
        glUniformMatrix4fv(model_matrix_location, 1, GL_FALSE, &model_matrix[0][0]);
    }

    void nova_renderer::upload_gui_model_matrix(gl_shader_program &program) {
        auto config = render_settings->get_options()["settings"];
        float view_width = config["viewWidth"];
        float view_height = config["viewHeight"];
        float scalefactor = config["scalefactor"];
        // The GUI matrix is super simple, just a viewport transformation
        glm::mat4 gui_model(1.0f);
        gui_model = glm::translate(gui_model, glm::vec3(-1.0f, 1.0f, 0.0f));
        gui_model = glm::scale(gui_model, glm::vec3(scalefactor, scalefactor, 1.0f));
        gui_model = glm::scale(gui_model, glm::vec3(1.0 / view_width, 1.0 / view_height, 1.0));
        gui_model = glm::scale(gui_model, glm::vec3(1.0f, -1.0f, 1.0f));

        LOG(DEBUG) << "view_width=" << view_width << " view_height=" << view_height << " scalefactor=" << scalefactor;

        auto model_matrix_location = program.get_uniform_location("gbufferModel");
        LOG(DEBUG) << "Uploading matrix " << gui_model << " to uniform location " << model_matrix_location << " on shader " << program.gl_name;

        glUniformMatrix4fv(model_matrix_location, 1, GL_FALSE, &gui_model[0][0]);
    }

    void nova_renderer::update_per_frame_ubos() {
        // Big thing here is to update the camera's matrices

        auto& per_frame_ubo = ubo_manager->get_per_frame_uniforms();

        auto per_frame_uniform_data = per_frame_uniforms{};
        per_frame_uniform_data.gbufferProjection = player_camera.get_projection_matrix();
        per_frame_uniform_data.gbufferModelView = player_camera.get_view_matrix();

        per_frame_ubo.send_data(per_frame_uniform_data);
    }

    camera &nova_renderer::get_player_camera() {
        return player_camera;
    }

    std::vector<render_pass> compile_into_list(std::unordered_map<std::string, render_pass> passes) {
        auto passes_dependency_order = order_passes(passes);
        auto ordered_passes = std::vector<render_pass>{};

        for(const auto& pass_name : passes_dependency_order) {
            ordered_passes.push_back(passes[pass_name]);
        }

        return ordered_passes;
    }

    void link_up_uniform_buffers(std::unordered_map<std::string, std::vector<material>> &materials, uniform_buffer_store &ubos) {
        for(const auto& item : materials) {
            for(const auto& mat : item.second) {
                ubos.register_all_buffers_with_shader(mat.program);
            }
        }
    }

    std::string translate_debug_source(GLenum source) {
        switch(source) {
            case GL_DEBUG_SOURCE_API:
                return "API";
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                return "window system";
            case GL_DEBUG_SOURCE_SHADER_COMPILER:
                return "shader compiler";
            case GL_DEBUG_SOURCE_THIRD_PARTY:
                return "third party";
            case GL_DEBUG_SOURCE_APPLICATION:
                return "application";
            case GL_DEBUG_SOURCE_OTHER:
                return "other";
            default:
                return "something else somehow";
        }
    }

    std::string translate_debug_type(GLenum type) {
        switch(type) {
            case GL_DEBUG_TYPE_ERROR:
                return "error";
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                return "some behavior marked deprecated has been used";
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                return "something has invoked undefined behavior";
            case GL_DEBUG_TYPE_PORTABILITY:
                return "some functionality the user relies upon is not portable";
            case GL_DEBUG_TYPE_PERFORMANCE:
                return "code has triggered possible performance issues";
            case GL_DEBUG_TYPE_MARKER:
                return "command stream annotation";
            case GL_DEBUG_TYPE_PUSH_GROUP:
                return "group pushing";
            case GL_DEBUG_TYPE_POP_GROUP:
                return "group popping";
            case GL_DEBUG_TYPE_OTHER:
                return "other";
            default:
                return "something else somwhow";
        }
    }

    void APIENTRY
    debug_logger(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message,
                 const void *user_param) {
        std::string source_name = translate_debug_source(source);
        std::string type_name = translate_debug_type(type);

        switch(severity) {
            case GL_DEBUG_SEVERITY_HIGH:
                LOG(ERROR) << id << " - Message from " << source_name << " of type " << type_name << ": "
                           << message;
                break;

            case GL_DEBUG_SEVERITY_MEDIUM:
                LOG(INFO) << id << " - Message from " << source_name << " of type " << type_name << ": " << message;
                break;

            case GL_DEBUG_SEVERITY_LOW:
                LOG(DEBUG) << id << " - Message from " << source_name << " of type " << type_name << ": "
                           << message;
                break;

            case GL_DEBUG_SEVERITY_NOTIFICATION:
                LOG(TRACE) << id << " - Message from " << source_name << " of type " << type_name << ": "
                           << message;
                break;

            default:
                LOG(INFO) << id << " - Message from " << source_name << " of type " << type_name << ": " << message;
        }
    }

    void nova_renderer::enable_debug() {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(debug_logger, nullptr);
    }
}
