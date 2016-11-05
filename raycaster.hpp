/**
 *	Raycaster engine for the Gosu game library
 */
#include <Gosu/Gosu.hpp>

namespace Gosu {
    class RayCaster {
    public:
        // Data provided to draw call, so that the sprites can display in the renderer
        struct Sprite {
            Gosu::Image * texture = NULL;
            double x = 0;
            double y = 0;
        };
        
        // Data supplied to the raycaster so it knows what it is looking at
        struct MapData {
            bool invalid = false; // Out of bounds; stops raycast
            
            // Textures to display
            Gosu::Image * wall = NULL;
            Gosu::Bitmap * floor = NULL;
            Gosu::Bitmap * ceiling = NULL;
            
            bool x_hidden = false;		// The x sides of the block are not drawn
            bool y_hidden = false;		// The y sides of the block are not drawn
            
            // The following settings are only for wall sprites, which are walls that can be transparent
            // or visually (but not actually) shifted by fractions of a wall block length.
            bool wall_sprite = false;
            float inset_amount = 0.0;	// Appears to shift the wall away from the camera by this amount
            float texture_offset = 0.0;	// Appears to shift this block to the left or right
        };
        
        RayCaster();
        
        // Debugging assistant
        void setDisplayFPS(const bool enable);
        
        // Place the camera at a specific position in the world
        void setCameraPosition(const double x, const double y);
        void setCameraPosition(const std::pair<double, double>& xy);
        
        const std::pair<double, double> getCameraPosition();
        
        // Rotate the camera to a new facing. This method is additive.
        void rotateCamera(const double degrees);
        
        // Retrieve the camera rotation by degrees
        const double getCameraRotation();
        
        // Retrieves the true camera coordinate system, based on rotation. For example, if it is 1,0,
        // that means you are facing perfectly in the x direction.
        const std::pair<double, double> getCoordinateSystem();
        
        // Directly change the coordinate system as it is described in getCoordinateSystem
        void setCoordinateSystem(const double x, const double y);
        void setCoordinateSystem(const std::pair<double, double>& xy);
        
        // Adds a forward and strafing motion by some amount.
        //
        // The 'query' callback is for collisions; it will be called with your new position after applying
        // this transform. If you return a 'true' there, the transform is cancelled.
        void transformCamera(const double forward, const double strafe, const std::function <bool(double, double)>& query);
        
        // This is the amount the camera bobs up and down, as a percentage of screen 0.0-1.0. This is advanced
        // by the bobCamera function.
        // Note: amount is clamped to -0.5 - 0.5 to prevent draws outside the screen.
        void setCameraBobRange(const double amount);
        
        const double getCameraBobRange();
        
        // Apply some delta-influenced amount which animates within the bobbing range, if there is any.
        void bobCamera(const double amount);
        
        // Pitch the camera by a certain amount as a percentage of screen 0.0-1.0. This stacks with camera bobbing.
        // Note: amount is clamped to -0.5 - 0.5 to prevent draws outside the screen.
        void pitchCamera(const double amount);
        
        // Direct setting for camera pitch. See pitchCamera, which is additive.
        void setCameraPitch(const double amount);
        
        const double getCameraPitch();
        
        // This is the heavy lifter. Call from the draw method of a Gosu::Window to render your world.
        //
        // win - link back to your window
        // query - callback to let the renderer see what your map looks like without actually maintaining it.
        //         Each call to it will be asking your code what is at an x, y location in the form of a MapData
        //         structure.
        // sprites - ALL drawable sprites. Off-screen sprites won't render, so don't worry about which to supply.
        void draw(Window * win, const std::function <MapData(int, int)>& query, const std::vector<Sprite>& sprites);
    };
};
