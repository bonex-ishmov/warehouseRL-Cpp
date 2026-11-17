#include "warehouse/render/renderer.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

namespace wrl {

namespace palette {

sf::Color stressColor(float stress) {
    const float s = std::clamp(stress, 0.0f, 1.0f);
    if (s < 0.5f) return sf::Color(200, 150, 80);  // carga tranquila: color cartón
    const float t = (s - 0.5f) / 0.5f;
    return sf::Color(static_cast<std::uint8_t>(230 + 25 * t), static_cast<std::uint8_t>(140 * (1 - t) + 40 * t), 40);
}

sf::Color speedColor(float fraction) {
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    auto mix = [](float a, float b, float t) { return static_cast<std::uint8_t>(a + (b - a) * t); };
    if (f < 0.5f) {
        const float t = f / 0.5f;
        return sf::Color(mix(60, 70, t), mix(110, 210, t), mix(240, 90, t));
    }
    const float t = (f - 0.5f) / 0.5f;
    return sf::Color(mix(70, 250, t), mix(210, 70, t), mix(90, 50, t));
}

}  // namespace palette

namespace {

using namespace palette;

sf::Vector2f v(Vec2 p) { return {p.x, p.y}; }

sf::Color alpha(sf::Color c, int a) { return sf::Color(c.r, c.g, c.b, static_cast<std::uint8_t>(std::clamp(a, 0, 255))); }

sf::Color lighten(sf::Color c, float t) {
    auto l = [t](std::uint8_t x) { return static_cast<std::uint8_t>(x + (255 - x) * t); };
    return sf::Color(l(c.r), l(c.g), l(c.b), c.a);
}

sf::Color darken(sf::Color c, float t) {
    auto d = [t](std::uint8_t x) { return static_cast<std::uint8_t>(x * (1.0f - t)); };
    return sf::Color(d(c.r), d(c.g), d(c.b), c.a);
}

Vec2 perp(Vec2 d) { return {-d.y, d.x}; }

}  // namespace

Renderer::Renderer(sf::RenderTarget& target) : window_(target) {
    // SFML 3 no trae fuente propia: se busca una del sistema.
    for (const char* path : {"/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
                             "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                             "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
                             "/usr/share/fonts/TTF/DejaVuSans.ttf", "C:/Windows/Fonts/arial.ttf"}) {
        if (!std::filesystem::exists(path)) continue;
        sf::Font f;
        if (f.openFromFile(path)) {
            font_ = std::move(f);
            break;
        }
    }
}

void Renderer::setWorldView(const WarehouseEnv& env, const RenderOptions& opt) {
    const auto size = window_.getSize();
    const float aspect = static_cast<float>(size.x) / static_cast<float>(size.y);
    const auto& m = env.map();
    float h = std::max(m.height, m.width / aspect) * 1.06f / opt.zoom;
    float w = h * aspect;
    Vec2 center{m.width * 0.5f, m.height * 0.5f};
    if (opt.follow) center = env.robot().position();
    center += opt.pan;
    view_ = sf::View();
    view_.setCenter(v(center));
    view_.setSize({w, -h});  // y negativo: el eje y del mundo apunta hacia arriba
    window_.setView(view_);
    metersPerPixel_ = h / static_cast<float>(size.y);
}

// --- Primitivas ---------------------------------------------------------------

void Renderer::tri(Vec2 a, Vec2 b, Vec2 c, sf::Color col) {
    batch_.append({v(a), col});
    batch_.append({v(b), col});
    batch_.append({v(c), col});
}

void Renderer::quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, sf::Color col) {
    tri(a, b, c, col);
    tri(a, c, d, col);
}

void Renderer::rect(const Rect& r, sf::Color fill) {
    quad({r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}, fill);
}

