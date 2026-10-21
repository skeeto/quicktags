// Shapes, defined. Public domain.
#include "shapes.hpp"

namespace geo {

int Shape::count = 0;

Vec Vec::operator+(const Vec &o) const
{
    return {x + o.x, y + o.y};
}

Shape::Shape(std::string name) : name_(std::move(name))
{
    count++;
}

Shape::~Shape()
{
    count--;
}

Circle::Circle(double r) : Shape("circle"), radius(r) {}

double Circle::area() const
{
    return 3.14159 * radius * radius;
}

Square::Square(double s) : Shape("square"), side(s) {}

double Square::area() const
{
    return side * side;
}

namespace {
int hidden_total;
double scale(double v) { return v * 2; }
}

}  // namespace geo

static int file_local;

int total_area(geo::ShapePtr *shapes, int n)
{
    auto add = [](double a, geo::Shape *s) { return a + s->area(); };
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum = add(sum, shapes[i]);
    }
    file_local = n;
    return (int)sum;
}
