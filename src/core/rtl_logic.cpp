#include "rtl_logic.h"

namespace {
constexpr uint32_t TURN_HALF_PERIOD_MS = 300;
constexpr uint32_t CHASE_STEP_MS = 120;
constexpr uint32_t PIT_STEP_MS = 400;

constexpr RtlColor OFF{0, 0, 0};
constexpr RtlColor RED_RUNNING{64, 0, 0};
constexpr RtlColor RED_BRAKE{255, 0, 0};
constexpr RtlColor AMBER_DIM{48, 10, 0};
constexpr RtlColor AMBER_STEADY{176, 44, 0};
constexpr RtlColor AMBER_BRIGHT{255, 92, 0};
constexpr RtlColor WORK_WHITE{255, 255, 220};

struct Zones {
    uint16_t left_red_end;
    uint16_t left_amber_begin;
    uint16_t left_amber_end;
    uint16_t center_begin;
    uint16_t center_end;
    uint16_t right_amber_begin;
    uint16_t right_amber_end;
    uint16_t right_red_begin;
};

Zones zonesFor(uint16_t count) {
    const uint16_t outer = count / 4;
    uint16_t center_size = static_cast<uint16_t>((count - outer * 2) / 2);
    if (center_size == 0) center_size = 1;
    const uint16_t center_begin = static_cast<uint16_t>((count - center_size) / 2);
    const uint16_t center_end = static_cast<uint16_t>(center_begin + center_size);
    return Zones{
        outer,
        outer,
        center_begin,
        center_begin,
        center_end,
        center_end,
        static_cast<uint16_t>(count - outer),
        static_cast<uint16_t>(count - outer),
    };
}

void fillRange(RtlFrame& out, uint16_t begin, uint16_t end,
               const RtlColor& color) {
    if (end > out.count) end = out.count;
    for (uint16_t i = begin; i < end; ++i) out.pixels[i] = color;
}

void setAmberBase(RtlFrame& out, const Zones& z, const RtlColor& color) {
    fillRange(out, z.left_amber_begin, z.left_amber_end, color);
    fillRange(out, z.right_amber_begin, z.right_amber_end, color);
}

void drawChase(RtlFrame& out, const Zones& z, uint32_t now_ms) {
    fillRange(out, z.left_amber_begin, z.left_amber_end, AMBER_DIM);
    fillRange(out, z.right_amber_begin, z.right_amber_end, AMBER_DIM);

    const uint16_t left_count = z.left_amber_end - z.left_amber_begin;
    const uint16_t right_count = z.right_amber_end - z.right_amber_begin;
    const uint32_t step = now_ms / CHASE_STEP_MS;
    if (left_count != 0) {
        out.pixels[z.left_amber_begin + (step % left_count)] = AMBER_BRIGHT;
    }
    if (right_count != 0) {
        // Mirror the chase so both bright points travel away from the centre.
        const uint16_t offset = static_cast<uint16_t>(step % right_count);
        out.pixels[z.right_amber_end - 1 - offset] = AMBER_BRIGHT;
    }
}

void drawPitTest(RtlFrame& out, const Zones& z, uint32_t now_ms) {
    const uint8_t phase = static_cast<uint8_t>((now_ms / PIT_STEP_MS) % 5U);
    if (phase == 0 || phase == 4) {
        fillRange(out, 0, z.left_red_end, RED_BRAKE);
        fillRange(out, z.right_red_begin, out.count, RED_BRAKE);
    }
    if (phase == 1 || phase == 4)
        fillRange(out, z.left_amber_begin, z.left_amber_end, AMBER_BRIGHT);
    if (phase == 2 || phase == 4)
        fillRange(out, z.center_begin, z.center_end, WORK_WHITE);
    if (phase == 3 || phase == 4)
        fillRange(out, z.right_amber_begin, z.right_amber_end, AMBER_BRIGHT);
}

RtlFunction primaryFor(const RtlCommandState& state) {
    const bool hazard = state.hazard || (state.left && state.right);
    if (state.brake) return RtlFunction::BRAKE;
    if (hazard) return RtlFunction::HAZARD;
    if (state.left) return RtlFunction::LEFT;
    if (state.right) return RtlFunction::RIGHT;
    if (state.reverse_work) return RtlFunction::REVERSE_WORK;
    if (state.dust_chase) return RtlFunction::DUST_CHASE;
    if (state.pit_test) return RtlFunction::PIT_TEST;
    return RtlFunction::RUNNING;
}
}  // namespace

bool RtlFrame::operator==(const RtlFrame& rhs) const {
    if (count != rhs.count || profile != rhs.profile ||
        primary != rhs.primary || command_timed_out != rhs.command_timed_out)
        return false;
    for (uint16_t i = 0; i < count; ++i) {
        if (pixels[i] != rhs.pixels[i]) return false;
    }
    return true;
}

RtlLogic::RtlLogic(RtlProfile profile, uint16_t led_count,
                   uint32_t command_timeout_ms) {
    configure(profile, led_count, command_timeout_ms);
}

void RtlLogic::configure(RtlProfile profile, uint16_t led_count,
                         uint32_t command_timeout_ms) {
    _profile = profile;
    _led_count = clampLedCount(led_count);
    _command_timeout_ms = command_timeout_ms;
    clearCommand();
}

void RtlLogic::submit(const RtlCommandState& state, uint32_t now_ms) {
    if (!_has_command || state != _command) _command_started_ms = now_ms;
    _command = state;
    _last_command_ms = now_ms;
    _has_command = true;
}

void RtlLogic::activate(RtlFunction function, uint32_t now_ms) {
    submit(stateFor(function), now_ms);
}

void RtlLogic::clearCommand() {
    _command = failSafeState();
    _last_command_ms = 0;
    _command_started_ms = 0;
    _has_command = false;
}