void Renderer::rectOutline(const Rect& r, float w, sf::Color col) {
    rect({r.x, r.y, r.w, w}, col);
    rect({r.x, r.y + r.h - w, r.w, w}, col);
    rect({r.x, r.y + w, w, r.h - 2 * w}, col);
    rect({r.x + r.w - w, r.y + w, w, r.h - 2 * w}, col);
}

void Renderer::box(Vec2 c, Vec2 half, float theta, sf::Color col) {
    const Vec2 ax = fromAngle(theta) * half.x, ay = perp(fromAngle(theta)) * half.y;
    quad(c - ax - ay, c + ax - ay, c + ax + ay, c - ax + ay, col);
}

void Renderer::disc(Vec2 c, float r, sf::Color col, int segments) {
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(segments);
        tri(c, c + fromAngle(a0) * r, c + fromAngle(a1) * r, col);
    }
}

void Renderer::ring(Vec2 c, float r, float w, sf::Color col, float a0, float a1, int segments) {
    const int n = std::max(2, static_cast<int>(std::ceil(static_cast<float>(segments) * std::abs(a1 - a0) / (2.0f * kPi))));
    for (int i = 0; i < n; ++i) {
        const float t0 = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(n);
        const float t1 = a0 + (a1 - a0) * static_cast<float>(i + 1) / static_cast<float>(n);
        quad(c + fromAngle(t0) * (r - w * 0.5f), c + fromAngle(t0) * (r + w * 0.5f), c + fromAngle(t1) * (r + w * 0.5f),
             c + fromAngle(t1) * (r - w * 0.5f), col);
    }
}

void Renderer::segment(Vec2 a, Vec2 b, float w, sf::Color col) {
    const Vec2 n = perp(normalized(b - a)) * (w * 0.5f);
    quad(a - n, b - n, b + n, a + n, col);
}

void Renderer::dashed(const std::vector<Vec2>& pts, float w, float dash, float gap, sf::Color col) {
    // Estado del patrón (trazo o hueco y cuánto le queda), continuo entre tramos.
    bool on = true;
    float left = dash;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const Vec2 a = pts[i], b = pts[i + 1];
        const float len = distance(a, b);
        if (len < 1e-6f) continue;
        const Vec2 d = (b - a) / len;
        float pos = 0.0f;
        while (len - pos > 1e-5f) {
            const float step = std::min(left, len - pos);
            if (on) segment(a + d * pos, a + d * (pos + step), w, col);
            pos += step;
            left -= step;
            if (left <= 1e-5f) {
                on = !on;
                left = on ? dash : gap;
            }
        }
    }
}

void Renderer::arrow(Vec2 from, Vec2 to, float w, float head, sf::Color col) {
    const float len = distance(from, to);
    if (len < 1e-4f) return;
    const Vec2 d = (to - from) / len;
    const float h = std::min(head, len * 0.6f);
    segment(from, to - d * h * 0.8f, w, col);
    tri(to, to - d * h + perp(d) * (h * 0.55f), to - d * h - perp(d) * (h * 0.55f), col);
}

void Renderer::hatch(const Rect& r, float spacing, float w, sf::Color col) {
    // Rayas a 45°: punto (x + k + t, y + t), recortadas al rectángulo.
    for (float k = -r.h; k < r.w; k += spacing) {
        const float t0 = std::max(0.0f, -k), t1 = std::min(r.h, r.w - k);
        if (t1 > t0) segment({r.x + k + t0, r.y + t0}, {r.x + k + t1, r.y + t1}, w, col);
    }
}

void Renderer::bolt(Vec2 c, float s, sf::Color col) {
    tri(c + Vec2{0.10f, 0.30f} * s, c + Vec2{-0.14f, -0.03f} * s, c + Vec2{0.05f, -0.03f} * s, col);
    tri(c + Vec2{-0.05f, 0.03f} * s, c + Vec2{0.14f, 0.03f} * s, c + Vec2{-0.10f, -0.30f} * s, col);
}

void Renderer::flush() {
    if (batch_.getVertexCount() > 0) window_.draw(batch_);
    batch_.clear();
}

