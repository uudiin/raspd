#include <math.h>

#include "helper_3dmath.h"

void Quaternion(struct Quaternion *this);
void VectorInt16(struct VectorInt16 *this);
void VectorFloat(struct VectorFloat *this);

static void q_getProduct(struct Quaternion *this, struct Quaternion *q1,
        struct Quaternion *q2)
{
    Quaternion(q2);
    // Quaternion multiplication is defined by:
    //     (Q1 * Q2).w = (w1w2 - x1x2 - y1y2 - z1z2)
    //     (Q1 * Q2).x = (w1x2 + x1w2 + y1z2 - z1y2)
    //     (Q1 * Q2).y = (w1y2 - x1z2 + y1w2 + z1x2)
    //     (Q1 * Q2).z = (w1z2 + x1y2 - y1x2 + z1w2
    q2->Quaternion(q2,
            this->w*q1->w - this->x*q1->x - this->y*q1->y - this->z*q1->z,  // new w
            this->w*q1->x + this->x*q1->w + this->y*q1->z - this->z*q1->y,  // new x
            this->w*q1->y - this->x*q1->z + this->y*q1->w + this->z*q1->x,  // new y
            this->w*q1->z + this->x*q1->y - this->y*q1->x + this->z*q1->w); // new z
}

static void q_getConjugate(struct Quaternion *this, struct Quaternion *q) {
    Quaternion(q);
    q->Quaternion(q, this->w, -this->x, -this->y, -this->z);
}

static float q_getMagnitude(struct Quaternion *this) {
    return sqrt(this->w*this->w + this->x*this->x + this->y*this->y + this->z*this->z);
}

static void q_normalize(struct Quaternion *this) {
    float m = this->getMagnitude(this);
    this->w /= m;
    this->x /= m;
    this->y /= m;
    this->z /= m;
}

void q_getNormalized(struct Quaternion *this, struct Quaternion *r) {
            Quaternion(r);
            r->Quaternion(r, this->w, this->x, this->y, this->z);
            r->normalize(r);
        }

static void q_Quaternion(struct Quaternion *this, float nw, float nx, float ny, float nz) {
    this->w = nw;
    this->x = nx;
    this->y = ny;
    this->z = nz;

    this->Quaternion = q_Quaternion;
    this->getProduct = q_getProduct;
    this->getConjugate = q_getConjugate;
    this->getMagnitude = q_getMagnitude;
    this->normalize = q_normalize;
    this->getNormalized = q_getNormalized;
}

void Quaternion(struct Quaternion *this) {
    q_Quaternion(this, 1.0f, 0.0f, 0.0f, 0.0f);
}

static float v16_getMagnitude(struct VectorInt16 *this) {
    return sqrt(this->x*this->x + this->y*this->y + this->z*this->z);
}

static void v16_normalize(struct VectorInt16 *this) {
    float m = this->getMagnitude(this);
    this->x /= m;
    this->y /= m;
    this->z /= m;
}

static void v16_getNormalized(struct VectorInt16 *this, struct VectorInt16 *r) {
    VectorInt16(r);
    r->VectorInt16(r, this->x, this->y, this->z);
    r->normalize(r);
}

static void v16_rotate(struct VectorInt16 *this, struct Quaternion *q)
{
    // http://www.cprogramming.com/tutorial/3d/quaternions.html
    // http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/transforms/index.htm
    // http://content.gpwiki.org/index.php/OpenGL:Tutorials:Using_Quaternions_to_represent_rotation
    // ^ or: http://webcache.googleusercontent.com/search?q=cache:xgJAp3bDNhQJ:content.gpwiki.org/index.php/OpenGL:Tutorials:Using_Quaternions_to_represent_rotation&hl=en&gl=us&strip=1

    // P_out = q * P_in * conj(q)
    // - P_out is the output vector
    // - q is the orientation quaternion
    // - P_in is the input vector (a*aReal)
    // - conj(q) is the conjugate of the orientation quaternion (q=[w,x,y,z], q*=[w,-x,-y,-z])
    struct Quaternion p, r;
    Quaternion(&p);
    p.Quaternion(&p, 0, this->x, this->y, this->z);

    // quaternion multiplication: q * p, stored back in p
    q->getProduct(q, &p, &p);

    // quaternion multiplication: p * conj(q), stored back in p
    q->getConjugate(q, &r);
    p.getProduct(&p, &r, &p);

    // p quaternion is now [0, x', y', z']
    this->x = p.x;
    this->y = p.y;
    this->z = p.z;
}

static void v16_getRotated(struct VectorInt16 *this, struct Quaternion *q, struct VectorInt16 *r) {
    VectorInt16(r);
    r->VectorInt16(r, this->x, this->y, this->z);
    r->rotate(r, q);
}

static void v16_VectorInt16(struct VectorInt16 *this, int16_t nx, int16_t ny, int16_t nz) {
    this->x = nx;
    this->y = ny;
    this->z = nz;

    this->VectorInt16 = v16_VectorInt16;
    this->getMagnitude = v16_getMagnitude;
    this->normalize = v16_normalize;
    this->getNormalized = v16_getNormalized;
    this->rotate = v16_rotate;
    this->getRotated = v16_getRotated;
}

void VectorInt16(struct VectorInt16 *this) {
    v16_VectorInt16(this, 0, 0, 0);
}

static float vf_getMagnitude(struct VectorFloat *this) {
    return sqrt(this->x*this->x + this->y*this->y + this->z*this->z);
}

static void vf_normalize(struct VectorFloat *this) {
    float m = this->getMagnitude(this);
    this->x /= m;
    this->y /= m;
    this->z /= m;
}

static void vf_getNormalized(struct VectorFloat *this, struct VectorFloat *r) {
    r->VectorFloat(r, this->x, this->y, this->z);
    r->normalize(r);
}

static void vf_rotate(struct VectorFloat *this, struct Quaternion *q)
{
    struct Quaternion p, r;
    Quaternion(&p);
    p.Quaternion(&p, 0, this->x, this->y, this->z);

    // quaternion multiplication: q * p, stored back in p
    q->getProduct(q, &p, &p);

    // quaternion multiplication: p * conj(q), stored back in p
    q->getConjugate(q, &r);
    p.getProduct(&p, &r, &p);

    // p quaternion is now [0, x', y', z']
    this->x = p.x;
    this->y = p.y;
    this->z = p.z;
}

static void vf_getRotated(struct VectorFloat *this, struct Quaternion *q, struct VectorFloat *r) {
    VectorFloat(r);
    r->VectorFloat(r, this->x, this->y, this->z);
    r->rotate(r, q);
}

static void vf_VectorFloat(struct VectorFloat *this, float nx, float ny, float nz) {
    this->x = nx;
    this->y = ny;
    this->z = nz;

    this->VectorFloat = vf_VectorFloat;
    this->getMagnitude = vf_getMagnitude;
    this->normalize = vf_normalize;
    this->getNormalized = vf_getNormalized;
    this->rotate = vf_rotate;
    this->getRotated = vf_getRotated;
}

void VectorFloat(struct VectorFloat *this) {
    vf_VectorFloat(this, 0, 0, 0);
}
