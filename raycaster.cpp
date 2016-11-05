#include "raycaster.hpp"

#include <math.h>
#include <stdlib.h>

enum DrawPass {
    FIRST_PASS = 0,
    WALL_PASS = 0,      // Draws non-transparent walls
    WALL_SPRITE_PASS,   // Draws walls with offsets or transparencies
    N_PASSES
};

// --- Hidden private data members - i didnt feel like doing pimpl ---
bool _ready;						// Ready to render!
bool _fps_enabled;					// Draw FPS on rendering

// Position and direction of camera
double _pos_x;
double _pos_y;
double _dir_x;
double _dir_y;
double _plane_x;
double _plane_y;
double _rotation; // _dir_x and y as a single angle in degrees

// Pitch and bob of camera
double _camera_pitch;
double _camera_bob_current;
double _camera_bob_range;
int _camera_bob_direction;

Gosu::Bitmap _ceiling_floor;

// ----

Gosu::RayCaster::RayCaster() {
    _ready = false;
    _camera_pitch = 0.0;
    _camera_bob_current = 0.0;
    _camera_bob_range = 0.0;
    _camera_bob_direction = 1;
    _plane_x = 0.66;
    _plane_y = 0.00;
    _dir_x = 0;
    _dir_y = -1;
    _rotation = 0;
    _fps_enabled = false;
}

void Gosu::RayCaster::setDisplayFPS(const bool enable) {
    _fps_enabled = enable;
}

void Gosu::RayCaster::setCameraPosition(const double x, const double y) {
    _ready = true;
    
    _pos_x = x;
    _pos_y = y;
}

void Gosu::RayCaster::setCameraPosition(const std::pair<double, double>& xy) {
    setCameraPosition(xy.first, xy.second);
}

const std::pair<double, double> Gosu::RayCaster::getCameraPosition() {
    return std::make_pair(_pos_x, _pos_y);
};

void Gosu::RayCaster::rotateCamera(const double degrees) {
    static double PI_HALF = M_PI / 180;
    double amount = degrees * PI_HALF;
    double old = _dir_x;
    _dir_x = _dir_x * cos(amount) - _dir_y * sin(amount);
    _dir_y = old * sin(amount) + _dir_y * cos(amount);
    
    _plane_x = _dir_y * -0.66;
    _plane_y = _dir_x * 0.66;
    
    _rotation += degrees;
    if(_rotation > 360.0) {
        _rotation -= 360.0;
    }
    if(_rotation < 0.0) {
        _rotation += 360.0;
    }
}

const double Gosu::RayCaster::getCameraRotation() {
    return _rotation;
}

const std::pair<double, double> Gosu::RayCaster::getCoordinateSystem() {
    return std::make_pair(_dir_x, _dir_y);
}

void Gosu::RayCaster::setCoordinateSystem(const double x, const double y) {
    _dir_x = x;
    _dir_y = y;
    
    _rotation = atan2(_dir_x, _dir_y) * (180/M_PI);
    _plane_x = y * -0.66;
    _plane_y = x * 0.66;
}

void Gosu::RayCaster::setCoordinateSystem(const std::pair<double, double>& xy) {
    setCoordinateSystem(xy.first, xy.second);
}

void Gosu::RayCaster::transformCamera(const double forward, const double strafe, const std::function <bool(double, double)>& query) {
    std::pair<double, double> old_position = getCameraPosition();
    
    _pos_x += _dir_x * forward;
    _pos_y += _dir_y * forward;
    
    _pos_x += _plane_x * strafe;
    _pos_y += _plane_y * strafe;
    
    // On collision, allow wall sliding by trying a combination of new and old x and y positions
    if(query(_pos_x, _pos_y)) {
        if(query(_pos_x, old_position.second) == false) {
            _pos_y = old_position.second;
        } else if(query(old_position.first, _pos_y) == false) {
            _pos_x = old_position.first;
        } else {
            _pos_x = old_position.first;
            _pos_y = old_position.second;
        }
    }
}