// --- Escena -------------------------------------------------------------------

void Renderer::draw(const WarehouseEnv& env, const RenderOptions& opt) {
    setWorldView(env, opt);
    labels_.clear();
    batch_.clear();

    drawFloor(env, opt);
    if (opt.history && opt.trail) drawTrail(*opt.history, env, opt);
    drawTasks(env, opt);

    if (opt.path && !env.path().empty()) {
        dashed(env.path().points(), 0.05f, 0.25f, 0.15f, alpha(kPath, 200));
        disc(env.carrot(), 0.07f, sf::Color::White);
        ring(env.decision().goal, env.config().sim.dockRadius, 0.04f, alpha(kPath, 160));
    }

    const Pose pose = env.robot().pose();
    if (opt.lidar) {
        // Rayos coloreados por cercanía: rojo = obstáculo a menos de 0.8 m.
        const auto& scan = env.lidar();
        const float range = env.config().sensors.lidarRange;
        for (size_t i = 0; i < scan.size(); ++i) {
            const Vec2 end = pose.pos + fromAngle(pose.theta + env.sensors().rayAngle(static_cast<int>(i))) * scan[i];
            const sf::Color c = scan[i] < 0.8f   ? sf::Color(255, 80, 70)
                                : scan[i] < 2.0f ? sf::Color(255, 180, 60)
                                                 : sf::Color(120, 255, 120);
            segment(pose.pos, end, 0.012f, alpha(c, scan[i] < 2.0f ? 110 : 45));
            if (scan[i] < range - 1e-3f) disc(end, 0.035f, alpha(c, 200), 8);
        }
    }

    drawObstacles(env);
    if (opt.history && opt.events) drawEvents(*opt.history, env, opt);
    drawRobot(env, opt);
    flush();

    if (opt.labels) drawLabels();
    drawScaleBar();
    window_.setView(view_);
}

void Renderer::drawFloor(const WarehouseEnv& env, const RenderOptions& opt) {
    const auto& m = env.map();
    rect({0, 0, m.width, m.height}, kFloor);

    for (const auto& z : m.frictionZones) {
        rect(z.rect, alpha(kFriction, 45));
        hatch(z.rect, 0.35f, 0.04f, alpha(kFriction, 110));
        rectOutline(z.rect, 0.04f, alpha(kFriction, 160));
        if (opt.labels) {
            char buf[48];
            std::snprintf(buf, sizeof buf, "\xce\xbc %.2f", static_cast<double>(z.mu));  // μ en UTF-8
            labels_.push_back({z.rect.center(), buf, lighten(kFriction, 0.4f), 10});
        }
    }
    for (const auto& r : m.restricted) {
        rect(r, alpha(kRestricted, 40));
        hatch(r, 0.3f, 0.05f, alpha(kRestricted, 120));
        rectOutline(r, 0.06f, alpha(kRestricted, 220));
        if (opt.labels) labels_.push_back({r.center(), "restringida", lighten(kRestricted, 0.3f), 10, 0.0f, true});
    }
    for (const auto& w : m.walls) rect(w, kWall);
    for (const auto& s : m.shelves) {
        // Estantería: marco oscuro y baldas, para que se lea como mueble y no como suelo.
        rect(s, darken(kShelf, 0.35f));
        rect(s.inflated(-0.06f), kShelf);
        const bool horizontal = s.w >= s.h;
        const int n = std::max(1, static_cast<int>((horizontal ? s.w : s.h) / 0.9f));
        for (int i = 1; i < n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            if (horizontal) segment({s.x + s.w * t, s.y + 0.06f}, {s.x + s.w * t, s.y + s.h - 0.06f}, 0.04f, darken(kShelf, 0.35f));
            else segment({s.x + 0.06f, s.y + s.h * t}, {s.x + s.w - 0.06f, s.y + s.h * t}, 0.04f, darken(kShelf, 0.35f));
        }
    }

    for (const auto& c : m.chargers) {
        rect({c.x - 0.45f, c.y - 0.45f, 0.9f, 0.9f}, alpha(kCharger, 45));
        rectOutline({c.x - 0.45f, c.y - 0.45f, 0.9f, 0.9f}, 0.05f, kCharger);
        bolt(c, 1.0f, kCharger);
    }
    for (size_t i = 0; i < m.dropoffs.size(); ++i) {
        const Vec2 d = m.dropoffs[i];
        rect({d.x - 0.4f, d.y - 0.4f, 0.8f, 0.8f}, alpha(kDropoff, 50));
        rectOutline({d.x - 0.4f, d.y - 0.4f, 0.8f, 0.8f}, 0.05f, kDropoff);
        // Flecha hacia adentro: zona de entrega.
        arrow(d + Vec2{0, 0.28f}, d - Vec2{0, 0.12f}, 0.05f, 0.16f, alpha(kDropoff, 200));
        if (opt.labels) labels_.push_back({d + Vec2{0, -0.4f}, "E" + std::to_string(i + 1), kDropoff, 11, 8.0f});
    }
    for (const auto& p : m.pickups) disc(p, 0.08f, alpha(kPickup, 110), 10);
}

