#include "ball_data.hpp"
#include "box.hpp"
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
     *! Converts matrix to float data
     */
    inline std::vector<float> copy_matrix_data(
        const std::vector<calc::mat4f>& mats)
    {
        std::vector<float> values(mats.size() * 16);
        for (::size_t i = 0; i != mats.size(); ++i)
            ::memcpy(&values[i * 16], calc::data(mats[i]), sizeof(calc::mat4f));
        return values;
    }
} // namespace

namespace {

    /*! Helper
     *! Builds the vertices for the map grid
     */
    std::vector<calc::mat4f> build_grid(int width, int length)
    {
        static const float xdim = 1.0;
        static const float ydim = 1.0;

        calc::mat4f model = calc::mat4f::identity();
        float& x = model[3][0];
        float& y = model[3][1];

        const int xinst = std::ceil(width / 2.0 / xdim);
        const int yinst = std::ceil(length / 2.0 / ydim);

        const int size = xinst * yinst * 4;

        std::vector<calc::mat4f> grid;
        grid.resize(size);
        calc::mat4f* ptr = &grid[0];

        for (int i = -xinst; i != xinst; ++i) {
            for (int j = -yinst; j != yinst; ++j) {
                x = i * xdim + 0.5;
                y = j * ydim + 0.5;
                *ptr++ = model;
            }
        }

        return grid;
    }

    /*! Helper
     *! Loads grid vertices and the grid render target
     */
    std::shared_ptr<render::GridSquare> load_grid(unsigned gridWidth,
                                                  unsigned gridLength)
    {
        // Load grid tiles
        const std::vector<float> gridCoords
            = copy_matrix_data(build_grid(gridWidth, gridLength));

        unsigned size = gridCoords.size() / 16;

        std::shared_ptr<render::GridSquare> tile
            = std::make_shared<render::GridSquare>(gridWidth * gridLength);
        tile->reset(gridCoords.data(), size);
        return tile;
    }