void Gosu::RayCaster::setCameraBobRange(const double amount) {
    _camera_bob_range = amount;
    _camera_bob_range = Gosu::clamp<double>(_camera_bob_range, -0.5, 0.5);
}

const double Gosu::RayCaster::getCameraBobRange() {
    return _camera_bob_range;
}

void Gosu::RayCaster::bobCamera(const double amount) {
    // Make sure to do no bobbing when rested
    if(_camera_bob_range == 0 && _camera_bob_current == 0) {
        return;
    } else {
        _camera_bob_current = _camera_bob_current + (amount * _camera_bob_direction);
        
        if(_camera_bob_direction == 1 && _camera_bob_current > _camera_bob_range) {
            _camera_bob_direction = -1;
            if(_camera_bob_current - _camera_bob_range < 0.1) {
                _camera_bob_current = _camera_bob_range;
            }
        } else if(_camera_bob_direction == -1 && _camera_bob_current < -_camera_bob_range) {
            _camera_bob_direction = 1;
            
            if(_camera_bob_range - _camera_bob_current < 0.1) {
                _camera_bob_current = -_camera_bob_range;
            }
        }
    }
}

void Gosu::RayCaster::pitchCamera(const double amount) {
    setCameraPitch(_camera_pitch + amount);
}

void Gosu::RayCaster::setCameraPitch(const double amount) {
    _camera_pitch = amount;
    _camera_pitch = Gosu::clamp<double>(_camera_pitch, -0.5, 0.5);
}

const double Gosu::RayCaster::getCameraPitch() {
    return _camera_pitch;
}

