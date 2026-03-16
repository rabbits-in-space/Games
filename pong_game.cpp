#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <deque>
#include <cmath>
#include <random>
#include <string>

template<typename T>
T clamp(T v, T lo, T hi) { return std::max(lo, std::min(v, hi)); }

float lerp(float a, float b, float t) { return a + (b - a) * t; }

struct Paddle {
    sf::RectangleShape shape;
    float speed = 780.f;
    bool isCPU = false;

    Paddle(sf::Vector2f pos, sf::Vector2f size, bool cpu = false) : isCPU(cpu) {
        shape.setSize(size);
        shape.setOrigin(size.x * 0.5f, size.y * 0.5f);
        shape.setPosition(pos);
        shape.setFillColor(cpu ? sf::Color(180, 220, 255) : sf::Color(255, 220, 180));
    }

    void update(float dt, float windowH, float targetY) {
        float y = shape.getPosition().y;
        if (isCPU) {
            // Simple AI: chase targetY with capped speed
            float dir = (targetY > y) ? 1.f : -1.f;
            if (std::abs(targetY - y) < 8.f) dir = 0.f;
            y += dir * speed * dt;
        }
        shape.setPosition(shape.getPosition().x, clamp(y, shape.getSize().y * 0.5f, windowH - shape.getSize().y * 0.5f));
    }
};

struct Ball {
    sf::CircleShape shape;
    sf::Vector2f vel{ 600.f, 180.f };
    float baseSpeed = 560.f;
    float maxSpeed = 980.f;
    float speed = baseSpeed;
    bool power = false;
    float powerTimer = 0.f;

    std::deque<sf::Vector2f> trail; // positions
    size_t trailMax = 20;

    Ball(float radius, sf::Vector2f pos) {
        shape.setRadius(radius);
        shape.setOrigin(radius, radius);
        shape.setPosition(pos);
        shape.setFillColor(sf::Color::White);
    }

    void reset(sf::Vector2f pos, std::mt19937& rng) {
        shape.setPosition(pos);
        speed = baseSpeed;
        power = false;
        powerTimer = 0.f;
        trail.clear();
        std::uniform_real_distribution<float> ang( -0.35f, 0.35f );
        float a = (rng() % 2 == 0 ? 0.f : 3.14159f) + ang(rng);
        vel = sf::Vector2f(std::cos(a), std::sin(a));
    }

    void update(float dt, float w, float h, float& shake) {
        // Trail
        trail.push_front(shape.getPosition());
        if (trail.size() > trailMax) trail.pop_back();

        sf::Vector2f pos = shape.getPosition();
        pos += vel * speed * dt;
        // Top/bottom bounce
        if (pos.y - shape.getRadius() < 0.f) { pos.y = shape.getRadius(); vel.y = std::abs(vel.y); shake += 2.f; }
        if (pos.y + shape.getRadius() > h) { pos.y = h - shape.getRadius(); vel.y = -std::abs(vel.y); shake += 2.f; }
        shape.setPosition(pos);

        // Power decay
        if (power) {
            powerTimer -= dt;
            if (powerTimer <= 0.f) {
                power = false;
                shape.setOutlineThickness(0.f);
            }
        }
    }
};

