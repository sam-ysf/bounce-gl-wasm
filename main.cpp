#include "box.hpp"
#include "box_data.hpp"
#include "camera.hpp"
#include "draw_instanced_no_texture.hpp"
#include "draw_instanced_with_texture.hpp"
#include "grid_square.hpp"
#include "images/awesome_face.h"
#include "images/brick_wall.h"
#include "images/incredulous_face.h"
#include "images/shocked_face.h"
#include "images/tiles/dark_grass.h"
#include "images/tiles/dry_grass.h"
#include "square.hpp"
#include "texture.hpp"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles.h>
#include <emscripten.h>
#include <memory>
#include <vector>

namespace {

    /*! Helper
     *! Builds the vertices for the map grid
     */
    std::vector<calc::Mat4f> build_grid(unsigned width, unsigned length)
    {
        const auto xinst
            = static_cast<int>(std::ceil(static_cast<float>(width) / 2.0F));
        const auto yinst
            = static_cast<int>(std::ceil(static_cast<float>(length) / 2.0F));

        calc::Mat4f model = calc::Mat4f::identity();
        float& x = model[3][0];
        float& y = model[3][1];

        std::vector<calc::Mat4f> grid;
        for (int i = -xinst; i != xinst; ++i) {
            for (int j = -yinst; j != yinst; ++j) {
                x = static_cast<float>(i) + 0.5F;
                y = static_cast<float>(j) + 0.5F;
                grid.push_back(model);
            }
        }

        return grid;
    }