void Gosu::RayCaster::draw(Window * win, const std::function <MapData(int, int)>& query, const std::vector<Sprite>& sprites) {
    if(_ready) {
        float z = -100;
        
        // This is the data gathered during passes, to prevent unneccessary re-calculations
        struct PassData {
            double camera_x;
            double ray_dir_x;
            double ray_dir_y;
            double delta_x;
            double delta_y;
            double wall_distance;
        };
        
        // Prepare the ceiling/floor background image
        unsigned screen_w = win->graphics().width();
        unsigned screen_h = win->graphics().height();
        _ceiling_floor.resize(screen_w, screen_h);
        
        // Make sure the combined tilt and bob don't exceed draw area
        double camera_pitch_clamped = Gosu::clamp<double>(_camera_pitch + _camera_bob_current, -0.5, 0.5);
        int camera_pitch = screen_h * camera_pitch_clamped;
        
        // We need data for every x value across the resolution
        PassData pass_data[screen_w];
        
        // Begin the passes - each vertical slice of the screen is handled. Ergo, resolution = computation required
        for(int pass = FIRST_PASS; pass < N_PASSES; pass++) {
            for(int x = 0; x < screen_w; x++) {
                // Very first pass collects the pass information so nobody else has to worry about it
                if(pass == FIRST_PASS) {
                    PassData pd;
                    pd.camera_x = 2.0f * x / (float)screen_w - 1.0f;;
                    pd.ray_dir_x = _dir_x + _plane_x * pd.camera_x;
                    pd.ray_dir_y = _dir_y + _plane_y * pd.camera_x;
                    pd.delta_x = sqrt(1 + (pd.ray_dir_y * pd.ray_dir_y) / (pd.ray_dir_x * pd.ray_dir_x));
                    pd.delta_y = sqrt(1 + (pd.ray_dir_x * pd.ray_dir_x) / (pd.ray_dir_y * pd.ray_dir_y));
                    pass_data[x] = pd;
                }
                
                // Begin the cast from player's position on the map
                int cur_x = _pos_x;
                int cur_y = _pos_y;
                
                // Find the step values, which determine how we change coordinates each step in the cast
                int step_x = pass_data[x].ray_dir_x < 0 ? -1 : 1;
                int step_y = pass_data[x].ray_dir_y < 0 ? -1 : 1;
                
                // find initial side dist - i am still not clear on this part of the algorithm. Explanation would be nice.
                double side_dist_x, side_dist_y;
                if(pass_data[x].ray_dir_x < 0) {
                    side_dist_x = (_pos_x - cur_x) * pass_data[x].delta_x;
                } else {
                    side_dist_x = (cur_x + 1.0 - _pos_x) * pass_data[x].delta_x;
                }
                if(pass_data[x].ray_dir_y < 0) {
                    side_dist_y = (_pos_y - cur_y) * pass_data[x].delta_y;
                } else {
                    side_dist_y = (cur_y + 1.0 - _pos_y) * pass_data[x].delta_y;
                }
                
                // Execute raycast
                int side = 0;
                bool casting = true;
                bool last_cast = false;
                int skip_cast = 0;
                while(casting) {
                    // Advance the ray
                    if(side_dist_x < side_dist_y) {
                        side_dist_x += pass_data[x].delta_x;
                        cur_x += step_x;
                        side = 0;
                    } else {
                        side_dist_y += pass_data[x].delta_y;
                        cur_y += step_y;
                        side = 1;
                    }
                    
                    if(skip_cast > 0) {
                        skip_cast --;
                        continue;
                    }
                    // See what we got
                    MapData response = query(cur_x, cur_y);
                    if(response.invalid) {
                        casting = false;
                    }
                    // Don't draw hidden 'sides' or spaces with no wall... don't worry, floors are handled by the distant walls once they're reached.
                    else if(response.wall && !(side == 0 && response.x_hidden) && !(side == 1 && response.y_hidden) ){
                        // One last thing to check... make sure we're on the right pass for this wall! Was it a wall sprite?
                        bool correct_pass = true;
                        if(pass == WALL_PASS && response.wall_sprite) {
                            correct_pass = false;
                        } else if(pass == WALL_SPRITE_PASS && response.wall_sprite == false) {
                            correct_pass = false;
                        }
                        
                        if(correct_pass) {
                            // Wall pass cancels raycasting, while sprites allow an inset to be applied.
                            double y_inset = 0;
                            double x_inset = 0;
                            if(pass == WALL_PASS) {
                                casting = false;
                            } else if(pass == WALL_SPRITE_PASS) {
                                if(side == 1) {
                                    y_inset = response.inset_amount * (pass_data[x].ray_dir_y > 0 ? 1:-1);
                                } else {
                                    x_inset = response.inset_amount * (pass_data[x].ray_dir_x > 0 ? 1:-1);
                                }
                            }
                            
                            // Get the distance and the height of the wall slice from that. Inset is factored, so really the inset
                            // block ISNT inset, it's just an illusion caused by adding extra distance. But it works!
                            double wall_dist;
                            if(side == 0) {
                                wall_dist = ((cur_x + x_inset) - _pos_x + (1 - step_x) / 2) / pass_data[x].ray_dir_x;
                            } else {
                                wall_dist = ((cur_y + y_inset) - _pos_y + (1 - step_y) / 2) / pass_data[x].ray_dir_y;
                            }
                            double line_height = wall_dist == 0 ? 0 : screen_h / wall_dist;
                            
                            // Only solid walls get copied to the distance buffer
                            if(pass == WALL_PASS) {
                                pass_data[x].wall_distance = wall_dist;
                            }
                            
                            // This will always pass for walls, but not always for wall sprites. This way walls can cover up wall sprites.
                            if(wall_dist <= pass_data[x].wall_distance && line_height > 1) {
                                // Determine the x of the wall that was hit
                                double wall_x;
                                if (side == 0) {
                                    wall_x = _pos_y + wall_dist * pass_data[x].ray_dir_y;
                                } else {
                                    wall_x = _pos_x + wall_dist * pass_data[x].ray_dir_x;
                                }
                                wall_x -= floor(wall_x);
                                
                                // Wall sprites can have a texture offset to simulate sliding left and right
                                if(pass == WALL_SPRITE_PASS) {
                                    wall_x -= response.texture_offset;
                                }
                                
                                // From wall_x, we can get the slice of the texture to render
                                int texX = (int)(response.wall->width() * wall_x);
                                if(side == 0 && pass_data[x].ray_dir_x > 0) texX = response.wall->width() - texX - 1;
                                if(side == 1 && pass_data[x].ray_dir_y < 0) texX = response.wall->height() - texX - 1;
                                
                                // Prevent out of bounds lines from trying to draw
                                if(texX == 0) {
                                    texX++;
                                } else if(texX == response.wall->width() - 1) {
                                    texX--;
                                }
                                
                                // Calculate the four points of the subimage quad to draw
                                int _x1 = x - 1;
                                int _y1 = ((screen_h / 2) - (line_height / 2)) + camera_pitch;
                                int _x2 = x;
                                int _y2 = ((screen_h / 2) + (line_height / 2)) + camera_pitch + 1;
                                
                                // Add color to simulate depth
                                int color_scaled = (255 * (line_height / screen_h));
                                if(color_scaled > 255) {
                                    color_scaled = 255;
                                }
                                Gosu::Color wall_color(255,color_scaled,color_scaled,color_scaled);
                                
                                // Render the line
                                response.wall->getData().subimage(texX, 1, 0, response.wall->height() - 2)->draw(
                                   _x1, _y1, wall_color,
                                   _x2, _y1, wall_color,
                                   _x2, _y2, wall_color,
                                   _x1, _y2, wall_color,
                                  z - (wall_dist * 0.05), Gosu::AlphaMode::amDefault
                                );
                                
                                // From the top and bottom of the line we just rendered, the ceiling and floor can be drawn.
                                // This is a pixel-by-pixel operation and is the best spot for any new optimization.
                                if(pass == WALL_PASS) {
                                    double floorXWall, floorYWall;
                                    if(side == 0 && pass_data[x].ray_dir_x > 0) {
                                        floorXWall = cur_x;
                                        floorYWall = cur_y + wall_x;
                                    }
                                    else if(side == 0 && pass_data[x].ray_dir_x < 0) {
                                        floorXWall = cur_x + 1.0;
                                        floorYWall = cur_y + wall_x;
                                    }
                                    else if(side == 1 && pass_data[x].ray_dir_y > 0) {
                                        floorXWall = cur_x + wall_x;
                                        floorYWall = cur_y;
                                    }
                                    else {
                                        floorXWall = cur_x + wall_x;
                                        floorYWall = cur_y + 1.0;
                                    }
                                    
                                    for(int y = _y2 - camera_pitch - 2; y < screen_h + abs(camera_pitch) + 2; y++) {
                                        float current_dist = screen_h / (2.0 * y - screen_h);
                                        double weight = current_dist / wall_dist;
                                        
                                        // Find the square on the ground
                                        double cur_floor_x = weight * floorXWall + (1.0 - weight) * _pos_x;
                                        double cur_floor_y = weight * floorYWall + (1.0 - weight) * _pos_y;
                                        
                                        // Once again, ask what floor is at that point if any
                                        MapData response = query(cur_floor_x, cur_floor_y);
                                        
                                        // And how much darkness to apply
                                        float darkness = fmax(0.0, 1.0 - (current_dist / 10));
                                        
                                        // Floor
                                        if(response.floor) {
                                            // Get the proper texture position
                                            int floorTexX = int(cur_floor_x * response.floor->width()) % response.floor->width();
                                            int floorTexY = int(cur_floor_y * response.floor->height()) % response.floor->height();
                                            
                                            Gosu::Color pixel = response.floor->getPixel(floorTexX, floorTexY);
                                            pixel.setRed(pixel.red() * darkness);
                                            pixel.setGreen(pixel.green() * darkness);
                                            pixel.setBlue(pixel.blue() * darkness);
                                            
                                            float floor_y = (y + camera_pitch);
                                            if(floor_y >= 0 && floor_y < screen_h) {
                                                _ceiling_floor.setPixel(x,floor_y, pixel);
                                            }
                                        } else {
                                            float floor_y = (y + camera_pitch);
                                            if(floor_y >= 0 && floor_y < screen_h) {
                                                _ceiling_floor.setPixel(x,floor_y, Gosu::Color::NONE);
                                            }
                                        }
                                        
                                        // Ceiling - only fully symmetric when player is not tilted
                                        if(response.ceiling) {
                                            int cielTexX = int(cur_floor_x * response.ceiling->width()) % response.ceiling->width();
                                            int cielTexY = int(cur_floor_y * response.ceiling->height()) % response.ceiling->height();
                                            
                                            Gosu::Color pixel = response.ceiling->getPixel(cielTexX, cielTexY);
                                            pixel.setRed(pixel.red() * darkness);
                                            pixel.setGreen(pixel.green() * darkness);
                                            pixel.setBlue(pixel.blue() * darkness);
                                            
                                            float ciel_y = ((screen_h + camera_pitch) - y);
                                            if(ciel_y >= 0 && ciel_y < screen_h) {
                                                _ceiling_floor.setPixel(x, ciel_y, pixel);
                                            }
                                        } else {
                                            float ciel_y = ((screen_h + camera_pitch) - y);
                                            if(ciel_y >= 0 && ciel_y < screen_h) {
                                                _ceiling_floor.setPixel(x, ciel_y, Gosu::Color::NONE);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // SPRITES - by now, our pass data will have included all wall distances. Don't draw slices
        // hidden by the wall distances, and put it at a z where wall sprites block them properly as well!
        for(auto sprite: sprites) {
            double sprite_x = (sprite.x + 0.5) - _pos_x;
            double sprite_y = (sprite.y + 0.5) - _pos_y;
            
            //translate sprite position to relative to player
            double invDet = 1.0 / (_plane_x * _dir_y - _dir_x * _plane_y); // Inverse for matrix math
            double transformX = invDet * (_dir_y * sprite_x - _dir_x * sprite_y);
            double transformZ = invDet * (-_plane_y * sprite_x + _plane_x * sprite_y);
            
            // Calculate our width and height
            int spriteScreenX = int((screen_w / 2) * (1 + transformX / transformZ));
            float spriteHeight = fabs(screen_w / (transformZ)) * 0.75;
            float scale = spriteHeight / sprite.texture->height();
            float spriteWidth = sprite.texture->width() * scale;
            
            // Some color for distance
            int color_scaled = (255 * (spriteHeight / screen_h));
            if(color_scaled > 255) {
                color_scaled = 255;
            }
            Gosu::Color color(255,color_scaled,color_scaled,color_scaled);
            
            // Each stripe is drawn with the same height, in this case
            int _y1 = (screen_h/2) - (spriteHeight / 2) + camera_pitch;
            int _y2 = (screen_h/2) + (spriteHeight / 2) + camera_pitch;
            
            if(transformZ > 0) {
                for(int stripe = 0; stripe < spriteWidth; stripe++) {
                    // Draw it!
                    int _x1 = (spriteScreenX  - (spriteWidth / 2)) + stripe;
                    if(_x1 > 0 && _x1 < screen_w) {
                        int _x2 = _x1 + 1;
                        
                        if(fabs(pass_data[_x1].wall_distance - transformZ) < 0.5 || (pass_data[_x1].wall_distance > transformZ)) {
                            sprite.texture->getData().subimage(stripe / scale, 0, 1, sprite.texture->height())->draw(
                                                                                                                     _x1, _y1, color,
                                                                                                                     _x2, _y1, color,
                                                                                                                     _x2, _y2, color,
                                                                                                                     _x1, _y2, color,
                                                                                                                     -transformZ, Gosu::AlphaMode::amDefault
                                                                                                                     );
                        }
                    }
                }
            }
        }
        
        // Drop the frame rate on the ceiling texture if it was enabled
        if(_fps_enabled) {
            Gosu::drawText(_ceiling_floor, std::to_wstring(Gosu::fps()),0,0,Gosu::Color::WHITE, L"arial", 20);
        }
        
        // Draw ceiling and floor
        Gosu::Image(_ceiling_floor).draw(0,0,z - 50);
    }
}
