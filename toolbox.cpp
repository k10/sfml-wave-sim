#include "toolbox.h"
const double PI = 4 * atan(1);
std::ostream & operator<<(std::ostream & lhs, const sf::Vector2f rhs)
{
    lhs << "{" << rhs.x << "," << rhs.y << "}";
    return lhs;
}
float clampf(float value, float minValue, float maxValue)
{
    return std::min(std::max(minValue, value), maxValue);
}