void Renderer::drawTasks(const WarehouseEnv& env, const RenderOptions& opt) {
    const double now = env.time();
    const int current = env.decision().taskId;
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now) * 6.0f);

    for (const auto& t : env.tasks().tasks()) {
        const bool isCurrent = t.id == current;
        const sf::Color c = kPriority[static_cast<int>(t.priority)];
        if (isCurrent && t.status != TaskStatus::Carrying) {
            // Encargo actual: línea punteada de la recogida al destino.
            dashed({t.pickup, t.destination}, 0.03f, 0.15f, 0.15f, alpha(c, 130));
        }
        if (isCurrent || t.status == TaskStatus::Carrying) {
            ring(t.destination, 0.32f, 0.05f, alpha(c, 220));
            disc(t.destination, 0.07f, c, 10);
        }
        if (!t.open()) continue;

        const float r = 0.22f;
        if (isCurrent) disc(t.pickup, r + 0.07f + 0.04f * pulse, alpha(sf::Color::White, 200), 4);
        disc(t.pickup, r, c, 4);  // rombo
        disc(t.pickup, r * 0.4f, darken(c, 0.4f), 4);

        std::string label = "#" + std::to_string(t.id);
        if (t.hasDeadline()) {
            // Anillo de plazo: se vacía a medida que se acerca el vencimiento.
            const double total = std::max(1.0, t.deadline - t.releaseTime);
            const float left = static_cast<float>((t.deadline - now) / total);
            if (left > 0.0f) {
                const sf::Color rc = left > 0.5f   ? sf::Color(90, 210, 110)
                                     : left > 0.2f ? sf::Color(240, 190, 50)
                                                   : sf::Color(255, 70, 60);
                ring(t.pickup, r + 0.13f, 0.05f, alpha(sf::Color::White, 35));
                ring(t.pickup, r + 0.13f, 0.05f, rc, kPi * 0.5f, kPi * 0.5f + 2.0f * kPi * std::min(left, 1.0f));
                label += "  " + std::to_string(static_cast<int>(t.deadline - now)) + " s";
            } else {
                ring(t.pickup, r + 0.13f, 0.05f, alpha(sf::Color(255, 60, 60), 120 + static_cast<int>(120 * pulse)));
                label += "  vencida";
            }
        }
        if (opt.labels) labels_.push_back({t.pickup + Vec2{0.0f, r + 0.2f}, label, lighten(c, 0.3f), 11, -6.0f, isCurrent});
    }
}

