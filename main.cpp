/**
 * Example application for testing the gosu raycaster engine - a simple shooter game
 */
#include "raycaster.hpp"

#define RCMapData Gosu::RayCaster::MapData

class Map {
public:
    static const auto MAP_WIDTH = 12;
    static const auto MAP_HEIGHT = 10;

    // Legend
    enum Legend {
        SPACE = 0,
        WALL = 1,
        ENTRANCE = 2,
        BADGUY = 3
    };
    
    Map() :
        _floor(Gosu::Image(L"./assets/floor.jpg").getData().toBitmap()),
        _carpet(Gosu::Image(L"./assets/carpet.png").getData().toBitmap()),
        _wall(Gosu::Image(L"./assets/wall.jpg")),
        _door(Gosu::Image(L"./assets/door.png")),
        _exit(Gosu::Image(L"./assets/exit.jpg"))

    {}
    
    // One X to the right of entrance is player start
    const std::pair<double,double> getPlayerStart() {
        unsigned xy = 0;
        for(; (xy < MAP_WIDTH * MAP_HEIGHT) && _map[xy] != 2; xy++ );
        std::pair<unsigned, unsigned> coord = _indexToCoord(xy);

        // Center the player in the tile
        return std::make_pair(coord.first + 0.5, coord.second + 0.5);
    }
    
    const RCMapData getMapData(const int x, const int y) {
        RCMapData result;
        
        if(x<0 || y<0 || x >= Map::MAP_WIDTH || y >= Map::MAP_HEIGHT) {
            result.invalid = true;
        } else {
            unsigned xy = _coordToIndex(x,y);
            switch(_map[xy]) {
                case WALL:
                    result.wall = &_wall;
                    break;
                case ENTRANCE:
                case SPACE:
                case BADGUY:
                    result.ceiling = &_carpet;
                    result.floor = &_floor;
                    
                    break;
            }
        }
        
        return result;
    }
    
    const bool checkCollision(const int x, const int y) {
        return _map[_coordToIndex(x,y)] == 1;
    }
    
    
    std::vector<Gosu::RayCaster::Sprite> getSprites() {
        std::vector<Gosu::RayCaster::Sprite> result;
        unsigned xy = 0;
        for(; (xy < MAP_WIDTH * MAP_HEIGHT); xy++ ) {
            if(_map[xy] == 3) {
                std::pair<unsigned, unsigned> coord = _indexToCoord(xy);
                Gosu::RayCaster::Sprite sprite;
                sprite.texture = new Gosu::Image(L"./assets/megaman.png");
                sprite.x = coord.first;
                sprite.y = coord.second;
                result.push_back(sprite);
            }
        }
        return result;
    }
    
    
    int testHit(std::pair<double, double> position, std::pair<double, double> coord_system, std::vector<Gosu::RayCaster::Sprite>& sprites) {
        float x = position.first;
        float y = position.second;
        
        do {
            x += coord_system.first;
            y += coord_system.second;
            int at_tile = _map[_coordToIndex(x,y)];
            
            if(at_tile == 1) {
                return -1;
            } else {
                for(int i = 0; i < sprites.size(); i++) {
                    if((int)sprites[i].x == (int)x && (int)sprites[i].y == (int)y) {
                        return i;
                    }
                }
            }
        } while(x > 0 && y > 0 && x < MAP_WIDTH && y < MAP_HEIGHT);
        
        return -1;
    }

private:
    std::pair<unsigned,unsigned> _indexToCoord(const unsigned xy) {
        return std::make_pair(xy % MAP_WIDTH, xy / MAP_WIDTH);
    }
    
    const unsigned _coordToIndex(std::pair<unsigned,unsigned>& xy) {
        return(_coordToIndex(xy.first, xy.second));
    }
    
    const unsigned _coordToIndex(const int x, const int y) {
        return (y * MAP_WIDTH) + x;
    }
    
private:
    // Actual map
    int _map[MAP_WIDTH * MAP_HEIGHT] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
        1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 3, 1,
        1, 0, 0, 1, 0, 0, 3, 0, 1, 0, 0, 1,
        1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 3, 0, 1, 0, 0, 0, 0, 0, 0, 3, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    
    Gosu::Image _wall, _door, _exit;
    Gosu::Bitmap _floor, _carpet;
};

