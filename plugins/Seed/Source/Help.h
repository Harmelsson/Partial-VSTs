#pragma once
// Tooltip texts (hover a control for ~0.6 s). User v0.6: "What is bend range for? What does Life do?
// What is Chaos / Formant / Shimmer?" — every unclear control gets a plain-language explanation here.
#include <juce_core/juce_core.h>
#include "SeedParams.h"

namespace seed
{
inline juce::String helpFor (const juce::String& id)
{
    using namespace ids;
    static const std::map<juce::String, juce::String> m = {
        { fundamental, "The pitch (in Hz) of the base key - the unison. Every note of the scale is measured from it." },
        { seed, "The stretch. 2 = ordinary harmonics and a normal 2:1 octave. 2.5 stretches every partial AND the 'octave' to 2.5:1, 3 gives a tritave... Try moving it while holding a note." },
        { partials, "How many harmonics the sound is built from (1-64)." },
        { rolloff, "How fast the higher partials get quieter. 1 = like a saw (1/n), 0 = all equally loud, 3 = very dull." },
        { minDip, "Dissonance method: dips of the curve weaker than this (in % of the curve's swing) are not kept as scale notes." },
        { tolerance, "Consonance method: how close (in cents) two partials must be to count as 'the same pitch'." },
        { minShared, "Consonance method: how many partials two notes must share to be kept." },
        { edoSteps, "EDO / Seed-EDO: how many equal steps the period is cut into (1-120). 12 is a piano; scroll up for many more notes, then switch off the ones you don't want in the Scale table." },
        { baseKey, "Which MIDI key plays the FIRST note (the unison) of the scale. Keys above climb through the scale, keys below go down. Default C4 (60). Set it to the key you start on so the first note you see in your piano roll is the plugin's first note. Not used in 12-EDO key mode (normal tuning)." },
        { keyMode, "12-EDO = normal piano tuning. Seed-12 = 12 keys per seed 'octave'. Scale = each key is the next note of the calculated scale (from the base key). Magnet = Seed-12 keys pulled towards the nearest scale note." },
        { magnet, "Magnet mode: how strongly each key is pulled to the nearest scale note (0 % = plain stretched keys, 100 % = snapped onto the scale)." },
        { life, "Loudness follows how strong each scale note is. A note in a shallow valley of the dissonance curve (weak) is played quieter, a deep one at full level. 0 % = all notes equally loud. While you hold keys and change the seed, notes can fade in or away as their valleys deepen or flatten." },
        { glide, "How long a held note takes to slide to its new pitch when the scale or seed changes under it (and between pitch changes)." },
        { velocity, "How much your key velocity changes the loudness (0 % = every note equally loud)." },
        { gain, "Master output level (before the always-on safety limiter)." },
        { pitch, "Transposes everything in cents (100 cents = a semitone). Live." },
        { fshift, "Adds this many Hz to EVERY partial (not a multiplication): the sound turns inharmonic, like a ring-mod / frequency shifter. Live." },
        { bendRange, "How far the pitch-bend wheel of your keyboard bends, in semitones (2 = plus/minus 2 semitones). 0 = wheel does nothing." },
        { chordSize, "One key plays this many notes at once." },
        { chordStep, "Distance between the chord's keys. In Scale mode this counts scale degrees, so you stack the calculated scale's own notes." },
        { drone, "Plays a constant note without any MIDI - handy for testing the sound." },
        { droneNote, "Which MIDI key the drone plays." },
        { attack, "Time to fade in (0 ms = instant)." },
        { decay, "Time to fall from full level to the sustain level." },
        { sustain, "Level held while the key is down." },
        { release, "Time to fade out after you let go (the fall of 60 dB). The tail then keeps fading smoothly all the way to silence - it is never cut off." },
        { tilt, "Brightens (+) or darkens (-) the sound: each octave up makes the partials this many dB louder or quieter." },
        { oddEven, "Balance between odd and even partials. + keeps the odd ones (hollow, clarinet-like), - keeps the even ones." },
        { spread, "Scatters the partials across the stereo field (each has its own fixed place)." },
        { shimmer, "Every partial's loudness slowly pulses on its own (each at a slightly different speed and phase), so the sound sparkles and shifts. It is NOT a reverb - it is a per-partial tremolo." },
        { shimmerRate, "How fast the shimmer pulses." },
        { formFreq, "Formant: a fixed, vowel-like bump in the spectrum at this frequency. Whatever note you play, the partials that land near it get louder (or quieter)." },
        { formGain, "Formant: how much the bump boosts (+) or cuts (-) the partials near its frequency." },
        { formWidth, "Formant: how wide the bump is, in octaves." },
        { wander, "CHAOS: every partial drifts up and down in pitch on its own random path, by up to this many cents. The sound becomes alive and slightly inharmonic." },
        { wanderRate, "CHAOS: how fast the partials wander." },
        { scramble, "CHAOS: partials swap loudness with other partials. 0 % = normal spectrum, 100 % = every partial has another partial's loudness." },
    };
    if (auto it = m.find (id); it != m.end()) return it->second;
    return {};
}
} // namespace seed