void Renderer::drawTrail(const EpisodeHistory& h, const WarehouseEnv& env, const RenderOptions& opt) {
    if (h.trail.size() < 2) return;
    const sf::Color base = opt.robotColor.value_or(sf::Color(50, 120, 230));
    const float vmax = std::max(0.1f, env.config().robot.maxForwardSpeed);
    for (size_t i = 1; i < h.trail.size(); ++i) {
        const uint8_t f = h.flags[i];
        sf::Color c;
        float w = 0.05f;
        if (opt.trailMode == TrailMode::Speed) {
            c = alpha(speedColor(std::abs(h.speed[i]) / vmax), 200);
            w = 0.07f;
        } else if (f & kCharging) {
            c = alpha(kCharger, 200);
            w = 0.08f;
        } else if (f & kCarrying) {
            c = alpha(base, 230);  // con carga: trazo grueso y opaco
            w = 0.10f;
        } else {
            c = alpha(lighten(base, 0.25f), 110);  // vacío: trazo fino y translúcido
        }
        segment(h.trail[i - 1], h.trail[i], w, c);
        if (f & kSlipping) disc(h.trail[i], 0.06f, alpha(kSlip, 170), 6);
    }
}

void Renderer::drawEventIcon(const EventMark& e, float s) {
    const Vec2 p = e.pos;
    switch (e.kind) {
        case EventKind::Collision: {
            const sf::Color c = kSeverity[std::clamp(e.detail, 0, 2)];
            disc(p, s * 1.1f, alpha(sf::Color::Black, 150), 12);
            segment(p + Vec2{-s, -s} * 0.7f, p + Vec2{s, s} * 0.7f, s * 0.35f, c);
            segment(p + Vec2{-s, s} * 0.7f, p + Vec2{s, -s} * 0.7f, s * 0.35f, c);
            if (e.detail == 2) ring(p, s * 1.15f, s * 0.2f, c);
            break;
        }
        case EventKind::Delivered:
        case EventKind::DeliveredLate:
            disc(p, s, e.kind == EventKind::Delivered ? kDelivered : kLate, 14);
            segment(p + Vec2{-0.5f, 0.0f} * s, p + Vec2{-0.12f, -0.4f} * s, s * 0.25f, sf::Color::White);
            segment(p + Vec2{-0.12f, -0.4f} * s, p + Vec2{0.55f, 0.45f} * s, s * 0.25f, sf::Color::White);
            break;
        case EventKind::Pickup:
            tri(p + Vec2{0, s}, p + Vec2{-s * 0.9f, -s * 0.6f}, p + Vec2{s * 0.9f, -s * 0.6f}, kPickupEvent);
            break;
        case EventKind::LoadDrop:
            tri(p + Vec2{0, -s * 1.1f}, p + Vec2{-s, s * 0.7f}, p + Vec2{s, s * 0.7f}, kLoadDrop);
            segment(p + Vec2{0, s * 0.45f}, p + Vec2{0, -s * 0.25f}, s * 0.22f, sf::Color::White);
            break;
        case EventKind::RechargeStart: bolt(p, s * 2.2f, kCharger); break;
        case EventKind::RestrictedEntry:
            ring(p, s, s * 0.3f, kRestricted);
            segment(p + Vec2{-s, -s} * 0.7f, p + Vec2{s, s} * 0.7f, s * 0.25f, kRestricted);
            break;
        case EventKind::CriticalOverdue: break;  // sin lugar en el mapa: solo en la línea de tiempo
    }
}

