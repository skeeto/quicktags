// Shapes, for the C++ outline and completion plugins. Public domain.
#pragma once
#include <string>

namespace geo {

enum class Kind { Circle, Square };

struct Vec {
    double x, y;
    Vec operator+(const Vec &o) const;
};

class Shape {
public:
    explicit Shape(std::string name);
    virtual ~Shape();
    virtual double area() const = 0;
    const std::string &name() const { return name_; }
    static int count;

protected:
    Vec origin;

private:
    std::string name_;
};

class Circle : public Shape {
public:
    Circle(double r);
    double area() const override;

private:
    double radius;
};

class Square final : public Shape, private Vec {
public:
    Square(double side);
    double area() const override;

private:
    double side;
};

template<typename T>
T clamp(T v, T lo, T hi)
{
    return v < lo ? lo : hi < v ? hi : v;
}

using ShapePtr = Shape *;

}  // namespace geo