    /*! Helper
     *! Build the vertices for the map wall
     */
    std::vector<calc::mat4f> build_wall(int width, int length)
    {
        std::vector<calc::mat4f> wall;

        // West wall
        for (int i = 1 - length / 2 / 3; i != length / 2 / 3; ++i) {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = width / 2 - 1;
            mat[1][3] = i * 3;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // East wall
        for (int i = 1 - length / 2 / 3; i != length / 2 / 3; ++i) {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = 1 - width / 2;
            mat[1][3] = i * 3;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // North wall
        for (int i = 1 - width / 2 / 3; i != width / 2 / 3; ++i) {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = i * 3;
            mat[1][3] = length / 2 - 1;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // South wall
        for (int i = 1 - width / 2 / 3; i != width / 2 / 3; ++i) {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = i * 3;
            mat[1][3] = 1 - length / 2;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        return wall;
    }

    /*! Helper
     *! Loads wall vertices and the wall render target
     */
    std::shared_ptr<render::Box> load_wall(unsigned cageWidth,
                                           unsigned cageLength,
                                           unsigned* wallTAO,
                                           unsigned wallTAOCount)
    {
        const std::vector<float> wallCoords
            = copy_matrix_data(build_wall(cageWidth, cageLength));

        unsigned size = wallCoords.size() / 16;

        std::shared_ptr<render::Box> object = std::make_shared<render::Box>(
            wallTAO, wallTAOCount, cageWidth * cageLength);
        object->reset(wallCoords.data(), size);
        return object;
    }
} // namespace

namespace {
    /*! Helper
     *! Loads dry grass coordinates and the grass render target
     */
    inline std::shared_ptr<render::Square> load_dry_grass(int gridWidth,
                                                          int gridLength,
                                                          int cageWidth,
                                                          int cageLength)
    {
        // Load dry grass textures and shape
        unsigned textureTAO = render::load_texture_from_data(
            dry_grass_png, dry_grass_png_len, false);
        unsigned tileTAO[] = {textureTAO, textureTAO};

        unsigned TAOCount = sizeof(tileTAO) / sizeof(unsigned);
        unsigned size = gridWidth * gridLength;

        std::shared_ptr<render::Square> tile
            = std::make_shared<render::Square>(tileTAO, TAOCount, size);

        // Calculate minimum and maximum coordinates
        int gridMaxLength = gridLength / 2;
        int gridMinLength = -gridMaxLength;

        int gridMaxWidth = gridWidth / 2;
        int gridMinWidth = -gridMaxWidth;

        int cageMaxLength = cageLength / 2;
        int cageMinLength = -cageMaxLength;

        int cageMaxWidth = cageWidth / 2;
        int cageMinWidth = -cageMaxWidth;

        // Load dry grass coordinates...
        calc::mat4f mat = calc::mat4f::identity();

        // Top field
        for (int i = cageMaxLength; i <= gridMaxLength; ++i) {
            for (int j = gridMinWidth; j <= gridMaxWidth; ++j) {
                mat[3][0] = j;
                mat[3][1] = i;
                tile->push_back(mat);
            }
        }

        // Right field
        for (int i = cageMinLength; i <= cageMaxLength; ++i) {
            for (int j = gridMinWidth; j <= cageMinWidth + 1; ++j) {
                mat[3][0] = j;
                mat[3][1] = i;
                tile->push_back(mat);
            }
        }

        // Left field
        for (int i = cageMinLength; i <= cageMaxLength; ++i) {
            for (int j = cageMaxWidth - 1; j <= gridMaxWidth; ++j) {
                mat[3][0] = j;
                mat[3][1] = i;
                tile->push_back(mat);
            }
        }

        // Bottom field
        for (int i = gridMinLength; i <= cageMinLength; ++i) {
            for (int j = gridMinWidth; j <= gridMaxWidth; ++j) {
                mat[3][0] = j;
                mat[3][1] = i;
                tile->push_back(mat);
            }
        }

        return tile;
    }

    /*! Helper
     *! Loads fresh grass coordinates and the grass render target
     */
    std::shared_ptr<render::Square> load_fresh_grass(int cageWidth,
                                                     int cageLength)
    {
        // Load fresh grass tiles...
        unsigned textureTAO = render::load_texture_from_data(
            dark_grass_png, dark_grass_png_len, false);
        unsigned tileTAO[] = {textureTAO, textureTAO};

        unsigned TAOCount = sizeof(tileTAO) / sizeof(unsigned);
        unsigned size = cageWidth * cageLength;

        std::shared_ptr<render::Square> tile
            = std::make_shared<render::Square>(tileTAO, TAOCount, size);

        int cageMaxLength = cageLength / 2;
        int cageMinLength = -cageMaxLength;

        int cageMaxWidth = cageWidth / 2;
        int cageMinWidth = -cageMaxWidth;

        int wallThickness = 2;

        // Load fresh grass coordinates
        for (int i = cageMinLength + wallThickness;
             i <= cageMaxLength - wallThickness;
             ++i) {
            for (int j = cageMinWidth + wallThickness;
                 j <= cageMaxWidth - wallThickness;
                 ++j) {
                tile->push_back(calc::transpose([i, j]() {
                    calc::mat4f mat = calc::mat4f::identity();
                    mat[0][3] = j;
                    mat[1][3] = i;
                    mat[2][3] = 0;
                    return mat;
                }()));
            }
        }

        return tile;
    }
} // namespace

namespace {

    /*! Class Runner
     *! Encapsulates the main loop
     */
    class Runner {
    public:
        /*! ctor.
         */
        Runner(SDL_Window* window, int screenWidth, int screenHeight);

        /*! Run loop
         */
        void run();

        /*! @set
         */
        void enable_grid(bool state)
        {
            gridEnabled_ = state;
        }

        bool get_grid_state() const
        {
            return gridEnabled_;
        }

        void set_background_color(const calc::vec4f& vec)
        {
            backgroundColor_ = vec;
        }

        void get_background_color(const calc::vec4f& vec)
        {
            backgroundColor_ = vec;
        }

        void set_grid_color(const calc::vec4f& vec)
        {
            gridColor_ = vec;
        }

        const calc::vec4f& get_grid_color() const
        {
            return gridColor_;
        }

        BallData& get_ball()
        {
            return ballData_;
        }

        const BallData& get_ball() const
        {
            return ballData_;
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
        /*! Helper
         *! Evt. handler
         */
        void on_window_event(const SDL_Event& e);

        /*! Helper
         *! Evt. handler
         */
        void on_text_input(const SDL_Event&)
        {}

        /*! Helper
         *! Renders the scene
         */
        void render();

        // Points to main SDL window
        SDL_Window* window_;

        // Camera / viewer
        std::shared_ptr<Camera> camera_;
        // Contains ball position and rotation information
        BallData ballData_;

        // Program, uses instancing;
        // called to draw grid squares
        DrawInstancedNoTexture gridDraw_;
        // Program, uses instancing;
        // called to draw all textured objects
        DrawInstancedWithTexture mainDraw_;

        // Map item
        std::shared_ptr<render::Square> grassTile_;
        // Map item
        std::shared_ptr<render::Square> dryGrassTile_;
        // Map item
        std::shared_ptr<render::GridSquare> gridTile_;
        // Map item
        std::shared_ptr<render::Box> ballObject_[3];
        // Map item
        std::shared_ptr<render::Box> wallObject_;

        // Box skins
        std::vector<unsigned> textureHandles_;

        // Dimension
        float cageWidth_;
        // Dimension
        float cageLength_;

        // Color of background
        calc::vec4f backgroundColor_;
        // Grid status
        bool gridEnabled_;
        // Color of grid lines
        calc::vec4f gridColor_;
    };

    /*! ctor.
     */
    Runner::Runner(SDL_Window* window, int screenWidth, int screenHeight)
        : window_(window)
        , backgroundColor_(0.0, 0.0, 0.0, 1.0)
        , gridEnabled_(true)
        , gridColor_(0.0, 0.0, 0.0, 1.0)
    {
        // Init camera defaults
        static float xPos = 0;
        static float yPos = 0;
        static float zPos = 0;
        static float fov = 12.5;
        static float zFar = 1000.0;
        static float zNear = 0.01;

        // Init. Camera
        camera_ = std::make_shared<Camera>(calc::vec3f(xPos, yPos, zPos),
                                           fov,
                                           zFar,
                                           zNear,
                                           screenWidth,
                                           screenHeight);

        // Load boxes
        unsigned boxTAO1[]
            = {render::load_texture_from_data(
                   brick_wall_png, brick_wall_png_len, false),
               render::load_texture_from_data(
                   awesome_face_png, awesome_face_png_len, true)};

        unsigned boxTAO2[]
            = {boxTAO1[0],
               render::load_texture_from_data(
                   shocked_face_png, shocked_face_png_len, true)};

        unsigned boxTAO3[]
            = {boxTAO1[0],
               render::load_texture_from_data(
                   incredulous_face_png, incredulous_face_png_len, true)};

        unsigned wallTAO[] = {
            boxTAO1[0],
            boxTAO1[0],
        };

        textureHandles_.push_back(render::load_texture_from_data(
            awesome_face_png, awesome_face_png_len, true, false));
        textureHandles_.push_back(render::load_texture_from_data(
            shocked_face_png, shocked_face_png_len, true, false));
        textureHandles_.push_back(render::load_texture_from_data(
            incredulous_face_png, incredulous_face_png_len, true, false));

        ballObject_[0] = std::make_shared<render::Box>(
            boxTAO1, (sizeof(boxTAO1) / sizeof(unsigned)), 1);
        ballObject_[0]->push_back(calc::mat4f::identity());

        ballObject_[1] = std::make_shared<render::Box>(
            boxTAO2, (sizeof(boxTAO2) / sizeof(unsigned)), 1);
        ballObject_[1]->push_back(calc::mat4f::identity());

        ballObject_[2] = std::make_shared<render::Box>(
            boxTAO3, (sizeof(boxTAO3) / sizeof(unsigned)), 1);
        ballObject_[2]->push_back(calc::mat4f::identity());

        // Load map...
        static float cageWidth = 30;
        cageWidth_ = cageWidth;

        static float cageLength = 30;
        cageLength_ = cageLength;

        float gridWidth = 2 * cageWidth;
        float gridLength = 2 * cageLength;

        // Load wall map objects
        wallObject_ = load_wall(
            cageWidth, cageLength, wallTAO, sizeof(wallTAO) / sizeof(unsigned));
        // Load grid tiles
        gridTile_ = load_grid(gridWidth, gridLength);
        // Load fresh grass tiles (inside-cage tiles)
        grassTile_ = load_fresh_grass(cageWidth, cageLength);
        // Load dry grass tiles (outside-cage tiles)
        dryGrassTile_
            = load_dry_grass(gridWidth, gridLength, cageWidth, cageLength);
    }

    /*! Run loop
     */
    void Runner::run()
    {
        // Handle events on queue
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                return;
            }

            switch (e.type) {
                case SDL_WINDOWEVENT:
                {
                    on_window_event(e);
                    break;
                }

                // Handle keypress with current mouse position
                case SDL_TEXTINPUT:
                {
                    on_text_input(e);
                    break;
                }
            }
        }

        render();
    }

    /*! Helper
     *! Evt. handler
     */
    void Runner::on_window_event(const SDL_Event& e)
    {
        if ((e.window).event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            unsigned screenWidth = e.window.data1;
            unsigned screenHeight = e.window.data2;
            glViewport(0, 0, screenWidth, screenHeight);

            // Update camera
            Camera& refcamera = *camera_;
            refcamera.resize(screenWidth, screenHeight);
            refcamera.update();
        }
    }

    /*! Helper
     *! Renders the scene
     */
    void Runner::render()
    {
        // Clear it
        glClearColor(
            backgroundColor_[0], backgroundColor_[1], backgroundColor_[2], 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const calc::mat4f& lookAt = camera_->get_device_look_at();
        const calc::mat4f& projection = camera_->get_device_projection();
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
        calc::vec3f& direction = ballData_.direction;
        calc::vec3f& speed = ballData_.speed;
        calc::mat4f& translation = ballData_.translation;

        translation[2][3] = -1.0;
        float x = (translation[0][3] += speed[0] * direction[0]);
        float y = (translation[1][3] += speed[1] * direction[1]);

        // Bounce back on wall hit
        float hitOffset = 3.0;
        if (x < +hitOffset - (cageWidth_ / 2)
            || x > -hitOffset + (cageWidth_ / 2)) {
            direction[0] *= -1;
        }

        if (y < +hitOffset - (cageLength_ / 2)
            || y > -hitOffset + (cageLength_ / 2)) {
            direction[1] *= -1;
        }

        const calc::vec3f turnRate
            = ballData_.turnRate * calc::radians(SDL_GetTicks() / 10.0);
        const calc::mat4f boxMat = calc::transpose(
            translation * calc::rotate_4x(turnRate[0])
            * calc::rotate_4y(turnRate[1]) * calc::rotate_4z(turnRate[2]));
        // Do the draw call
        render::Box& refobject = *ballObject_[ballData_.selectedSkin];
        refobject.modify(calc::data(boxMat), 0);
        refobject.draw();
        // Update screen & return
        SDL_GL_SwapWindow(window_);
    }
} // namespace

namespace {
    /*! Encapsulates world and run loop
     */
    std::shared_ptr<Runner> runner;

    /*! Called by emscripten loop
     */
    void run()
    {
        runner->run();
    }
} // namespace

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void set_box_skin(int value)
    {
        // Validate before setting value
        if (value <= 3 && value >= 1) {
            (runner->get_ball()).selectedSkin = value - 1;
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

        (runner->get_ball()).speed[0] = (float)value / 100.0;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_y_speed(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 20);
        value = std::max(value, 0);

        (runner->get_ball()).speed[1] = (float)value / 100.0;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_x_turn_rate(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 25);
        value = std::max(value, 0);

        (runner->get_ball()).turnRate[0] = (float)value / 10.0;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_y_turn_rate(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 25);
        value = std::max(value, 0);

        (runner->get_ball()).turnRate[1] = (float)value / 10.0;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_z_turn_rate(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 25);
        value = std::max(value, 0);

        (runner->get_ball()).turnRate[2] = (float)value / 10.0;
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_pitch(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 45);
        value = std::max(value, 0);

        (runner->get_camera()).set_scene_pitch(value);
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_yaw(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 45);
        value = std::max(value, -45);

        (runner->get_camera()).set_scene_yaw(value);
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_roll(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 180);
        value = std::max(value, -180);

        (runner->get_camera()).set_scene_roll(value);
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

        (runner->get_camera()).set_x_position(value);
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_y_translation(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 10);
        value = std::max(value, -10);

        (runner->get_camera()).set_y_position(value);
        (runner->get_camera()).update();
    }

    EMSCRIPTEN_KEEPALIVE
    void set_scene_z_translation(int value)
    {
        // Just in case
        // Clamp value
        value = std::min(value, 100);
        value = std::max(value, 10);

        (runner->get_camera()).set_z_position(value * -1);
        (runner->get_camera()).update();
    }
}

namespace {

    inline int parse_num(char ch)
    {
        if (::isalpha(ch))
            return ::tolower(ch) - 'a';
        else
            return ch - '0';
    }

    inline void parse_color(const char* str, calc::vec4f& vec /* [out] */)
    {
        vec = calc::vec4f(0, 0, 0, 1.0);

        if (::strlen(str) != 7) {
            return;
        }

        // Validate str.
        if (str[0] != '#') {
            return;
        }

        unsigned i = 0;
        for (; i != 3; ++i) {
            char ch1 = str[i * 2 + 1];
            char ch2 = str[i * 2 + 2];

            // Validate
            if (!::isalnum(ch1) || !::isalnum(ch2))
                break;
            vec[i] = (float)((parse_num(ch1)) * 16 + parse_num(ch2)) / 256.0;
        }
    }
} // namespace

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void set_background_color(const char* str)
    {
        calc::vec4f vec;
        parse_color(str, vec);
        runner->set_background_color(vec);
    }

    EMSCRIPTEN_KEEPALIVE
    void set_grid_color(const char* str)
    {
        calc::vec4f vec;
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
        (runner->get_ball()).speed[0] = 0;
        (runner->get_ball()).speed[1] = 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void reset_turn_rate()
    {
        (runner->get_ball()).turnRate[0] = 0;
        (runner->get_ball()).turnRate[1] = 0;
        (runner->get_ball()).turnRate[2] = 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void reset_scene_translation()
    {
        (runner->get_camera()).set_x_position(0);
        (runner->get_camera()).set_y_position(0);
        (runner->get_camera()).set_z_position(-80);
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

/*! Entry point
 */
int main(void)
{
    int screenWidth = 1000;
    int screenHeight = 1000;

    // Init SDL and OpenGL
    SDL_Window* window;
    SDL_Renderer* renderer = nullptr;
    SDL_CreateWindowAndRenderer(
        screenWidth, screenHeight, SDL_WINDOW_OPENGL, &window, &renderer);
    if (window == nullptr)
        return (printf("SDL window could not be created! SDL Error: %s\n",
                       SDL_GetError()),
                1);

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, screenWidth, screenHeight);

    // Enter run loop
    runner = std::make_shared<Runner>(window, screenWidth, screenHeight);

    // Init defaults...
    (runner->get_camera()).set_z_position(-80);
    (runner->get_camera()).update();

    runner->set_grid_color(calc::vec4f(0.75, 0.75, 0.75, 1.0));
    runner->enable_grid(true);
    runner->set_background_color(calc::vec4f(0.63, 0.58, 0.10, 1.0));

    // Init main run loop
    emscripten_set_main_loop(run, 0, 1);
    return 0;
}