void Renderer::drawEvents(const EpisodeHistory& h, const WarehouseEnv& env, const RenderOptions& opt) {
    const float now = static_cast<float>(env.time());
    for (const auto& e : h.events) {
        if (e.kind == EventKind::CriticalOverdue) continue;
        const float age = now - e.time;
        if (age < 1.5f && age >= 0.0f) {
            // Onda que se expande: llama la atención sobre lo que acaba de pasar.
            const float k = age / 1.5f;
            const sf::Color c = e.kind == EventKind::Collision ? kSeverity[std::clamp(e.detail, 0, 2)]
                                : e.kind == EventKind::LoadDrop ? kLoadDrop
                                : e.kind == EventKind::Pickup   ? kPickupEvent
                                                                : kDelivered;
            ring(e.pos, 0.2f + 0.9f * k, 0.06f, alpha(c, static_cast<int>(230 * (1.0f - k))));
        }
        drawEventIcon(e, 0.16f);

        if (opt.labels && age < kRecentEvent && age >= 0.0f) {
            std::string text;
            sf::Color c = sf::Color::White;
            switch (e.kind) {
                case EventKind::Collision:
                    text = e.detail == 2 ? "¡choque grave!" : e.detail == 1 ? "choque leve" : "roce";
                    c = kSeverity[std::clamp(e.detail, 0, 2)];
                    break;
                case EventKind::Delivered: text = "entregada #" + std::to_string(e.detail); c = kDelivered; break;
                case EventKind::DeliveredLate: text = "entregada tarde #" + std::to_string(e.detail); c = kLate; break;
                case EventKind::Pickup: text = "recogida #" + std::to_string(e.detail); c = kPickupEvent; break;
                case EventKind::LoadDrop: text = "¡carga caída!"; c = kLoadDrop; break;
                case EventKind::RechargeStart: text = "recargando"; c = kCharger; break;
                case EventKind::RestrictedEntry: text = "zona restringida"; c = kRestricted; break;
                case EventKind::CriticalOverdue: break;
            }
            const int a = static_cast<int>(255 * std::min(1.0f, (kRecentEvent - age) / 1.0f));
            labels_.push_back({e.pos, text, alpha(c, a), 13, -18.0f - 6.0f * age, true});
        }
    }
}

void Renderer::drawObstacles(const WarehouseEnv& env) {
    for (const auto& o : env.obstacles()) {
        const Vec2 dir = fromAngle(o.theta);
        if (o.isWorker()) {
            const float r = o.halfExtents.x;
            disc(o.pos, r + 0.04f, darken(kWorker, 0.5f));
            disc(o.pos, r, kWorker);
            // Cabeza vista desde arriba y una punta hacia donde mira.
            disc(o.pos + dir * (r * 0.15f), r * 0.45f, lighten(kWorker, 0.5f), 12);
            tri(o.pos + dir * (r + 0.12f), o.pos + dir * (r - 0.02f) + perp(dir) * 0.08f,
                o.pos + dir * (r - 0.02f) - perp(dir) * 0.08f, sf::Color::White);
        } else {
            const Vec2 he = o.halfExtents;
            box(o.pos, he + Vec2{0.04f, 0.04f}, o.theta, darken(kForklift, 0.6f));
            box(o.pos, he, o.theta, kForklift);
            // Cabina atrás y horquillas delante.
            box(o.pos - dir * (he.x * 0.35f), {he.x * 0.35f, he.y * 0.7f}, o.theta, darken(kForklift, 0.45f));
            for (float side : {-0.55f, 0.55f})
                box(o.pos + dir * (he.x + 0.22f) + perp(dir) * (he.y * side), {0.22f, 0.04f}, o.theta, sf::Color(90, 90, 95));
        }
        if (o.vel.length() > 0.05f) {
            const Vec2 start = o.pos + normalized(o.vel) * (o.isWorker() ? o.halfExtents.x : o.halfExtents.x + 0.45f);
            arrow(start, start + o.vel * 0.8f, 0.035f, 0.14f, alpha(sf::Color::White, 170));
        }
    }
}

