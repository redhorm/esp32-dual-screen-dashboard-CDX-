#pragma once

#include <math.h>
#include <stdint.h>

// Original, allocation-free animation and game model. All positions are logical
// coordinates; the two displays project the same state independently.
namespace Wow {
enum class Kind : uint8_t { Midnight, Living, Liquid, Garage, Beat };
enum class Control : uint8_t {
  Steer,
  Touch,
  Impulse,
  Explode,
  Orbit,
  Audio,
  Heartbeat,
  Background
};
struct Particle {
  float x = 0, y = 0;
};
struct Snapshot {
  Kind kind = Kind::Midnight;
  uint32_t ms = 0, elapsed = 0, score = 0, best = 0, seed = 1;
  uint16_t hits = 0;
  bool paused = false, done = false, audioActive = false;
  float car = 0, distance = 0, speed = 0, curve = 0;
  float obstacleZ = 1, obstacleLane = 0;
  float gazeX = 0, gazeY = 0;
  uint8_t mood = 0;
  float blink = 0;
  Particle particles[48];
  float energy = 0, yaw = 0, tilt = 0, exploded = 0;
  uint8_t bands[4] = {};
  uint8_t palette = 0;
};

class State {
public:
  Snapshot s;

  void enter(Kind kind, uint32_t now, uint32_t seed = 1) {
    const uint32_t best = s.best;
    s = Snapshot();
    s.kind = kind;
    s.best = best;
    s.seed = seed ? seed : 1;
    rng_ = s.seed;
    last_ = heartbeat_ = steerAt_ = touchAt_ = orbitAt_ = audioAt_ = now;
    initialized_ = true;
    remainder_ = 0;
    steer_ = 0;
    manualPause_ = touched_ = orbited_ = audioReceived_ = false;
    hitObstacle_ = false;
    explodeTarget_ = 0;
    yawTarget_ = tiltTarget_ = touchX_ = touchY_ = 0;
    hitSteps_ = 0;
    blinkAge_ = 0;
    blinkAt_ = 1800 + random() % 1800;
    s.mood = moodKnown_ && (uint32_t)(now - moodAt_) < 5000 ? pendingMood_ : 0;
    for (unsigned i = 0; i < 4; ++i)
      audioTarget_[i] = 0;
    for (unsigned i = 0; i < 48; ++i) {
      const float col = float(i % 8) - 3.5f;
      const float row = float(i / 8) - 2.5f;
      s.particles[i].x = .5f + col * .103f + .012f * sinf(row);
      s.particles[i].y = .53f + row * .105f + .012f * cosf(col);
      vx_[i] = vy_[i] = 0;
    }
    if (kind == Kind::Midnight) {
      s.speed = .55f;
      s.obstacleLane = lane();
    }
  }

  void control(Control control, uint32_t value, uint32_t now) {
    if (!initialized_)
      return;
    tick(now);
    switch (control) {
    case Control::Steer:
      if (s.kind == Kind::Midnight && value <= 2 && !s.done) {
        steer_ = int(value) - 1;
        steerAt_ = heartbeat_ = now;
        s.paused = manualPause_;
      }
      break;
    case Control::Heartbeat:
      if (value > 1)
        break;
      heartbeat_ = now;
      if (s.kind == Kind::Midnight && !s.done) {
        if (value == 1)
          manualPause_ = false;
        s.paused = manualPause_;
      }
      break;
    case Control::Background:
      if (s.kind == Kind::Midnight) {
        s.paused = true;
        manualPause_ = true;
        steer_ = 0;
        heartbeat_ = now - 2501u;
        remainder_ = 0;
      }
      if (s.kind == Kind::Beat) {
        audioReceived_ = false;
        s.audioActive = false;
        s.energy = 0;
        for (unsigned i = 0; i < 4; ++i)
          audioTarget_[i] = s.bands[i] = 0;
      }
      touched_ = orbited_ = false;
      break;
    case Control::Touch:
      if ((s.kind == Kind::Living || s.kind == Kind::Liquid) &&
          value <= 65535) {
        touchX_ = float((value >> 8) & 255) / 255.0f;
        touchY_ = float(value & 255) / 255.0f;
        touchAt_ = now;
        touched_ = true;
        if (s.kind == Kind::Liquid) {
          s.gazeX = touchX_ * 2 - 1;
          s.gazeY = touchY_ * 2 - 1;
        }
      }
      break;
    case Control::Orbit:
      if (s.kind == Kind::Garage && value <= 65535) {
        yawTarget_ = (float((value >> 8) & 255) / 255.0f - .5f) * kTau;
        tiltTarget_ = (.5f - float(value & 255) / 255.0f) * .9f;
        orbitAt_ = now;
        orbited_ = true;
      }
      break;
    case Control::Explode:
      if (s.kind == Kind::Garage && value <= 1)
        explodeTarget_ = float(value);
      break;
    case Control::Audio:
      if (s.kind == Kind::Beat) {
        for (unsigned i = 0; i < 4; ++i)
          audioTarget_[i] = (value >> (i * 8)) & 255;
        audioReceived_ = true;
        audioAt_ = now;
        s.audioActive = true;
      }
      break;
    case Control::Impulse:
      if (s.kind == Kind::Midnight && !s.done) {
        manualPause_ = !s.paused;
        s.paused = manualPause_;
        heartbeat_ = now;
        steer_ = 0;
        remainder_ = 0;
      } else if (s.kind == Kind::Living) {
        blinkAge_ = 20;
        s.blink = sinf(kPi * .125f);
      } else if (s.kind == Kind::Liquid) {
        for (unsigned i = 0; i < 48; ++i) {
          vy_[i] -= .016f + .009f * sinf(s.particles[i].x * kPi);
          vx_[i] += (s.particles[i].x - .5f) * .012f;
        }
        s.energy = 1;
      } else if (s.kind == Kind::Garage) {
        explodeTarget_ = explodeTarget_ > .5f ? 0 : 1;
      } else if (s.kind == Kind::Beat) {
        s.palette = (s.palette + 1) % 4;
      }
      break;
    }
  }

