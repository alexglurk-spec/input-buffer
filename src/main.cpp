// Input Buffer - version 2
//
// Problem: you click a few frames BEFORE you land, the game ignores the click,
// and you die. Fix: if you click (or hold) while in the air, we keep the jump
// "alive" for a short window so it fires the moment you touch the ground.
//
// Settings now live in a separate window (the tuner app). It writes config.txt;
// this mod re-reads it while you play, so changes apply instantly, no restart.
//
// How it works:
//   1. Hook handleButton, which the game calls on every click / release.
//   2. If the jump button is pressed while in the air, "arm" the buffer.
//   3. If the button is released while armed, hold the release back
//      (the game still thinks you are holding jump, so it jumps on landing).
//   4. A step or two after landing (or when the window runs out), send the
//      release we held back.
//
// Limits: Player 1 only, ground-based modes only (cube, ball, robot, spider),
// no platformer mode, no orbs (the game already has its own orb buffer).

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include "buffer_config.hpp"

using namespace geode::prelude;

class $modify(BufferLayer, GJBaseGameLayer) {
    struct Fields {
        bool armed = false;         // we saw an early click and are waiting to land
        bool releaseHeld = false;   // the player let go early; we are holding that release back
        float timeLeft = 0.f;       // seconds left in the buffer window
        float timeSinceLanding = -1.f; // seconds since landing; -1 = not landed yet
    };

    // Is the buffer on, and is this the kind of click we handle?
    bool shouldHandle(int button, bool isPlayer1) {
        if (!buffercfg::current().enabled) return false;
        if (button != static_cast<int>(PlayerButton::Jump)) return false;
        if (!isPlayer1) return false;
        if (m_isPlatformer) return false;
        return m_player1 != nullptr;
    }

    // Called by the game on every click and every release.
    void handleButton(bool down, int button, bool isPlayer1) {
        if (!this->shouldHandle(button, isPlayer1)) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto player = m_player1;
        auto& fields = m_fields;

        if (down) {
            // Early click: in the air, in a mode that needs the ground to jump.
            bool early = !player->m_isDead && !player->m_isOnGround && !player->isFlying();
            if (early) {
                fields->armed = true;
                fields->releaseHeld = false;
                fields->timeSinceLanding = -1.f;
                fields->timeLeft = buffercfg::current().windowMs / 1000.f;
                buffercfg::bufferedCount()++;
                log::debug("buffer armed ({} ms window)", buffercfg::current().windowMs);
            }
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        // Release.
        if (fields->armed) {
            // Hold the release back; the game still sees jump as held.
            fields->releaseHeld = true;
            return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    // Called by the game every physics step. (In GD 2.2081 and newer this
    // function takes the two extra bool arguments.)
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        // Pick up changes from the tuner window, and tell it we are alive.
        buffercfg::reloadIfChanged();
        buffercfg::writeStatus();

        auto& fields = m_fields;
        if (!fields->armed) return;

        auto player = m_player1;
        bool finished = false;

        if (!player || player->m_isDead) {
            finished = true;
        } else if (player->m_isOnGround && fields->timeSinceLanding < 0.f) {
            // Just landed. Do NOT release yet: the jump itself is processed
            // after this function, so we give it a little time first.
            fields->timeSinceLanding = 0.f;
        } else if (fields->timeSinceLanding >= 0.f) {
            fields->timeSinceLanding += dt;
            if (fields->timeSinceLanding >= 0.009f) finished = true; // about 2 physics ticks
        } else {
            fields->timeLeft -= dt;
            if (fields->timeLeft <= 0.f) finished = true; // window ran out
        }

        if (finished) {
            if (fields->releaseHeld) {
                GJBaseGameLayer::handleButton(false, static_cast<int>(PlayerButton::Jump), true);
            }
            fields->armed = false;
            fields->releaseHeld = false;
            fields->timeSinceLanding = -1.f;
            log::debug("buffer finished");
        }
    }
};
