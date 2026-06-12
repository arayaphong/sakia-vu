#include "Box2dPhysicsWorld.h"

#include "core/models/MeterLayout.h"

#include <algorithm>
#include <cmath>

namespace ml = meterlayout;

namespace {
constexpr b2Vec2 kGravityNormal{0.0f, 19.6f}; // y-down world; demo ratio 2.0
constexpr b2Vec2 kGravityLow{0.0f, 1.96f};    //                demo ratio 0.2
} // namespace

Box2dPhysicsWorld::Box2dPhysicsWorld() {
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = kGravityNormal;
    worldDef.enableSleep = false; // bars move constantly; keep contacts live
    world_ = b2CreateWorld(&worldDef);

    // Four static walls, 1 m thick, just outside the canvas. Ground top edge
    // sits exactly on the bar-bottom line (kGroundY = 514 px).
    const float w = toM(ml::kLogicalW), h = toM(ml::kLogicalH);
    const float groundY = toM(ml::kGroundY);
    struct Wall { b2Vec2 center; b2Vec2 half; };
    const Wall walls[] = {
        {{w / 2, groundY + 0.5f}, {w / 2 + 1.0f, 0.5f}}, // ground
        {{w / 2, -0.5f}, {w / 2 + 1.0f, 0.5f}},          // ceiling
        {{-0.5f, h / 2}, {0.5f, h / 2 + 1.0f}},          // left
        {{w + 0.5f, h / 2}, {0.5f, h / 2 + 1.0f}},       // right
    };
    for (const Wall& wall : walls) {
        b2BodyDef bd = b2DefaultBodyDef();
        bd.position = wall.center;
        b2BodyId body = b2CreateBody(world_, &bd);
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.material.friction = 0.3f;
        sd.material.restitution = 0.72f;
        b2Polygon box = b2MakeBox(wall.half.x, wall.half.y);
        b2CreatePolygonShape(body, &sd, &box);
    }

    // Kinematic bar bodies: x fixed at each band center, tall enough that the
    // bottom never clears the ground no matter how low the top drops.
    const float barHalfW = toM(ml::kBarW) / 2;
    const float peakHalfH = toM(ml::kSegH) / 2;
    for (int b = 0; b < MeterState::kNumBands; b++) {
        const float cx = toM(ml::barCenterX(b));

        b2BodyDef barDef = b2DefaultBodyDef();
        barDef.type = b2_kinematicBody;
        barDef.position = {cx, groundY + kBarHalfH};
        bars_[b] = b2CreateBody(world_, &barDef);
        b2ShapeDef barShape = b2DefaultShapeDef();
        barShape.material.friction = 0.1f;
        barShape.material.restitution = 0.0f; // bounce comes from bar motion
        b2Polygon barBox = b2MakeBox(barHalfW, kBarHalfH);
        b2CreatePolygonShape(bars_[b], &barShape, &barBox);

        // Peak-hold platform: a thin ledge matching the drawn peak segment.
        b2BodyDef peakDef = b2DefaultBodyDef();
        peakDef.type = b2_kinematicBody;
        peakDef.position = {cx, groundY + peakHalfH};
        peakDef.isEnabled = false; // peak hold starts off
        peakPlatforms_[b] = b2CreateBody(world_, &peakDef);
        b2ShapeDef peakShape = b2DefaultShapeDef();
        peakShape.material.friction = 0.2f;
        peakShape.material.restitution = 0.3f;
        b2Polygon peakBox = b2MakeBox(barHalfW, peakHalfH);
        b2CreatePolygonShape(peakPlatforms_[b], &peakShape, &peakBox);
    }
}

Box2dPhysicsWorld::~Box2dPhysicsWorld() {
    b2DestroyWorld(world_);
}

void Box2dPhysicsWorld::step(float dtSeconds, const MeterState& meter) {
    accumulator_ += std::min(dtSeconds, 0.05f);
    while (accumulator_ >= kFixedDt) {
        syncKinematics(meter);
        b2World_Step(world_, kFixedDt, kSubSteps);
        accumulator_ -= kFixedDt;
    }
    pruneEscaped();
}

void Box2dPhysicsWorld::syncKinematics(const MeterState& meter) {
    const float peakHalfH = toM(ml::kSegH) / 2;

    if (meter.peakHoldEnabled != peaksEnabled_) {
        peaksEnabled_ = meter.peakHoldEnabled;
        for (int b = 0; b < MeterState::kNumBands; b++) {
            if (peaksEnabled_) {
                // Snap into place before enabling so the first tracked step
                // doesn't turn the teleport into a launch impulse.
                float ty = toM(ml::barTopForLevel(meter.peaks[b])) + peakHalfH;
                b2Body_SetTransform(peakPlatforms_[b],
                                    {toM(ml::barCenterX(b)), ty}, b2MakeRot(0));
                b2Body_SetLinearVelocity(peakPlatforms_[b], {0, 0});
                b2Body_Enable(peakPlatforms_[b]);
            } else {
                b2Body_Disable(peakPlatforms_[b]);
            }
        }
    }

    for (int b = 0; b < MeterState::kNumBands; b++) {
        // The Matter demo's "reposition + velocity" trick, the Box2D way:
        // velocity (not transform) so the solver imparts real momentum, and
        // v = error/dt lands the body exactly on target after the step.
        float targetY = toM(ml::barTopForLevel(meter.levels[b])) + kBarHalfH;
        float vy = (targetY - b2Body_GetPosition(bars_[b]).y) / kFixedDt;
        b2Body_SetLinearVelocity(
            bars_[b], {0, std::clamp(vy, -kMaxKinematicSpeed, kMaxKinematicSpeed)});

        if (peaksEnabled_) {
            float peakTargetY = toM(ml::barTopForLevel(meter.peaks[b])) + peakHalfH;
            float pvy =
                (peakTargetY - b2Body_GetPosition(peakPlatforms_[b]).y) / kFixedDt;
            b2Body_SetLinearVelocity(
                peakPlatforms_[b],
                {0, std::clamp(pvy, -kMaxKinematicSpeed, kMaxKinematicSpeed)});
        }
    }
}

