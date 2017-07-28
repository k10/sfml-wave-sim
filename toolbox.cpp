#include "toolbox.h"
std::ostream & operator<<(std::ostream & lhs, const sf::Vector2f rhs)
{
    lhs << "{" << rhs.x << "," << rhs.y << "}";
    return lhs;
}