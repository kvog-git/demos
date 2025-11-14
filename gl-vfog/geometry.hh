#ifndef _GEOMETRY_HH_
#define _GEOMETRY_HH_

#include <cmath>

static inline float Radians(float degrees)
{
  return degrees * M_PI / 180.0f;
}

class Vec3
{
public:
  float x;
  float y;
  float z;
public:
  Vec3(float x, float y, float z)
    : x(x), y(y), z(z) { }

  inline Vec3 operator+(const Vec3& rhs) const { return Vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
  inline Vec3 operator-(const Vec3& rhs) const { return Vec3(x - rhs.x, y - rhs.y, z - rhs.z); }

  inline float* Base() { return &x; }

  inline float Length() { return std::sqrtf(x * x + y * y + z * z); }

  inline Vec3 Normalize()
  {
    float l = Length();
    if (l < 0.0f) { l = 1.0f; }
    return Vec3(x / l, y / l, z / l);
  }

  inline float Dot(const Vec3& rhs)
  {
    return x * rhs.x + y * rhs.y + z * rhs.z;
  }

  inline Vec3 Cross(const Vec3& rhs)
  {
    return Vec3(
      y * rhs.z - z * rhs.y,
      z * rhs.x - x * rhs.z,
      x * rhs.y - y * rhs.x
    );
  }
};

class Mat4x4
{
private:
  float m[4 * 4];
public:
  inline float& operator[](std::size_t s)       { return m[s]; }
  inline float  operator[](std::size_t s) const { return m[s]; }

  inline float* Base() { return m; }

  inline Mat4x4 operator*(const Mat4x4& rhs)
  {
    Mat4x4 r = { };
    for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      r[col * 4 + row] =
        m[0 * 4 + row] * rhs[col * 4 + 0] +
        m[1 * 4 + row] * rhs[col * 4 + 1] +
        m[2 * 4 + row] * rhs[col * 4 + 2] +
        m[3 * 4 + row] * rhs[col * 4 + 3];
    }
    }
    return r;
  }
public:
  static Mat4x4 LookAt(const Vec3& eye, const Vec3& at, const Vec3& up)
  {
    Vec3 f = (at - eye).Normalize();
    Vec3 s = f.Cross(up).Normalize();
    Vec3 u = s.Cross(f);
    Vec3 t = Vec3(-s.Dot(eye), -u.Dot(eye), f.Dot(eye));

    Mat4x4 m;
    m[ 0] = s.x; m[ 1] = u.x; m[ 2] = -f.x; m[ 3] = 0.0f;
    m[ 4] = s.y; m[ 5] = u.y; m[ 6] = -f.y; m[ 7] = 0.0f;
    m[ 8] = s.z; m[ 9] = u.z; m[10] = -f.z; m[11] = 0.0f;
    m[12] = t.x; m[13] = t.y; m[14] =  t.z; m[15] = 1.0f;

    return m;
  }

  static Mat4x4 Perspective(float fov_deg, float aspect, float z_near, float z_far)
  {
    const float fov_cot = 1.0f / std::tanf(Radians(fov_deg) / 2.0f);

    Mat4x4 m = { };

    m[0*4+0] = fov_cot / aspect;
    m[1*4+1] = fov_cot;
    m[2*4+3] = -1.0f;

    m[2*4+2] = (z_far + z_near) / (z_near - z_far);
    m[3*4+2] = (2.0f * z_near * z_far) / (z_near - z_far);

    return m;
  }
};

#endif // _GEOMETRY_HH_
