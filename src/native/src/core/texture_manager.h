/*!
 * \author David
 * \date 29-Apr-16.
 */

#ifndef RENDERER_TEXTURE_RECEIVER_H
#define RENDERER_TEXTURE_RECEIVER_H

#include <string>
#include <GL/glm/glm.hpp>
#include <map>
#include "../mc/mc_objects.h"
#include "../gl/glad/glad.h"
#include "../interfaces/itexture.h"
#include "../gl/objects/texture2D.h"
#include "../gl/core/opengl_wrapper.h"

/*!
 * \brief Holds all the textures that the Nova Renderer can deal with
 *
 * This class does a few things. I'm going to walk you through a couple usage scenarios because it's way easier for me
 * to explain things that way
 *
 * \par Loading a resource pack:
 * When MC loads a resource pack, the Java Nova code should reset the texture manager. That clears out all existing
 * textures, freeing up the VRAM and RAM they used. Next, the Nova Renderer loops through all the textures it cares
 * about, which is super gross because I have to hardcode the values it cares about but I don't know a better way to do
 * is yet, and gets each texture from the resource pack. It sends each texture to the texture manager by way of
 * nova_renderer#add_texture(mc_texture). Once all textures have been loaded, the Nova Renderer calls
 * nova_renderer#finalize_textures, which tells the texture manager (this thing) to stitch as many textures as possible
 * into a texture atlas and generate a mapping from texture place in the atlas to texture name, such that someone can
 * call texture_manager#get_texture_location(std::string) and get back a texture_location struct, which has the GL ID
 * of the requested texture, the minimum UV coordinates that refer to that texture, and the maximum UV coordinates that
 * refer to that texture. This is useful mostly when building chunk geometry, so I can assign the right UV coordinates
 * to each triangle.
 *
 * \par Rendering the world:
 * This class won't perform a lot of actions while rendering the world. Mostly I'll just be like "I need the terrain
 * texture" or "I really need the entity texture". I'm going to be using texture atlases as much as possible. Anyway,
 * I'll ask the texture manager for a certain texture atlas, and the texture manager will give it back to me. Then, I
 * can bind that texture and render my pants off.
 */
class texture_manager {
public:
    /*!
     * \brief Identifies which atlas a texture is
     */
    enum class atlas_type {
        TERRAIN,    //!< The atlas for textures used by the terrain
        ENTITIES,   //!< The atlas for textures used by entities
        GUI,        //!< The atlas for textures used by the GUI
        PARTICLES,  //!< The atlas for textures used by particles
        EFFECTS,    //!< The atlas for textures used by effects, such as the underwater overlay
        FONT,       //!< The atlas for textures in the current font
        NUM_ATLASES,
    };

    /*!
     * \brief Identifies which sort of data is stored in each texture atlas
     */
    enum class texture_type {
        ALBEDO,     //!< The texture holds albedo information. I expect at least one albedo texture for each atlas
        NORMAL,     //!< The texture holds normals. I expect there will only be a normals texture for terrain and entities. Maybe particles later on
        SPECULAR,   //!< The texture holds specular data. Same expectations as normals
    };

    /*!
     * \brief Tells you the texture ID and min/max UV coordinates of a texture in an atlas
     *
     * The name of the texture in the atlas is used as a key in a hash map
     *
     * The exact atlas is not identified here. That's because I expect the caller to know what kind of texture they
     * have. if you're making the terrain, you know you need the terrain texture.
     */
    struct texture_location {
        GLuint atlas_id;    //!< The OpenGL name for the atlas texture. Might get removed later on, idk
        glm::ivec2 min;     //!< The minimum UV coordinate of the requested texture in its atlas
        glm::ivec2 max;     //!< The maximum UV coordinate of the requested texture in its atlas
    };

    /*!
     * \brief Initializes the texture_manager. Doesn't do anything special.
     */
    texture_manager(opengl_wrapper * wrapper);

    /*!
     * \brief De-allocates everything ths texture_manager uses
     *
     * The destructor calls #reset to de-allocate all OpenGL textures. All other memory should get cleaned up when it
     * does out of scope
     */
    ~texture_manager();

    /*!
     * \brief De-allocates all OpenGL textures and clears all data, making way for a new resource pack's textures
     */
    void reset();

    /*!
     * \brief Adds a texture to this resource manager
     *
     * The texture is not put into an atlas immediately. Rather, it is held in a staging area until #finalize_textures
     * is called. Then it's put into an atlas
     *
     * \param make_texture_2D The new texture
     */
    void add_texture(mc_texture & new_texture);

    /*!
     * \brief Takes all the textures from the staging areas and puts them into multiple atlases
     *
     * This method determines which atlas a texture should go into by looking at its resource location. The texture is
     * assigned an atlas based on what folder it's in
     *
     * This method also remembers where in each atlas eachtexture went, and maintains the locations map
     */
    void finalize_textures();

    /*!
     * \brief Retrieves the texture location for a texture with a specific name
     *
     * \param texture_name The MC resource location of the texture to get the location of. This name should be the
     * exact MC name of the texture
     * \return The location of the requested texture
     */
    const texture_location & get_texture_location(std::string &texture_name);

    /*!
     * \brief Returns a pointer to the specified atlas
     *
     * \param atlas The type of atlas to get
     * \param type The type of data that should be in the atlas
     * \return A pointer to the atlas texture
     */
    itexture * get_texture_atlas(atlas_type atlas, texture_type type);

private:
    std::vector<mc_texture> loaded_textures;
    std::map<std::pair<atlas_type, texture_type>, texture2D> atlases;
    std::map<std::string, texture_location> locations;

    opengl_wrapper * gl_wrapper;

    void pack_into_atlas(std::vector<mc_texture> &textures_to_pack, texture2D &atlas);
};


#endif //RENDERER_TEXTURE_RECEIVER_H