class Window : public Gosu::Window {
public:
    Window() : Gosu::Window(800, 600, false),
        _gun1(L"./assets/gun1.png"),
        _gun2(L"./assets/gun2.png")
    {
        setCaption(L"RayCast");
        _caster.setCameraPosition(_map.getPlayerStart());
        _caster.setCoordinateSystem(0,1); // Face 100% south
        _timer = Gosu::milliseconds();
        
        _map_response = [this](int x, int y) -> RCMapData {
            return _map.getMapData(x,y);
        };
        
        _collision_detector = [this](double x, double y)  -> bool {
            return _map.checkCollision((int)x,(int)y);
        };
        
        _sprites = _map.getSprites();
        
        _gun = &_gun1;
    }
    
    void draw() {
        _caster.draw(this, _map_response, _sprites);
        float gun_scale = (this->graphics().width() / _gun->width())/3;
        _gun->draw(this->graphics().width() / 2, this->graphics().height() - (_gun->height() * gun_scale), 1, gun_scale, gun_scale);
    }
    
    
    void update() {
        // Time elapsed since last frame
        float delta = Gosu::milliseconds() - _timer;
        _timer = Gosu::milliseconds();
        
        // Scaled controls
        float turn_speed = 0.08 * delta;
        float pitch_speed = 0.001 * delta;
        float walk_speed = 0.002 * delta;
        float bob_speed = delta * 0.0003;
        
        // Turn camera on y axis
        if (Gosu::Input::down(Gosu::kbA)) {
            _caster.rotateCamera(-turn_speed);
        } else if (Gosu::Input::down(Gosu::kbD)) {
            _caster.rotateCamera(turn_speed);
        }
        
        // Tilt camera on its x axis
        if (Gosu::Input::down(Gosu::kbQ)) {
            _caster.pitchCamera(pitch_speed);
        } else if (Gosu::Input::down(Gosu::kbW)) {
            _caster.pitchCamera(-pitch_speed);
        }
        
        // Walking. Forward steps bob the camera
        if (Gosu::Input::down(Gosu::kbUp)) {
            _caster.transformCamera(walk_speed, 0, _collision_detector);
            _caster.setCameraBobRange(0.03);
        } else {
            // Quickly return to a bob of 0 in a smooth motion
            _caster.setCameraBobRange(0);
            
            // Walk backwards
            if (Gosu::Input::down(Gosu::kbDown)) {
                _caster.transformCamera(-walk_speed,0, _collision_detector);
            }
        }
        
        // Strafe
        if (Gosu::Input::down(Gosu::kbLeft)) {
            _caster.transformCamera(0,-walk_speed, _collision_detector);
        } else if (Gosu::Input::down(Gosu::kbRight)) {
            _caster.transformCamera(0,walk_speed, _collision_detector);
        }
        
        
        if(_guncooldown > 0) {
            _guncooldown -= delta;
            if(_guncooldown <= 0) {
                _guncooldown = 0;
            }
        } else {
            if(_guntimer > 0) {
                _guntimer -= delta;
                if(_guntimer <= 0) {
                    _guntimer = 0;
                    _gun = &_gun1;
                    _guncooldown = 100;
                }
            } else if(Gosu::Input::down(Gosu::kbSpace)) {
                
                int hit = _map.testHit(_caster.getCameraPosition(), _caster.getCoordinateSystem(), _sprites);
                if(hit >= 0) {
                    _sprites.erase(_sprites.begin() + hit);
                }
                
                _guntimer = 200;
                _gun = &_gun2;
            }
        }
        
        _caster.bobCamera(bob_speed);
    }
    
private:
    Map _map;
    Gosu::RayCaster _caster;
    unsigned long _timer;
    std::vector<Gosu::RayCaster::Sprite> _sprites;
    std::function <RCMapData(int, int)> _map_response;
    std::function <bool(double, double)> _collision_detector;
    Gosu::Image * _gun;
    Gosu::Image _gun1, _gun2;
    unsigned long  _guntimer;
    unsigned long _guncooldown;
};

int main() {
    Window win;
    win.show();
};