  void react(uint8_t event, uint32_t now) {
    if (event > 4)
      return;
    pendingMood_ = event;
    moodAt_ = now;
    moodKnown_ = true;
    s.mood = event;
  }

  void tick(uint32_t now) {
    if (!initialized_)
      return;
    uint32_t dt = now - last_;
    last_ = now;
    if (moodKnown_ && (uint32_t)(now - moodAt_) >= 5000)
      s.mood = 0;
    if (s.kind == Kind::Midnight) {
      if (s.done)
        return;
      if ((uint32_t)(now - heartbeat_) > 2500)
        s.paused = true;
      if ((uint32_t)(now - steerAt_) >= 500)
        steer_ = 0;
      if (s.paused) {
        remainder_ = 0;
        return;
      }
    }
    // Discard a stalled frame's backlog rather than run an unbounded catch-up.
    if (dt > 100)
      dt = 100;
    remainder_ += dt;
    if (touched_ && (uint32_t)(now - touchAt_) >= 1200)
      touched_ = false;
    if (orbited_ && (uint32_t)(now - orbitAt_) >= 2000)
      orbited_ = false;
    if (s.kind == Kind::Beat &&
        (!audioReceived_ || (uint32_t)(now - audioAt_) >= 500)) {
      s.audioActive = false;
      for (unsigned i = 0; i < 4; ++i)
        audioTarget_[i] = 0;
    }
    while (remainder_ >= 20) {
      remainder_ -= 20;
      s.ms += 20;
      switch (s.kind) {
      case Kind::Midnight:
        midnight();
        break;
      case Kind::Living:
        living();
        break;
      case Kind::Liquid:
        liquid();
        break;
      case Kind::Garage:
        garage();
        break;
      case Kind::Beat:
        beat();
        break;
      }
      if (s.done) {
        remainder_ = 0;
        break;
      }
    }
  }

private:
  static constexpr float kPi = 3.14159265358979323846f;
  static constexpr float kTau = 6.28318530717958647692f;
  uint32_t last_ = 0, heartbeat_ = 0, steerAt_ = 0, touchAt_ = 0;
  uint32_t orbitAt_ = 0, audioAt_ = 0, moodAt_ = 0, rng_ = 1, blinkAt_ = 0;
  uint32_t remainder_ = 0;
  int steer_ = 0;
  bool initialized_ = false, manualPause_ = false, touched_ = false;
  bool orbited_ = false, audioReceived_ = false, hitObstacle_ = false;
  bool moodKnown_ = false;
  uint8_t pendingMood_ = 0, audioTarget_[4] = {};
  uint16_t hitSteps_ = 0, blinkAge_ = 0;
  float touchX_ = 0, touchY_ = 0, yawTarget_ = 0, tiltTarget_ = 0;
  float explodeTarget_ = 0, vx_[48] = {}, vy_[48] = {};