    /*! Helper
     *! Build the vertices for the map wall
     */
    std::vector<calc::Mat4f> build_wall(unsigned width, unsigned length)
    {
        std::vector<calc::Mat4f> wall;

        const int lengthSpan = (length / 2) / 3;
        const int widthSpan = (width / 2) / 3;

        // West wall
        for (int i = 1 - lengthSpan; i != lengthSpan; ++i) {
            calc::Mat4f mat = calc::Mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = (static_cast<float>(width) / 2.0F) - 1.0F;
            mat[1][3] = static_cast<float>(i) * 3.0F;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // East wall
        for (int i = 1 - lengthSpan; i != lengthSpan; ++i) {
            calc::Mat4f mat = calc::Mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = 1 - (static_cast<float>(width) / 2.0F);
            mat[1][3] = static_cast<float>(i) * 3.0F;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // North wall
        for (int i = 1 - widthSpan; i != widthSpan; ++i) {
            calc::Mat4f mat = calc::Mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = static_cast<float>(i) * 3.0F;
            mat[1][3] = (static_cast<float>(length) / 2.0F) - 1;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // South wall
        for (int i = 1 - widthSpan; i != widthSpan; ++i) {
            calc::Mat4f mat = calc::Mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = static_cast<float>(i) * 3.0F;
            mat[1][3] = 1 - (static_cast<float>(length) / 2.0F);
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        return wall;
    }

    /*! Helper
     *! Converts matrix to float data
     */
    inline std::vector<float> copy_matrix_data(
        const std::vector<calc::Mat4f>& mats)
    {
        std::vector<float> values(mats.size() * 16);
        for (std::size_t i = 0; i != mats.size(); ++i)
            std::memcpy(
                &values[i * 16], calc::data(mats[i]), sizeof(calc::Mat4f));
        return values;
    }
} // namespace

namespace {

    /*! Class Runner
     *! Encapsulates the main loop
     */
    class Runner {
    public:
        ~Runner()
        {
            discard_textures();
        }

        /*! Ctor.
         */
        Runner(SDL_Window* window, const std::shared_ptr<Camera>& camera)
            : window_(window)
            , camera_(camera)
        {
            static const unsigned kWidth = 30;
            static const unsigned kHeight = 30;

            try {
                textureHandles_
                    = std::array<unsigned, 9>{0, 0, 0, 0, 0, 0, 0, 0, 0};

                unsigned tao1 = render::load_texture_from_data(
                    kBrickWall, kBrickWallLen, false);
                if (tao1 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[0] = tao1;

                unsigned tao2 = render::load_texture_from_data(
                    kAwesomeFace, kAwesomeFaceLen, true);
                if (tao2 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[1] = tao2;

                unsigned tao3 = render::load_texture_from_data(
                    kShockedFace, kShockedFaceLen, true);
                if (tao3 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[2] = tao3;

                unsigned tao4 = render::load_texture_from_data(
                    kIncredulousFace, kIncredulousFaceLen, true);
                if (tao4 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[3] = tao4;

                unsigned tao5 = render::load_texture_from_data(
                    kAwesomeFace, kAwesomeFaceLen, false);
                if (tao5 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[4] = tao5;

                unsigned tao6 = render::load_texture_from_data(
                    kShockedFace, kShockedFaceLen, false);
                if (tao6 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[5] = tao6;

                unsigned tao7 = render::load_texture_from_data(
                    kIncredulousFace, kIncredulousFaceLen, false);
                if (tao7 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[6] = tao7;

                unsigned tao8 = render::load_texture_from_data(
                    kDryGrass, kDryGrassLen, false);
                if (tao8 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[7] = tao8;

                unsigned tao9 = render::load_texture_from_data(
                    kDarkGrass, kDarkGrassLen, false);
                if (tao9 == 0) {
                    throw std::runtime_error("Invalid texture");
                }

                textureHandles_[8] = tao9;

                // Load boxes
                unsigned boxTAO1[] = {tao1, tao2};
                unsigned boxTAO2[] = {tao1, tao3};
                unsigned boxTAO3[] = {tao1, tao4};
                wallTAO_ = {tao1, tao1};

                boxSkins_.push_back(tao5);
                boxSkins_.push_back(tao6);
                boxSkins_.push_back(tao7);

                boxObject_[0] = std::make_unique<render::Box>(
                    boxTAO1, std::size(boxTAO1), 1);
                boxObject_[0]->push_back((calc::Mat4f::identity()).data());

                boxObject_[1] = std::make_unique<render::Box>(
                    boxTAO2, std::size(boxTAO2), 1);
                boxObject_[1]->push_back((calc::Mat4f::identity()).data());

                boxObject_[2] = std::make_unique<render::Box>(
                    boxTAO3, std::size(boxTAO3), 1);
                boxObject_[2]->push_back((calc::Mat4f::identity()).data());

                dryGrassTileTAO_ = {tao8, tao8};

                // Load map...
                ranges_ = Ranges(kWidth, kHeight);

                // Load tiles
                load_grid();

                // Load tiles
                load_wall();

                // Load tiles
                load_dry_grass();

                // Load tiles
                load_fresh_grass(tao9);

            } catch (...) {
                discard_textures();
                throw;
            }
        }

        /*! Run loop
         */
        void run()
        {
            // Handle events on queue
            SDL_Event e;
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    return;
                }

                if (e.type == SDL_WINDOWEVENT) {
                    on_window_event(e);
                }
            }

            render();
        }

        /*! Evt. handler
         */
        void on_window_event(const SDL_Event& e) const
        {
            if ((e.window).event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                int screenWidth = std::max<int>(e.window.data1, 1);
                int screenHeight = std::max<int>(e.window.data2, 1);
                glViewport(0, 0, screenWidth, screenHeight);

                // Update camera
                camera_->resize(static_cast<unsigned>(screenWidth),
                                static_cast<unsigned>(screenHeight));
                camera_->update();
            }
        }

        /*! Renders the scene
         */
        void render()
        {
            // Clear it
            glClearColor(backgroundColor_[0],
                         backgroundColor_[1],
                         backgroundColor_[2],
                         1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const calc::Mat4f& lookAt = camera_->get_device_look_at();
            const calc::Mat4f& projection = camera_->get_device_projection();
            // Maybe draw the grid
            if (gridEnabled_) {
                gridDraw_.use();
                gridDraw_.set_color(gridColor_);
                gridDraw_.set_scene(lookAt, projection);
                gridTile_->draw();
            }

            // Draw the wall
            mainDraw_.use();
            mainDraw_.set_scene(lookAt, projection);
            wallObject_->draw();

            // Draw the grass inside the cage
            grassTile_->draw();
            // Draw the grass outside the cage
            dryGrassTile_->draw();

            // Draw the box
            calc::Vec3f& direction = boxData_.direction;
            calc::Vec3f& speed = boxData_.speed;
            calc::Mat4f& translation = boxData_.translation;

            translation[0][3] += speed[0] * direction[0];
            translation[1][3] += speed[1] * direction[1];
            translation[2][3] = -1.0;

            float x = translation[0][3];
            float y = translation[1][3];

            // Bounce back on wall hit
            float hitOffset = 3.0;
            if (x < (hitOffset - (cageWidth_ / 2))
                || x > ((cageWidth_ / 2) - hitOffset)) {
                direction[0] *= -1;
            }

            if (y < (hitOffset - (cageLength_ / 2))
                || y > ((cageLength_ / 2) - hitOffset)) {
                direction[1] *= -1;
            }

            const calc::Vec3f turnRate
                = boxData_.turnRate
                  * calc::radians(static_cast<float>(SDL_GetTicks()) / 10.0F);
            const calc::Mat4f boxMat = calc::transpose(
                translation * calc::rotate_4x(turnRate[0])
                * calc::rotate_4y(turnRate[1]) * calc::rotate_4z(turnRate[2]));
            // Do the draw call
            render::Box& boxObject = *boxObject_[boxData_.selectedSkin];
            boxObject.modify(calc::data(boxMat), 0);
            boxObject.draw();
            // Update screen & return
            SDL_GL_SwapWindow(window_);
        }

        void enable_grid(bool state)
        {
            gridEnabled_ = state;
        }

        bool get_grid_state() const
        {
            return gridEnabled_;
        }

        void set_background_color(const calc::Vec4f& vec)
        {
            backgroundColor_ = vec;
        }

        void get_background_color(calc::Vec4f& vec) const
        {
            vec = backgroundColor_;
        }

        void set_grid_color(const calc::Vec4f& vec)
        {
            gridColor_ = vec;
        }

        const calc::Vec4f& get_grid_color() const
        {
            return gridColor_;
        }

        BoxData& get_box()
        {
            return boxData_;
        }

        const BoxData& get_box() const
        {
            return boxData_;
        }

        Camera& get_camera()
        {
            return *camera_;
        }

        const Camera& get_camera() const
        {
            return *camera_;
        }
    private:
        void discard_textures()
        {
            for (unsigned tao: textureHandles_) {
                if (tao > 0) {
                    render::delete_texture(tao);
                }
            }

            textureHandles_
                = std::array<unsigned, 9>{0, 0, 0, 0, 0, 0, 0, 0, 0};
        }

        void load_grid()
        {
            const std::vector<float> grid = copy_matrix_data(
                build_grid(ranges_.gridWidth, ranges_.gridLength));
            gridTile_ = std::make_unique<render::GridSquare>(
                ranges_.gridWidth * ranges_.gridLength);
            gridTile_->reset(grid.data(),
                             static_cast<unsigned>(grid.size() / 16));
        }

        void load_wall()
        {
            // Load wall
            const std::vector<float> wall = copy_matrix_data(
                build_wall(ranges_.cageWidth, ranges_.cageLength));
            wallObject_ = std::make_unique<render::Box>(
                wallTAO_.value,
                static_cast<unsigned>(std::size(wallTAO_.value)),
                ranges_.cageWidth * ranges_.cageLength);
            wallObject_->reset(wall.data(),
                               static_cast<unsigned>(wall.size() / 16));
        }

        void load_dry_grass()
        {
            const int cageMinWidth = ranges_.cageMinWidth;
            const int cageMaxWidth = ranges_.cageMaxWidth;

            const int cageMinLength = ranges_.cageMinLength;
            const int cageMaxLength = ranges_.cageMaxLength;

            const int gridMinWidth = ranges_.gridMinWidth;
            const int gridMaxWidth = ranges_.gridMaxWidth;

            const int gridMinLength = ranges_.gridMinLength;
            const int gridMaxLength = ranges_.gridMaxLength;

            dryGrassTile_ = std::make_unique<render::Square>(
                dryGrassTileTAO_.value,
                static_cast<unsigned>(std::size(dryGrassTileTAO_.value)),
                ranges_.gridWidth * ranges_.gridLength);
            calc::Mat4f mat = calc::Mat4f::identity();

            // Top field
            for (int i = cageMaxLength; i <= gridMaxLength; ++i) {
                for (int j = gridMinWidth; j <= gridMaxWidth; ++j) {
                    mat[3][0] = static_cast<float>(j);
                    mat[3][1] = static_cast<float>(i);
                    dryGrassTile_->push_back(mat.data());
                }
            }

            // Right field
            for (int i = cageMinLength; i <= cageMaxLength; ++i) {
                for (int j = gridMinWidth; j <= (cageMinWidth + 1); ++j) {
                    mat[3][0] = static_cast<float>(j);
                    mat[3][1] = static_cast<float>(i);
                    dryGrassTile_->push_back(mat.data());
                }
            }

            // Left field
            for (int i = cageMinLength; i <= cageMaxLength; ++i) {
                for (int j = cageMaxWidth - 1; j <= gridMaxWidth; ++j) {
                    mat[3][0] = static_cast<float>(j);
                    mat[3][1] = static_cast<float>(i);
                    dryGrassTile_->push_back(mat.data());
                }
            }

            // Bottom field
            for (int i = gridMinLength; i <= cageMinLength; ++i) {
                for (int j = gridMinWidth; j <= gridMaxWidth; ++j) {
                    mat[3][0] = static_cast<float>(j);
                    mat[3][1] = static_cast<float>(i);
                    dryGrassTile_->push_back(mat.data());
                }
            }
        }

        void load_fresh_grass(unsigned tao9)
        {
            const unsigned cageWidth = ranges_.cageWidth;
            const unsigned cageLength = ranges_.cageLength;

            const int cageMinWidth = ranges_.cageMinWidth;
            const int cageMaxWidth = ranges_.cageMaxWidth;

            const int cageMinLength = ranges_.cageMinLength;
            const int cageMaxLength = ranges_.cageMaxLength;

            unsigned grassTileTAO[] = {tao9, tao9};

            grassTile_ = std::make_unique<render::Square>(
                grassTileTAO, std::size(grassTileTAO), cageWidth * cageLength);

            const int wallThickness = 2;

            // Load fresh grass coordinates
            int xStart = cageMinLength + wallThickness;
            int xEnd = cageMaxLength - wallThickness;

            int yStart = cageMinWidth + wallThickness;
            int yEnd = cageMaxWidth - wallThickness;

            for (int i = xStart; i <= xEnd; ++i) {
                for (int j = yStart; j <= yEnd; ++j) {
                    const calc::Mat4f transposed = calc::transpose([i, j]() {
                        calc::Mat4f mat = calc::Mat4f::identity();
                        mat[0][3] = static_cast<float>(j);
                        mat[1][3] = static_cast<float>(i);
                        mat[2][3] = 0;
                        return mat;
                    }());
                    grassTile_->push_back(transposed.data());
                }
            }
        }

        /*! Helper
         *! Evt. handler
         */
        void on_window_resize()
        {
            int screenWidth = 0;
            int screenHeight = 0;
            SDL_GL_GetDrawableSize(window_, &screenWidth, &screenHeight);

            // Just in case
            screenWidth = std::max<int>(screenWidth, 1);
            screenHeight = std::max<int>(screenHeight, 1);

            glViewport(0, 0, screenWidth, screenHeight);

            // Update camera
            Camera& camera = *camera_;
            camera.resize(static_cast<unsigned>(screenWidth),
                          static_cast<unsigned>(screenHeight));
            camera.update();
        }

        // Points to main SDL window
        SDL_Window* window_ = nullptr;

        // Camera / viewer
        std::shared_ptr<Camera> camera_;
        // Contains box position and rotation information
        BoxData boxData_;

        // Program, uses instancing;
        // called to draw grid squares
        DrawInstancedNoTexture gridDraw_;
        // Program, uses instancing;
        // called to draw all textured objects
        DrawInstancedWithTexture mainDraw_;

        // Map item
        std::unique_ptr<render::Square> grassTile_;
        // Map item
        std::unique_ptr<render::Square> dryGrassTile_;
        // Map item
        std::unique_ptr<render::GridSquare> gridTile_;
        // Map item
        std::unique_ptr<render::Box> boxObject_[3];
        // Map item
        std::unique_ptr<render::Box> wallObject_;

        // Box skins
        std::vector<unsigned> boxSkins_;

        // All texture handles
        std::array<unsigned, 9> textureHandles_;

        // Dimension
        float cageWidth_ = 0;

        // Dimension
        float cageLength_ = 0;

        // Color of background
        calc::Vec4f backgroundColor_ = calc::Vec4f(0.0, 0.0, 0.0, 1.0);

        // Grid visibility
        bool gridEnabled_ = true;

        // Color of grid lines
        calc::Vec4f gridColor_ = calc::Vec4f(0.0, 0.0, 0.0, 1.0);

        struct WallTAO {
            unsigned value[2];
        } wallTAO_;

        struct DryGrassTileTAO {
            unsigned value[2];
        } dryGrassTileTAO_;

        struct Ranges {
            Ranges() = default;

            Ranges(unsigned width, unsigned height)
                : cageWidth(width + (width % 2))
                , cageLength(height + (height % 2))
                , gridWidth(2 * cageWidth)
                , gridLength(2 * cageLength)
                , gridMaxWidth(gridWidth / 2)
                , gridMinWidth(-gridMaxWidth)
                , gridMaxLength(gridLength / 2)
                , gridMinLength(-gridMaxLength)
                , cageMaxWidth(cageWidth / 2)
                , cageMinWidth(-cageMaxWidth)
                , cageMaxLength(cageLength / 2)
                , cageMinLength(-cageMaxLength)
            {}

            unsigned cageWidth = 0;
            unsigned cageLength = 0;

            unsigned gridWidth = 0;
            unsigned gridLength = 0;

            int gridMaxWidth = 0;
            int gridMinWidth = 0;

            int gridMaxLength = 0;
            int gridMinLength = 0;

            int cageMaxWidth = 0;
            int cageMinWidth = 0;

            int cageMaxLength = 0;
            int cageMinLength = 0;
        } ranges_;
    };
} // namespace
namespace {
    /*! Encapsulates world and run loop
     */
    std::shared_ptr<Runner> runner;
} // namespace

extern "C" {
    /*! Called by emscripten loop
     */
    void run()
    {
        runner->run();
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void set_box_skin(int value)
    {
        // Validate before setting value
        if (value <= 3 && value >= 1) {
            (runner->get_box()).selectedSkin = static_cast<unsigned>(value - 1);
        }
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void set_x_speed(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 20);
        value = std::max(value, 0);

        (runner->get_box()).speed[0] = static_cast<float>(value) / 100.0F;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_y_speed(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 20);
        value = std::max(value, 0);

        (runner->get_box()).speed[1] = static_cast<float>(value) / 100.0F;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_x_turn_rate(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 25);
        value = std::max(value, 0);

        (runner->get_box()).turnRate[0] = static_cast<float>(value) / 10.0F;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_y_turn_rate(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 25);
        value = std::max(value, 0);

        (runner->get_box()).turnRate[1] = static_cast<float>(value) / 10.0F;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_z_turn_rate(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 25);
        value = std::max(value, 0);

        (runner->get_box()).turnRate[2] = static_cast<float>(value) / 10.0F;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_pitch(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 45);
        value = std::max(value, 0);

        (runner->get_camera()).set_scene_pitch(static_cast<float>(value));
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_yaw(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 45);
        value = std::max(value, -45);

        (runner->get_camera()).set_scene_yaw(static_cast<float>(value));
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_roll(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 180);
        value = std::max(value, -180);

        (runner->get_camera()).set_scene_roll(static_cast<float>(value));
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_x_translation(int value)
    {
        // Just in case
        // Clamp value
        value = -value;
        value = std::min(value, 10);
        value = std::max(value, -10);

        (runner->get_camera()).set_x_translation(static_cast<float>(value));
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_y_translation(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 10);
        value = std::max(value, -10);

        (runner->get_camera()).set_y_translation(static_cast<float>(value));
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_z_translation(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 100);
        value = std::max(value, 10);

        (runner->get_camera())
            .set_z_translation(static_cast<float>(value * -1));
        (runner->get_camera()).update();
    }
}

namespace {

    inline bool parse_num(char ch, float* out)
    {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            *out = static_cast<float>(ch - '0');
            return true;
        }

        if (std::isalpha(static_cast<unsigned char>(ch))) {
            auto chl = static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch)));
            if ('a' <= chl && chl <= 'f') {
                *out = static_cast<float>(chl - 'a') + 10;
                return true;
            }
        }

        return false;
    }

    inline void parse_color(const char* str, calc::Vec4f& vec /* [out] */)
    {
        vec = calc::Vec4f(0, 0, 0, 1.0);

        if (std::strlen(str) != 7) {
            return;
        }

        // Validate str.
        if (str[0] != '#') {
            return;
        }

        for (int i = 0; i != 3; ++i) {
            char ch1 = str[(i * 2) + 1];
            char ch2 = str[(i * 2) + 2];

            // Validate
            float num1 = 0;
            float num2 = 0;

            if (!parse_num(ch1, &num1)) {
                break;
            }

            if (!parse_num(ch2, &num2)) {
                break;
            }

            vec[i] = ((num1 * 16) + num2) / 255.0F;
        }
    }
} // namespace

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void set_background_color(const char* str)
    {
        calc::Vec4f vec;
        parse_color(str, vec);
        runner->set_background_color(vec);
    }

    EMSCRIPTEN_KEEPALIVE
    void set_grid_color(const char* str)
    {
        calc::Vec4f vec;
        parse_color(str, vec);
        runner->set_grid_color(vec);
    }

    EMSCRIPTEN_KEEPALIVE
    void set_grid_state(bool state)
    {
        runner->enable_grid(state);
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void reset_speed()
    {
        (runner->get_box()).speed[0] = 0;
        (runner->get_box()).speed[1] = 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void reset_turn_rate()
    {
        (runner->get_box()).turnRate[0] = 0;
        (runner->get_box()).turnRate[1] = 0;
        (runner->get_box()).turnRate[2] = 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void reset_scene_translation()
    {
        (runner->get_camera()).set_x_translation(0);
        (runner->get_camera()).set_y_translation(0);
        (runner->get_camera()).set_z_translation(-80);
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void reset_scene_rotation()
    {
        (runner->get_camera()).set_scene_pitch(0);
        (runner->get_camera()).set_scene_yaw(0);
        (runner->get_camera()).set_scene_roll(0);
        (runner->get_camera()).update();
    }
}

namespace {

    /*! Parameters that are initialized by SDL
     */
    struct SDLParam {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
    };

    /*! Initializes SDL
     */
    bool init_sdl(SDLParam& params, unsigned widthHint, unsigned heightHint)
    {
        // Initialize sdl
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        // Create window with graphics context
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        return SDL_CreateWindowAndRenderer(static_cast<int>(widthHint),
                                           static_cast<int>(heightHint),
                                           SDL_WINDOW_OPENGL,
                                           &params.window,
                                           &params.renderer)
               == 0;
    }
} // namespace

/*! Entry point
 */
int main()
{
    unsigned screenWidth = 1000;
    unsigned screenHeight = 1000;

    // Init SDL and OpenGL
    SDLParam params;
    if (!init_sdl(params, screenWidth, screenHeight)) {
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glViewport(
        0, 0, static_cast<int>(screenWidth), static_cast<int>(screenHeight));

    // Init camera defaults
    static float xPos = 0.0F;
    static float yPos = 0.0F;
    static float zPos = -20.0F;
    static float fov = 30.0F;
    static float zFar = 1000.0F;

    try {
        // Init camera
        auto camera = std::make_shared<Camera>(calc::Vec3f(xPos, yPos, zPos),
                                               static_cast<float>(screenWidth),
                                               static_cast<float>(screenHeight),
                                               fov,
                                               zFar);
        camera->resize(screenWidth, screenHeight);
        camera->set_scene_rotation(0, 0, -80);
        camera->update();

        runner = std::make_unique<Runner>(params.window, camera);

        // Init defaults...
        runner->set_grid_color(calc::Vec4f(0.75F, 0.75F, 0.75F, 1.0F));
        runner->enable_grid(true);
        runner->set_background_color(calc::Vec4f(0.68F, 0.58F, 0.10F, 1.0F));
    }

    catch (Program::ProgramBuildException&) {
        printf("Error: Make sure that your implementation of OpenGL supports "
               "version 3.3 or above\n");
        runner.reset();
        return 1;
    }

    catch (Program::ShaderBuildException&) {
        printf("Error: Make sure that your implementation of OpenGL supports "
               "version 3.3 or above\n");
        runner.reset();
        return 1;
    }

    catch (std::runtime_error& e) {
        printf("%s\n", e.what());
        runner.reset();
        return 1;
    }

    // Init main run loop
    emscripten_set_main_loop(run, 0, true);
    return 0;
}