bool RtlLogic::commandTimedOut(uint32_t now_ms) const {
    return !_has_command ||
           static_cast<uint32_t>(now_ms - _last_command_ms) >
               _command_timeout_ms;
}

RtlFrame RtlLogic::frame(uint32_t now_ms) const {
    RtlFrame out;
    out.count = _led_count;
    out.profile = _profile;
    out.command_timed_out = commandTimedOut(now_ms);
    for (uint16_t i = 0; i < out.count; ++i) out.pixels[i] = OFF;

    const RtlCommandState state = out.command_timed_out
        ? failSafeState() : _command;
    const uint32_t animation_ms = out.command_timed_out
        ? 0U : static_cast<uint32_t>(now_ms - _command_started_ms);
    const Zones z = zonesFor(out.count);
    const bool hazard = state.hazard || (state.left && state.right);
    const bool flash_on = ((animation_ms / TURN_HALF_PERIOD_MS) & 1U) == 0U;
    const RtlColor flash_low =
        (_profile == RtlProfile::ULTRA4_4400 && state.running)
            ? AMBER_DIM : OFF;
    const bool has_live_signal = state.brake || state.left || state.right ||
                                 state.hazard || state.reverse_work ||
                                 state.dust_chase;
    const bool pit_test = state.pit_test && !has_live_signal;

    out.primary = primaryFor(state);
    if (pit_test) {
        drawPitTest(out, z, animation_ms);
        return out;
    }

    if (state.running || state.brake) {
        const RtlColor red = state.brake ? RED_BRAKE : RED_RUNNING;
        fillRange(out, 0, z.left_red_end, red);
        fillRange(out, z.right_red_begin, out.count, red);
    }

    // The 4400 profile keeps a rear amber marker illuminated whenever the
    // running state is active.  RTL-S leaves amber dark until commanded.
    if (_profile == RtlProfile::ULTRA4_4400 && state.running)
        setAmberBase(out, z, AMBER_STEADY);

    // Dust/chase has lower priority than turn and hazard signalling.
    if (state.dust_chase && !state.left && !state.right && !state.hazard)
        drawChase(out, z, animation_ms);

    if (hazard) {
        // In the 4400 profile the low phase stays visibly amber instead of
        // going black, preserving the always-on race marker.
        setAmberBase(out, z, flash_on ? AMBER_BRIGHT : flash_low);
    } else {
        if (state.left)
            fillRange(out, z.left_amber_begin, z.left_amber_end,
                      flash_on ? AMBER_BRIGHT : flash_low);
        if (state.right)
            fillRange(out, z.right_amber_begin, z.right_amber_end,
                      flash_on ? AMBER_BRIGHT : flash_low);
    }

    if (state.reverse_work)
        fillRange(out, z.center_begin, z.center_end, WORK_WHITE);

    return out;
}

RtlCommandState RtlLogic::stateFor(RtlFunction function) {
    RtlCommandState out;
    switch (function) {
        case RtlFunction::BRAKE:
            out.brake = true;
            break;
        case RtlFunction::LEFT:
            out.left = true;
            break;
        case RtlFunction::RIGHT:
            out.right = true;
            break;
        case RtlFunction::HAZARD:
            out.hazard = true;
            break;
        case RtlFunction::REVERSE_WORK:
            out.reverse_work = true;
            break;
        case RtlFunction::DUST_CHASE:
            out.dust_chase = true;
            break;
        case RtlFunction::PIT_TEST:
            out.running = false;
            out.pit_test = true;
            break;
        default:
            break;
    }
    return out;
}

RtlCommandState RtlLogic::stateForEsc(EscDriveState state) {
    RtlCommandState out;
    switch (state) {
        case EscDriveState::BRAKING:
            out.brake = true;
            break;
        case EscDriveState::REVERSE_ARMED:
            // Neutral arming is neither braking nor reverse.  Running markers
            // stay on, while white remains reserved for confirmed reverse.
            break;
        case EscDriveState::REVERSE:
            out.reverse_work = true;
            break;
        default:
            break;
    }
    return out;
}

RtlCommandState RtlLogic::mergeEsc(const RtlCommandState& base,
                                   EscDriveState state) {
    RtlCommandState out = base;
    // ESC owns only the mutually-exclusive brake/reverse pair.  Manual turn,
    // hazard and dust commands remain composable and keep their priority.
    out.brake = false;
    out.reverse_work = false;
    const RtlCommandState drive = stateForEsc(state);
    out.brake = drive.brake;
    out.reverse_work = drive.reverse_work;
    return out;
}

const char* RtlLogic::functionName(RtlFunction function) {
    switch (function) {
        case RtlFunction::BRAKE:        return "brake";
        case RtlFunction::LEFT:         return "left";
        case RtlFunction::RIGHT:        return "right";
        case RtlFunction::HAZARD:       return "hazard";
        case RtlFunction::REVERSE_WORK: return "reverse_work";
        case RtlFunction::DUST_CHASE:   return "dust_chase";
        case RtlFunction::PIT_TEST:     return "pit_test";
        default:                        return "running";
    }
}

const char* RtlLogic::profileName(RtlProfile profile) {
    return profile == RtlProfile::ULTRA4_4400 ? "ultra4_4400" : "rtl_s";
}

uint16_t RtlLogic::clampLedCount(uint16_t count) {
    if (count < RTL_LOGIC_MIN_PIXELS) return RTL_LOGIC_MIN_PIXELS;
    if (count > RTL_LOGIC_MAX_PIXELS) return RTL_LOGIC_MAX_PIXELS;
    return count;
}

RtlCommandState RtlLogic::failSafeState() {
    RtlCommandState out;
    out.running = true;
    return out;
}