int main() {
    const float W = 1280.f, H = 720.f;
    sf::RenderWindow window(sf::VideoMode((unsigned)W, (unsigned)H), "Pong: Slow-Mo & Power", sf::Style::Close);
    window.setVerticalSyncEnabled(true);

    // RNG
    std::random_device rd; std::mt19937 rng(rd());
    std::uniform_real_distribution<float> unit(0.f, 1.f);

    // Entities
    Paddle player({ 40.f, H * 0.5f }, { 24.f, 140.f }, false);
    Paddle cpu({ W - 40.f, H * 0.5f }, { 24.f, 140.f }, true);
    Ball ball(12.f, { W * 0.5f, H * 0.5f });

    // Audio
    sf::SoundBuffer bufHit, bufScore, bufWhoosh;
    bufHit.loadFromFile("assets/hit.wav");
    bufScore.loadFromFile("assets/score.wav");
    bufWhoosh.loadFromFile("assets/whoosh.wav");
    sf::Sound sHit(bufHit), sScore(bufScore), sWhoosh(bufWhoosh);

    // Fonts & HUD
    sf::Font font; font.loadFromFile("assets/Roboto-Regular.ttf");
    sf::Text hud("", font, 22);
    hud.setFillColor(sf::Color::White);

    // Time & slomo
    sf::Clock clock;
    float timeScale = 1.f;
    float targetTimeScale = 1.f;
    bool slomo = false;
    float slomoDur = 1.75f;
    float slomoCooldown = 3.f;
    float slomoTimer = 0.f;
    float slomoCDTimer = 0.f;

    // Screen shake
    float shake = 0.f;
    float shakeDecay = 6.f;

    // Motion blur overlay (fade to black)
    sf::RectangleShape fade({ W, H });
    fade.setFillColor(sf::Color(0, 0, 0, 28)); // higher alpha -> more blur

    // Scoring
    int scoreL = 0, scoreR = 0;

    auto triggerSlomo = [&](float dur = 1.75f) {
        if (slomoCDTimer > 0.f) return;
        slomo = true;
        slomoTimer = dur;
        targetTimeScale = 0.32f; // dramatic but readable
        sWhoosh.setPitch(0.7f);
        sWhoosh.play();
    };

    auto endSlomo = [&]() {
        slomo = false;
        targetTimeScale = 1.f;
        slomoCDTimer = slomoCooldown;
    };

    auto triggerPower = [&](Ball& b) {
        b.power = true;
        b.powerTimer = 1.2f;
        b.speed = clamp(b.speed * 1.8f, b.baseSpeed, b.maxSpeed * 1.2f);
        b.shape.setOutlineColor(sf::Color(255, 80, 30));
        b.shape.setOutlineThickness(4.f);
        shake += 6.f;
        sWhoosh.setPitch(1.1f);
        sWhoosh.play();
        // Optional: brief auto-slow to dramatize the hit
        triggerSlomo(0.6f);
    };

    ball.reset({ W * 0.5f, H * 0.5f }, rng);

    while (window.isOpen()) {
        // Events
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            if (e.type == sf::Event::KeyPressed) {
                if (e.key.code == sf::Keyboard::Escape) window.close();
                if (e.key.code == sf::Keyboard::Space) triggerSlomo(); // manual trigger
            }
        }

        // Delta time
        float dt = clock.restart().asSeconds();

        // Timers
        if (slomo) {
            slomoTimer -= dt;
            if (slomoTimer <= 0.f) endSlomo();
        }
        if (slomoCDTimer > 0.f) slomoCDTimer = std::max(0.f, slomoCDTimer - dt);

        // Smooth time scale
        timeScale = lerp(timeScale, targetTimeScale, clamp(8.f * dt, 0.f, 1.f));

        // CPU balance: don’t slow AI as much — keep a bit of threat
        float cpuTimeScale = lerp(1.f, timeScale, 0.5f); // halfway to slow

        // Input
        float pDir = 0.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) pDir -= 1.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) pDir += 1.f;

        // Update paddles
        float effDtPlayer = dt * timeScale;
        float effDtCPU = dt * cpuTimeScale;
        player.shape.move(0.f, pDir * player.speed * effDtPlayer);
        player.update(0.f, H, player.shape.getPosition().y); // clamp only
        cpu.update(effDtCPU, H, ball.shape.getPosition().y);

        // Update ball
        float effDt = dt * timeScale;
        ball.update(effDt, W, H, shake);

        // Auto slow-mo near score moments (ball close to edges and fast)
        if (!slomo && slomoCDTimer <= 0.f) {
            if ((ball.shape.getPosition().x < 160.f || ball.shape.getPosition().x > W - 160.f) && ball.speed > 700.f) {
                triggerSlomo(0.7f);
            }
        }

        // Collisions with paddles
        auto collidePaddle = [&](Paddle& pad) {
            sf::FloatRect pb = pad.shape.getGlobalBounds();
            sf::FloatRect bb(ball.shape.getPosition().x - ball.shape.getRadius(),
                             ball.shape.getPosition().y - ball.shape.getRadius(),
                             ball.shape.getRadius() * 2.f, ball.shape.getRadius() * 2.f);
            if (pb.intersects(bb)) {
                float padY = pad.shape.getPosition().y;
                float diff = (ball.shape.getPosition().y - padY);
                float norm = clamp(diff / (pad.shape.getSize().y * 0.5f), -1.f, 1.f);
                // Reflect horizontally, set vertical by contact point
                float dirX = (pad.isCPU ? -1.f : 1.f);
                sf::Vector2f newDir(dirX, norm);
                // Normalize
                float len = std::sqrt(newDir.x * newDir.x + newDir.y * newDir.y);
                newDir /= len;

                ball.vel = newDir;
                ball.speed = clamp(ball.speed * 1.06f, ball.baseSpeed, ball.maxSpeed);
                // Slight position nudge to avoid sticking
                ball.shape.move(newDir.x * 6.f, newDir.y * 2.f);

                // Power chance if off-center or random spice
                bool offCenter = std::abs(norm) > 0.6f;
                if (offCenter || unit(rng) > 0.86f) {
                    triggerPower(ball);
                }

                sHit.setPitch(slomo ? 0.8f : 1.0f);
                sHit.play();
                shake += 4.f;
            }
        };

        collidePaddle(player);
        collidePaddle(cpu);

        // Scoring
        bool scored = false;
        if (ball.shape.getPosition().x < -40.f) { scoreR++; scored = true; }
        if (ball.shape.getPosition().x > W + 40.f) { scoreL++; scored = true; }
        if (scored) {
            sScore.setPitch(1.0f);
            sScore.play();
            endSlomo();
            shake = 0.f;
            ball.reset({ W * 0.5f, H * 0.5f }, rng);
            // Nudge serve toward last scorer
            ball.vel.x = (scoreL > scoreR) ? -std::abs(ball.vel.x) : std::abs(ball.vel.x);
        }

        // Visuals: screen shake
        if (shake > 0.f) {
            shake = std::max(0.f, shake - shakeDecay * dt);
        }
        sf::Vector2f camOffset(0.f, 0.f);    
        if (shake > 0.f) {
            std::uniform_real_distribution<float> sdist(-shake, shake);
            camOffset = { sdist(rng), sdist(rng) };
        }

        // Clear with slight translucency for motion blur
        // Strategy: draw previous frame + translucent black rect to leave trails
        window.clear(sf::Color::Black);

        // Draw fade for motion blur
        window.draw(fade);

        // Draw ball trail
        for (size_t i = 0; i < ball.trail.size(); ++i) {
            float t = 1.f - (float)i / (float)ball.trail.size();
            sf::CircleShape dot(ball.shape.getRadius() * (0.5f + 0.5f * t));
            dot.setOrigin(dot.getRadius(), dot.getRadius());
            dot.setPosition(ball.trail[i] + camOffset);
            sf::Color c = ball.power ? sf::Color(255, 120, 60) : sf::Color(200, 220, 255);
            c.a = static_cast<sf::Uint8>(180 * t);
            dot.setFillColor(c);
            window.draw(dot);
        }

        // Draw paddles and ball
        sf::RectangleShape p1 = player.shape;
        sf::RectangleShape p2 = cpu.shape;
        p1.move(camOffset); p2.move(camOffset);
        window.draw(p1); window.draw(p2);
        sf::CircleShape bdraw = ball.shape;
        bdraw.move(camOffset);
        // Power glow
        if (ball.power) {
            sf::CircleShape glow(ball.shape.getRadius() * 2.4f);
            glow.setOrigin(glow.getRadius(), glow.getRadius());
            glow.setPosition(bdraw.getPosition());
            glow.setFillColor(sf::Color(255, 80, 30, 40));
            window.draw(glow, sf::BlendAdd);
        }
        window.draw(bdraw);

        // HUD
        hud.setString(
            "Score: " + std::to_string(scoreL) + " - " + std::to_string(scoreR) +
            "   Speed: " + std::to_string((int)ball.speed) +
            "   Slow-mo: " + (slomo is ? "ON" : "OFF") +
            (slomoCDTimer > 0.f ? " (CD " + std::to_string((int)std::ceil(slomoCDTimer)) + "s)" : "") +
            (ball.power ? "   POWER!" : "")
        );
        hud.setPosition(W * 0.5f - hud.getLocalBounds().width * 0.5f, 16.f);
        window.draw(hud);

        window.display();
    }

    return 0;
}
