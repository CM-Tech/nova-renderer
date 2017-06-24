//
// Created by David on 25-Dec-15.
//

#include "nova_renderer.h"
#include "../utils/utils.h"
#include "../data_loading/loaders/loaders.h"

#include <easylogging++.h>
#include <glm/gtc/matrix_transform.hpp>

INITIALIZE_EASYLOGGINGPP

namespace nova {
    std::unique_ptr<nova_renderer> nova_renderer::instance;

    nova_renderer::nova_renderer(){
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

        init_opengl_state();
    }

    void nova_renderer::init_opengl_state() const {
        glClearColor(0.0, 0.0, 0.0, 1.0);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClearDepth(1.0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    nova_renderer::~nova_renderer() {

        inputs.reset();
        meshes.reset();
        textures.reset();
        ubo_manager.reset();
        game_window.reset();

    }

    void nova_renderer::render_frame() {
        // Make geometry for any new chunks
        meshes->generate_needed_chunk_geometry();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // upload shadow UBO things

        render_shadow_pass();

        update_gbuffer_ubos();

        render_gbuffers();

        //render_gbuffers();

        render_composite_passes();

        render_final_pass();

        // We want to draw the GUI on top of the other things, so we'll render it last
        // Additionally, I could use the stencil buffer to not draw MC underneath the GUI. Could be a fun
        // optimization - I'd have to watch out for when the user hides the GUI, though. I can just re-render the
        // stencil buffer when the GUI screen changes
        render_gui();

        game_window->end_frame();
    }

    void nova_renderer::render_shadow_pass() {
        //shadow_camera.position = {100, 100, 100};
        //shadow_camera.rotation = {0, -1};
        LOG(TRACE) << "Rendering shadow pass";
        /*auto settings = render_settings->get_options()["settings"];
        // size of the shadow map
        GLuint shadowMapSize = 1024;//settings["shadowMapResolution"];
GLuint shadowFramebuffer;
//glClear(GL_DEPTH_BUFFER_BIT);
//glGenFramebuffers(1, &shadowFramebuffer);
LOG(INFO) << "Bind Shadow";
shadow_framebuffer->bind();
glClear(GL_DEPTH_BUFFER_BIT);
//shadow_framebuffer->check_status();
LOG(INFO) << "End Bind Shadow";*/
        // create the shadow map
        /*GLuint shadowMap;
        glGenTextures(1, &shadowMap);
        glBindTexture(GL_TEXTURE_2D, shadowMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, shadowMapSize, shadowMapSize, 
                        0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // GL_CLAMP_TO_EDGE setups the shadow map in such a way that
        // fragments for which the shadow map is undefined
        // will get values from closest edges of the shadow map
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // comparison mode of the shadow map
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_TEXTURE_2D, shadowMap, 1);
        ////////////////////////////////////////////////////
*/
        // create framebuffer
        //LOG(INFO) << "Bind Shadow";
        //glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
        
        /*auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    LOG(INFO) << "Framebuffer not complete: " << fboStatus;
        LOG(INFO) << "Done Bind Shadow";*/
        

        // depth is stored in z-buffer to which the shadow map is attached,
        // so there is no need for any color buffers
        //glDrawBuffer(GL_NONE);
        //auto& terrain_shader = loaded_shaderpack->get_shader("gbuffers_terrain");
        //render_shader(terrain_shader);
    }

    void nova_renderer::render_gbuffers() {
        LOG(TRACE) << "Rendering gbuffer pass";
        main_framebuffer->bind();
        //main_framebuffer->check_status();
        

        // TODO: Get shaders with gbuffers prefix, draw transparents last, etc
        auto& terrain_shader = loaded_shaderpack->get_shader("gbuffers_terrain");
        render_shader(terrain_shader);
        auto& water_shader = loaded_shaderpack->get_shader("gbuffers_water");
        render_shader(water_shader);
    }

    void nova_renderer::render_composite_passes() {
        LOG(TRACE) << "Rendering composite passes";
    }

    void nova_renderer::render_final_pass() {
        LOG(TRACE) << "Rendering final pass";
        // The fullscreen quad's FBO
        GLuint quad_VertexArrayID;
        glGenVertexArrays(1, &quad_VertexArrayID);
        glBindVertexArray(quad_VertexArrayID);

        static const GLfloat g_quad_vertex_buffer_data[] = {
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            1.0f,  1.0f, 0.0f,
        };

        GLuint quad_vertexbuffer;
        glGenBuffers(1, &quad_vertexbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertex_buffer_data), g_quad_vertex_buffer_data, GL_STATIC_DRAW);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        auto& final_shader = loaded_shaderpack->get_shader("final");
        final_shader.bind();
        
        
    }

    void nova_renderer::render_gui() {
        LOG(TRACE) << "Rendering GUI";
        glClear(GL_DEPTH_BUFFER_BIT);

        // Bind all the GUI data
        auto &gui_shader = loaded_shaderpack->get_shader("gui");
        gui_shader.bind();

        upload_gui_model_matrix(gui_shader);

        // Render GUI objects
        std::vector<render_object>& gui_geometry = meshes->get_meshes_for_shader("gui");
        for(const auto& geom : gui_geometry) {
            if (geom.color_texture != "") {
                auto color_texture = textures->get_texture(geom.color_texture);
                color_texture.bind(0);
            }
            geom.geometry->set_active();
            geom.geometry->draw();
        }
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
                return "something else somehow";
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
        glDebugMessageCallback(debug_logger, NULL);
    }

    void nova_renderer::on_config_change(nlohmann::json &new_config) {
		auto& shaderpack_name = new_config["loadedShaderpack"];
        if(!loaded_shaderpack || (loaded_shaderpack && shaderpack_name != loaded_shaderpack->get_name())) {
            load_new_shaderpack(shaderpack_name);
        }
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

    void nova_renderer::load_new_shaderpack(const std::string &new_shaderpack_name) {
		
        LOG(INFO) << "Loading shaderpack " << new_shaderpack_name;
        loaded_shaderpack = std::make_shared<shaderpack>(load_shaderpack(new_shaderpack_name));
        meshes->set_shaderpack(loaded_shaderpack);
        LOG(INFO) << "Loading complete";
		
        link_up_uniform_buffers(loaded_shaderpack->get_loaded_shaders(), *ubo_manager);
        LOG(DEBUG) << "Linked up UBOs";

        create_framebuffers_from_shaderpack();
    }

    void nova_renderer::create_framebuffers_from_shaderpack() {
        // TODO: Examine the shaderpack and determine what's needed
        // For now, just create framebuffers with all possible attachments

        auto settings = render_settings->get_options()["settings"];
        int viewWidth=854;//(settings["viewWidth"]); 
        int viewHeight =480;//(settings["viewHeight"]);
        main_framebuffer_builder.set_framebuffer_size(settings["viewWidth"],settings["viewHeight"])
                                .enable_color_attachment(0)
                                .enable_color_attachment(1)
                                .enable_color_attachment(2)
                                .enable_color_attachment(3)
                                .enable_color_attachment(4)
                                .enable_color_attachment(5)
                                .enable_color_attachment(6)
                                .enable_color_attachment(7);

        main_framebuffer = std::make_unique<framebuffer>(main_framebuffer_builder.build());
        LOG(INFO) << "Make Shadow SIZE "<< settings["shadowMapResolution"];//settings["shadowMapResolution"];
        shadow_framebuffer_builder.set_framebuffer_size(settings["shadowMapResolution"], settings["shadowMapResolution"])
                                  .enable_color_attachment(0)
                                  .enable_color_attachment(1)
                                  .enable_color_attachment(2)
                                  .enable_color_attachment(3);

        shadow_framebuffer = std::make_unique<framebuffer>(shadow_framebuffer_builder.build());
        /*GLuint shadowMap;
        glGenTextures(1, &shadowMap);
        shadow_depth_textures[0]=shadowMap;
        shadow_framebuffer->set_depth_buffer(shadow_depth_textures[0]);*/
        LOG(INFO) << "Made Shadow Frame Buffer";
    }

    void nova_renderer::deinit() {
        instance.release();
    }
    void nova_renderer::render_shader(gl_shader_program &shader) {
        LOG(TRACE) << "Rendering everything for shader " << shader.get_name();
        shader.bind();

        auto& geometry = meshes->get_meshes_for_shader(shader.get_name());
        for(auto& geom : geometry) {
            if(geom.color_texture != "") {
                auto color_texture = textures->get_texture(geom.color_texture);
                color_texture.bind(0);
            }

            if(geom.normalmap) {
                textures->get_texture(*geom.normalmap).bind(1);
            }

            if(geom.data_texture) {
                textures->get_texture(*geom.data_texture).bind(2);
            }
            //glBindTexture (GL_TEXTURE_2D, shadow_depth_textures[0]);
            //glActiveTexture(GL_TEXTURE3);
            //shadowMap.bind(3);

            upload_model_matrix(geom, shader);

            geom.geometry->set_active();
            geom.geometry->draw();
        }
    }
    

    void nova_renderer::upload_model_matrix(render_object &geom, gl_shader_program &program) const {
        glm::mat4 model_matrix = glm::translate(glm::mat4(1), geom.position);

        auto model_matrix_location = program.get_uniform_location("gbufferModel");
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

        auto model_matrix_location = program.get_uniform_location("gbufferModel");

        glUniformMatrix4fv(model_matrix_location, 1, GL_FALSE, &gui_model[0][0]);
    }

    void nova_renderer::update_gbuffer_ubos() {
        // Big thing here is to update the camera's matrices

        auto& per_frame_ubo = ubo_manager->get_per_frame_uniforms();

        auto per_frame_uniform_data = per_frame_uniforms{};
        per_frame_uniform_data.gbufferProjection = player_camera.get_projection_matrix();
        per_frame_uniform_data.gbufferModelView = player_camera.get_view_matrix();
        per_frame_uniform_data.shadowProjection = shadow_camera.get_projection_matrix();
        per_frame_uniform_data.shadowModelView = shadow_camera.get_view_matrix();
        per_frame_ubo.send_data(per_frame_uniform_data);
    }

    camera &nova_renderer::get_player_camera() {
        return player_camera;
    }
    camera &nova_renderer::get_shadow_camera() {
        return shadow_camera;
    }

    void link_up_uniform_buffers(std::unordered_map<std::string, gl_shader_program> &shaders, uniform_buffer_store &ubos) {
        nova::foreach(shaders, [&](auto shader) { ubos.register_all_buffers_with_shader(shader.second); });
    }
}