void Renderer::drawRobot(const WarehouseEnv& env, const RenderOptions& opt) {
    const auto& rp = env.config().robot;
    const Pose pose = env.robot().pose();
    const Vec2 dir = fromAngle(pose.theta), side = perp(dir);
    const sf::Color color = opt.robotColor.value_or(sf::Color(50, 120, 230));
    const float t = static_cast<float>(env.time());

    if (opt.intent) {
        // Intención: dónde acabaría el robot en 1.5 s si mantuviera la acción
        // actual de la política (modelo uniciclo, sin rampa ni derrapes).
        const Action a = env.lastAction();
        const float vCmd = a.linear >= 0 ? a.linear * rp.maxForwardSpeed : a.linear * rp.maxReverseSpeed;
        const float wCmd = a.angular * rp.maxAngularSpeed;
        std::vector<Vec2> pts{pose.pos};
        Vec2 p = pose.pos;
        float th = pose.theta;
        for (int i = 0; i < 15; ++i) {
            p += fromAngle(th) * (vCmd * 0.1f);
            th += wCmd * 0.1f;
            pts.push_back(p);
        }
        const sf::Color ic = lighten(color, 0.45f);
        if (distance(pts.front(), pts.back()) > 0.05f) {
            dashed(pts, 0.05f, 0.12f, 0.08f, alpha(ic, 230));
            const Vec2 d = normalized(pts.back() - pts[pts.size() - 2]);
            tri(pts.back() + d * 0.16f, pts.back() + perp(d) * 0.1f, pts.back() - perp(d) * 0.1f, ic);
        } else if (std::abs(wCmd) > 0.1f) {
            // Giro en el sitio: arco alrededor del robot en el sentido pedido.
            const float sgn = wCmd > 0 ? 1.0f : -1.0f;
            ring(pose.pos, rp.radius() + 0.2f, 0.04f, alpha(ic, 220), pose.theta, pose.theta + sgn * 1.4f);
        }
    }

    // Localizador: con la vista alejada el robot mide pocos píxeles; un anillo
    // de tamaño fijo en pantalla permite encontrarlo de un vistazo.
    const float finder = 16.0f * metersPerPixel_;
    if (finder > rp.radius() + 0.15f) ring(pose.pos, finder, 2.0f * metersPerPixel_, alpha(color, 170));

    if (env.charging()) {
        const float pulse = 0.5f + 0.5f * std::sin(t * 5.0f);
        ring(pose.pos, rp.radius() + 0.12f + 0.06f * pulse, 0.06f, alpha(kCharger, 150 + static_cast<int>(100 * pulse)));
    }
    const bool slipping = opt.history && !opt.history->flags.empty() && (opt.history->flags.back() & kSlipping);
    if (slipping) ring(pose.pos, rp.radius() + 0.08f, 0.05f, alpha(kSlip, 220), 0.0f, 2.0f * kPi, 16);

    // Ruedas, chasis con borde oscuro y flecha de frente.
    for (float s : {-1.0f, 1.0f})
        box(pose.pos + side * (s * (rp.halfWidth + 0.01f)), {rp.halfLength * 0.55f, 0.06f}, pose.theta, sf::Color(25, 25, 28));
    box(pose.pos, {rp.halfLength + 0.03f, rp.halfWidth + 0.03f}, pose.theta, darken(color, 0.55f));
    box(pose.pos, {rp.halfLength, rp.halfWidth}, pose.theta, color);
    const Vec2 nose = pose.pos + dir * (rp.halfLength - 0.04f);
    tri(nose, nose - dir * 0.18f + side * 0.13f, nose - dir * 0.18f - side * 0.13f, sf::Color::White);

    if (env.robot().payload() > 0.0f) {
        // Caja de la carga coloreada por estrés: marrón tranquila, naranja/rojo a punto de caer.
        const float s = 0.1f + 0.1f * env.robot().payload() / rp.maxCarryMass;
        const Vec2 c = pose.pos - dir * 0.06f;
        box(c, {s + 0.025f, s + 0.025f}, pose.theta, sf::Color(40, 30, 20));
        box(c, {s, s}, pose.theta, stressColor(env.loadStress()));
        segment(c - dir * s, c + dir * s, 0.02f, alpha(sf::Color(60, 40, 20), 160));
    }

    // Velocidad medida (flecha blanca, longitud = 1 s de avance).
    const Vec2 vel = env.robot().velocity();
    if (vel.length() > 0.05f) arrow(pose.pos, pose.pos + vel, 0.03f, 0.12f, alpha(sf::Color::White, 200));

    // Batería sobre el robot.
    const float bat = env.batteryFraction();
    const Vec2 b0 = pose.pos + Vec2{-0.32f, rp.radius() + 0.14f};
    rect({b0.x - 0.02f, b0.y - 0.02f, 0.68f, 0.12f}, sf::Color(0, 0, 0, 170));
    rect({b0.x, b0.y, 0.64f * bat, 0.08f},
         bat > 0.4f ? sf::Color(80, 200, 100) : bat > 0.2f ? sf::Color(240, 190, 50) : sf::Color(255, 70, 60));
}