  static float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }
  uint32_t random() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
  }
  float lane() { return (int(random() % 3) - 1) * .62f; }
  void midnight() {
    s.elapsed += 20;
    s.curve = .62f * sinf(s.distance * .017f + float(s.seed % 19)) +
              .25f * sinf(s.distance * .039f);
    const float cruise = .55f + .45f * float(s.elapsed) / 60000.0f;
    s.speed = cruise * (hitSteps_ ? .45f : 1.0f);
    if (hitSteps_)
      --hitSteps_;
    if (fabsf(s.car) > 1)
      s.speed *= .65f;
    s.car = clamp(s.car + float(steer_) * .026f - s.curve * s.speed * .004f,
                  -1.15f, 1.15f);
    s.distance += s.speed * .4f;
    s.obstacleZ -= s.speed * .008f;
    s.energy *= .92f;
    if (s.obstacleZ <= .09f && !hitObstacle_) {
      hitObstacle_ = true;
      if (fabsf(s.car - s.obstacleLane) < .29f) {
        ++s.hits;
        hitSteps_ = 22;
        s.energy = 1;
      }
    }
    if (s.obstacleZ <= .015f) {
      s.obstacleZ = 1.05f + float(random() % 26) / 100.0f;
      s.obstacleLane = lane();
      hitObstacle_ = false;
    }
    const int earned = int(s.distance * 10) - int(s.hits) * 80;
    s.score = earned > 0 ? uint32_t(earned) : 0;
    if (s.elapsed >= 60000) {
      s.elapsed = 60000;
      s.done = true;
      s.speed = 0;
      s.best = s.score > s.best ? s.score : s.best;
    }
  }
  void living() {
    const float t = float(s.ms % 60000) * .001f;
    const float gx = touched_ ? touchX_ * 2 - 1 : .55f * sinf(t * .61f);
    const float gy = touched_ ? touchY_ * 2 - 1 : .25f * sinf(t * .43f);
    s.gazeX += (gx - s.gazeX) * .10f;
    s.gazeY += (gy - s.gazeY) * .10f;
    if (!blinkAge_ && (int32_t)(s.ms - blinkAt_) >= 0)
      blinkAge_ = 20;
    if (blinkAge_) {
      s.blink = sinf(kPi * float(blinkAge_) / 160.0f);
      blinkAge_ += 20;
      if (blinkAge_ >= 160) {
        blinkAge_ = 0;
        s.blink = 0;
        blinkAt_ = s.ms + 2300 + random() % 2400;
      }
    }
    s.energy = .45f + .25f * sinf(t * (s.mood >= 3 ? 9.0f : 4.0f));
  }
  void liquid() {
    const float t = float(s.ms % 60000) * .001f;
    // A small spring/repulsion fluid: deterministic, fixed cost, no buffers on
    // the heap. Soft cohesion keeps the pool together during drag and waves.
    for (unsigned i = 0; i < 48; ++i) {
      for (unsigned j = i + 1; j < 48; ++j) {
        float dx = s.particles[j].x - s.particles[i].x;
        float dy = s.particles[j].y - s.particles[i].y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < .000001f || d2 > .0324f)
          continue;
        const float d = sqrtf(d2);
        const float force =
            d < .078f ? (d - .078f) * .011f : (d - .078f) * .00022f;
        dx = dx / d * force;
        dy = dy / d * force;
        vx_[i] += dx;
        vy_[i] += dy;
        vx_[j] -= dx;
        vy_[j] -= dy;
      }
    }
    float motion = 0;
    for (unsigned i = 0; i < 48; ++i) {
      Particle &p = s.particles[i];
      vx_[i] += (.5f - p.x) * .00017f + .00020f * sinf(t * .77f + p.y * 7.0f);
      vy_[i] += (.57f - p.y) * .00022f + .00018f * cosf(t * .91f + p.x * 6.0f);
      if (touched_) {
        const float dx = touchX_ - p.x, dy = touchY_ - p.y;
        const float gain = .0018f / (1.0f + 8.0f * (dx * dx + dy * dy));
        vx_[i] += dx * gain - dy * .0005f;
        vy_[i] += dy * gain + dx * .0005f;
      }
      vx_[i] = clamp(vx_[i] * .975f, -.035f, .035f);
      vy_[i] = clamp(vy_[i] * .975f, -.035f, .035f);
      p.x += vx_[i];
      p.y += vy_[i];
      if (p.x < .035f || p.x > .965f) {
        p.x = clamp(p.x, .035f, .965f);
        vx_[i] *= -.65f;
      }
      if (p.y < .06f || p.y > .94f) {
        p.y = clamp(p.y, .06f, .94f);
        vy_[i] *= -.65f;
      }
      motion += fabsf(vx_[i]) + fabsf(vy_[i]);
    }
    s.energy = clamp(s.energy * .94f + motion * .06f, .05f, 1.0f);
  }
  void garage() {
    if (orbited_) {
      float difference = yawTarget_ - s.yaw;
      while (difference > kPi)
        difference -= kTau;
      while (difference < -kPi)
        difference += kTau;
      s.yaw += difference * .12f;
      s.tilt += (tiltTarget_ - s.tilt) * .10f;
    } else {
      s.yaw += .009f;
      s.tilt +=
          (-.12f + .08f * sinf(float(s.ms % 60000) * .0007f) - s.tilt) * .04f;
    }
    if (s.yaw > kPi)
      s.yaw -= kTau;
    if (s.yaw < -kPi)
      s.yaw += kTau;
    s.exploded += (explodeTarget_ - s.exploded) * .075f;
    if (fabsf(explodeTarget_ - s.exploded) < .001f)
      s.exploded = explodeTarget_;
    s.energy = s.exploded;
  }
  void beat() {
    unsigned sum = 0;
    for (unsigned i = 0; i < 4; ++i) {
      const int difference = int(audioTarget_[i]) - int(s.bands[i]);
      const float rate = difference > 0 ? .5f : (s.audioActive ? .22f : .38f);
      int next = int(s.bands[i]) + int(float(difference) * rate);
      if (difference < 0 && next == s.bands[i])
        --next;
      if (difference > 0 && next == s.bands[i])
        ++next;
      s.bands[i] = uint8_t(next);
      sum += s.bands[i];
    }
    s.energy = float(sum) / 1020.0f;
  }
};
} // namespace Wow