float Box2dPhysicsWorld::safeSpawnY(float lx, float halfPx, float ly) const {
    float topY = ml::kGroundY;
    for (int b = 0; b < MeterState::kNumBands; b++) {
        if (std::abs(ml::barCenterX(b) - lx) < halfPx + ml::kBarW / 2) {
            float barTop = toPx(b2Body_GetPosition(bars_[b]).y - kBarHalfH);
            topY = std::min(topY, barTop);
        }
    }
    return std::min(ly, topY - halfPx - 6.0f);
}

void Box2dPhysicsWorld::addObject(PhysicsObject::Kind kind, float lx, float ly) {
    if (static_cast<int>(objects_.size()) >= kMaxObjects) {
        b2DestroyBody(objects_.front().first);
        objects_.pop_front();
    }

    const bool isBall = kind == PhysicsObject::Kind::Ball;
    std::uniform_real_distribution<float> sizeDist(isBall ? 42.0f : 40.0f,
                                                   isBall ? 60.0f : 55.0f);
    std::uniform_real_distribution<float> hueDist(isBall ? 200.0f : 265.0f,
                                                  isBall ? 255.0f : 310.0f);
    const float sizePx = sizeDist(rng_); // ball radius / box half-extent
    const float hue = hueDist(rng_);

    if (lx < 0.0f) { // button spawn: random x across the meter
        std::uniform_real_distribution<float> xDist(
            ml::kPadX + sizePx + 2.0f, ml::kLogicalW - ml::kPadX - sizePx - 2.0f);
        lx = xDist(rng_);
    }
    lx = std::clamp(lx, sizePx + 2.0f, ml::kLogicalW - sizePx - 2.0f);
    ly = safeSpawnY(lx, sizePx, ly);

    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = {toM(lx), toM(ly)};
    bd.isBullet = true; // fast kinematic bars: no tunneling
    bd.linearDamping = isBall ? 0.1f : 0.05f;
    bd.angularDamping = isBall ? 0.1f : 0.05f;
    b2BodyId body = b2CreateBody(world_, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = isBall ? 20.0f : 30.0f;
    sd.material.friction = isBall ? 0.05f : 0.1f;
    sd.material.restitution = isBall ? 0.8f : 0.6f;
    if (isBall) {
        b2Circle circle{{0, 0}, toM(sizePx)};
        b2CreateCircleShape(body, &sd, &circle);
    } else {
        b2Polygon box = b2MakeBox(toM(sizePx), toM(sizePx));
        b2CreatePolygonShape(body, &sd, &box);
    }

    PhysicsObject obj;
    obj.kind = kind;
    obj.size = sizePx;
    obj.hue = hue;
    objects_.emplace_back(body, obj);
}

void Box2dPhysicsWorld::spawnBall(float lx, float ly) {
    addObject(PhysicsObject::Kind::Ball, lx, ly);
}

void Box2dPhysicsWorld::spawnBox(float lx, float ly) {
    addObject(PhysicsObject::Kind::Box, lx, ly);
}

void Box2dPhysicsWorld::clear() {
    for (auto& [body, obj] : objects_) b2DestroyBody(body);
    objects_.clear();
}

void Box2dPhysicsWorld::setLowGravity(bool low) {
    b2World_SetGravity(world_, low ? kGravityLow : kGravityNormal);
}

void Box2dPhysicsWorld::pruneEscaped() {
    // Safety net: the walls should make escape impossible, but never let a
    // glitched body linger at huge coordinates.
    std::erase_if(objects_, [](const auto& entry) {
        b2Vec2 p = b2Body_GetPosition(entry.first);
        if (std::abs(p.x) > 50.0f || std::abs(p.y) > 50.0f) {
            b2DestroyBody(entry.first);
            return true;
        }
        return false;
    });
}

PhysicsState Box2dPhysicsWorld::state() const {
    PhysicsState out;
    out.objects.reserve(objects_.size());
    for (const auto& [body, obj] : objects_) {
        PhysicsObject o = obj;
        b2Vec2 p = b2Body_GetPosition(body);
        o.x = toPx(p.x);
        o.y = toPx(p.y);
        o.angle = b2Rot_GetAngle(b2Body_GetRotation(body));
        out.objects.push_back(o);
    }
    return out;
}