void Renderer::drawLabels() {
    if (!font_) return;
    std::vector<sf::Vector2i> pixels;
    pixels.reserve(labels_.size());
    for (const auto& l : labels_) pixels.push_back(window_.mapCoordsToPixel(v(l.world), view_));
    window_.setView(window_.getDefaultView());
    for (size_t i = 0; i < labels_.size(); ++i) {
        const auto& l = labels_[i];
        sf::Text t(*font_, sf::String::fromUtf8(l.text.begin(), l.text.end()), l.size);
        if (l.bold) t.setStyle(sf::Text::Bold);
        const auto b = t.getLocalBounds();
        t.setOrigin({b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
        t.setPosition({static_cast<float>(pixels[i].x), static_cast<float>(pixels[i].y) + l.dyPixels});
        t.setFillColor(l.color);
        t.setOutlineColor(sf::Color(0, 0, 0, l.color.a));
        t.setOutlineThickness(2.0f);
        window_.draw(t);
    }
}

void Renderer::drawScaleBar() {
    // Barra de escala abajo a la izquierda con una longitud "redonda".
    window_.setView(window_.getDefaultView());
    float meters = 0.5f;
    for (float m : {0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f})
        if (m / metersPerPixel_ <= 110.0f) meters = m;
    const float px = meters / metersPerPixel_;
    // Arriba a la izquierda, en el margen que deja el encuadre (abajo están las zonas de entrega).
    const float x0 = 10.0f, y0 = 8.0f;
    sf::RectangleShape bar({px, 4.0f});
    bar.setPosition({x0, y0});
    bar.setFillColor(sf::Color(230, 230, 230));
    bar.setOutlineColor(sf::Color::Black);
    bar.setOutlineThickness(1.0f);
    window_.draw(bar);
    if (font_) {
        char buf[16];
        std::snprintf(buf, sizeof buf, meters < 1.0f ? "%.1f m" : "%.0f m", static_cast<double>(meters));
        sf::Text t(*font_, buf, 11);
        t.setPosition({x0 + px + 6.0f, y0 - 5.0f});
        t.setFillColor(sf::Color(230, 230, 230));
        t.setOutlineColor(sf::Color::Black);
        t.setOutlineThickness(2.0f);
        window_.draw(t);
    }
}

void Renderer::drawHud(const std::vector<std::string>& lines) {
    window_.setView(window_.getDefaultView());
    if (!font_) return;
    const float lineH = 18.0f;
    sf::RectangleShape panel({330.0f, 12.0f + lineH * static_cast<float>(lines.size())});
    panel.setPosition({8.0f, 8.0f});
    panel.setFillColor(sf::Color(0, 0, 0, 150));
    window_.draw(panel);
    float y = 14.0f;
    for (const auto& l : lines) {
        sf::Text t(*font_, sf::String::fromUtf8(l.begin(), l.end()), 14);
        t.setPosition({16.0f, y});
        t.setFillColor(sf::Color(235, 235, 235));
        window_.draw(t);
        y += lineH;
    }
}

}  // namespace wrl
