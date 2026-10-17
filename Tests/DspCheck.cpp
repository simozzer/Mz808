// Mz808 - an emulation of the TR-808 Rhythm Composer
// Created by Simon Moscrop
//
// This program is free software: you can redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software Foundation, version 3.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for the full terms.

//
// The engine, checked on its own: every sound's pitch and character, accent, the decays and
// the knobs that set them, the hats' choke, the shared circuits, independence from the
// sample rate, and that no setting of any control makes it blow up. Links no JUCE.
//
//     DspCheck                 run the checks
//     DspCheck --wav <dir>     and also write demo renders there to listen to
//

#define _CRT_SECURE_NO_WARNINGS   // fopen, for the demo renders

#include "Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace tr808;

namespace
{

double rate = 48000.0;
int failures = 0;

void check(bool ok, const char* what, const std::string& detail)
{
    std::printf("  %s  %-46s %s\n", ok ? "ok  " : "FAIL", what, detail.c_str());
    if (! ok) ++failures;
}

std::string fmt(const char* f, double a, double b = 0, double c = 0)
{
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a, b, c);
    return buf;
}

// every level up, so each sound is heard at full
Settings loud()
{
    Settings s;
    s.bdLevel = s.sdLevel = s.ltLevel = s.mtLevel = s.htLevel = 10;
    s.rsLevel = s.cpLevel = s.cbLevel = s.cyLevel = s.ohLevel = s.chLevel = 10;
    s.volume = 7;
    return s;
}

std::vector<float> hit(int voice, const Settings& s, double seconds, bool accent = false)
{
    Engine e;
    e.prepare(rate);
    e.setSettings(s);
    e.trigger(voice, accent);
    std::vector<float> v((size_t) (seconds * rate));
    e.render(v.data(), (int) v.size());
    return v;
}

size_t at(double seconds) { return (size_t) (seconds * rate); }

double peak(const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > v.size()) to = v.size();
    double p = 0;
    for (size_t i = from; i < to; ++i) p = std::max(p, (double) std::fabs(v[i]));
    return p;
}

double rms(const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > v.size()) to = v.size();
    double s = 0;
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return std::sqrt(s / (double) std::max<size_t>(1, to - from));
}

bool finite(const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite(x)) return false;
    return true;
}

// Frequency from rising zero crossings, interpolated, between two times.
double frequency(const std::vector<float>& v, double from, double to)
{
    double first = -1, last = -1;
    int count = 0;
    const size_t end = std::min(v.size(), at(to));

    for (size_t i = at(from) + 1; i < end; ++i)
    {
        if (v[i - 1] < 0 && v[i] >= 0)
        {
            const double t = (double) (i - 1) + v[i - 1] / (double) (v[i - 1] - v[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    }

    return count > 0 ? count * rate / (last - first) : 0.0;
}

// The same, with the noise taken off first - two RC low passes at `below` - for drums with
// a noise layer over the ring.
double ringFrequency(const std::vector<float>& v, double from, double to, double below)
{
    const float c = 1.0f - std::exp(-2.0f * (float) pi * (float) below / (float) rate);
    std::vector<float> f(v.size());
    float a = 0, b = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        a += (v[i] - a) * c;
        b += (a - b) * c;
        f[i] = b;
    }
    return frequency(f, from, to);
}

// The length of the first half-cycle, from the moment the sound starts.
double firstHalfCycle(const std::vector<float>& v)
{
    const double threshold = peak(v) * 0.02;
    size_t start = 0;
    while (start < v.size() && std::fabs(v[start]) < threshold) ++start;
    const bool positive = v[start] > 0;
    size_t i = start;
    while (i < v.size() && (v[i] > 0) == positive) ++i;
    return (double) (i - start) / rate;
}

// The level at one frequency, by Goertzel, over a stretch.
double toneAt(const std::vector<float>& v, double hz, double from, double to)
{
    const double w = 2.0 * pi * hz / rate, c = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    const size_t a = at(from), b = std::min(v.size(), at(to));
    for (size_t i = a; i < b; ++i)
    {
        const double s0 = v[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / (double) std::max<size_t>(1, b - a);
}

// How bright: the level of the first difference against the level itself.
double brightness(const std::vector<float>& v, size_t from, size_t to)
{
    double d = 0, s = 0;
    for (size_t i = from + 1; i < std::min(to, v.size()); ++i)
    {
        d += (double) (v[i] - v[i - 1]) * (v[i] - v[i - 1]);
        s += (double) v[i] * v[i];
    }
    return s > 0 ? std::sqrt(d / s) : 0;
}

// Seconds until the level, in 5 ms windows, stays below the peak window by `db`.
double ringTime(const std::vector<float>& v, double db)
{
    const size_t w = at(0.005);
    std::vector<double> win;
    for (size_t i = 0; i + w <= v.size(); i += w) win.push_back(rms(v, i, i + w));
    const double top = *std::max_element(win.begin(), win.end());
    const double floor = top * std::pow(10.0, -db / 20.0);
    size_t lastAbove = 0;
    for (size_t i = 0; i < win.size(); ++i) if (win[i] > floor) lastAbove = i;
    return (double) (lastAbove + 1) * 0.005;
}

//------------------------------------------------------------------------------ the checks

void everySound()
{
    std::printf("\nevery sound\n");

    for (int voice = 0; voice < Voice::count; ++voice)
    {
        const auto& info = voiceInfo(voice);
        const auto plain = hit(voice, loud(), 4.0);
        const auto accented = hit(voice, loud(), 4.0, true);

        const double p = peak(plain), pa = peak(accented);
        const double tail = rms(plain, at(3.5));
        const bool ok = finite(plain) && finite(accented) && p > 0.05 && pa < 1.5 && tail < 1.0e-4;

        std::string what = std::string(info.code) + " " + info.name + ": sounds, rings out";
        check(ok, what.c_str(), fmt("peak %.2f, accented %.2f, tail %.1e", p, pa, tail));

        what = std::string(info.code) + " accent is louder";
        check(pa > p * 1.15, what.c_str(), fmt("%+.1f dB", 20 * std::log10(pa / p)));
    }
}

void accentFromVelocity()
{
    std::printf("\naccent from velocity\n");

    auto played = [] (int velocity)
    {
        Engine e;
        e.prepare(rate);
        e.setSettings(loud());
        e.noteOn(38, velocity);
        std::vector<float> v(at(0.3));
        e.render(v.data(), (int) v.size());
        return peak(v);
    };

    const double soft = played(1), at100 = played(100), at101 = played(101), hard = played(127);
    check(at100 == soft, "100 and below is unaccented", fmt("peak %.3f at 1, %.3f at 100", soft, at100));
    check(at101 > at100 * 1.15 && at101 == hard, "above 100 is accented", fmt("peak %.3f at 101, %.3f at 127", at101, hard));
}

void bassDrum()
{
    std::printf("\nthe bass drum\n");

    const auto v = hit(Voice::BD, loud(), 2.0);
    const double tailHz = frequency(v, 0.08, 0.3);
    check(std::fabs(tailHz - 56.0) < 3.0, "rings at Roland's 56 Hz", fmt("%.1f Hz", tailHz));

    // the attack: the bridged-T's centre lifted more than an octave for a few milliseconds
    // A half-cycle at 56 Hz is 8.9 ms; lifted for the first few, the first is much shorter.
    const double half = firstHalfCycle(v);
    check(half < 0.0065, "the attack is lifted above the body", fmt("first half-cycle %.1f ms (8.9 at 56 Hz)", half * 1000));

    // the sigh: a little sharp early on, settling
    const double early = frequency(v, 0.012, 0.06), late = frequency(v, 0.3, 0.6);
    check(early > late * 1.003 && early < late * 1.1, "the pitch sighs down",
          fmt("%.2f Hz early, %.2f Hz late", early, late));

    Settings s = loud();
    s.bdDecay = 0;  const double shortest = ringTime(hit(Voice::BD, s, 6.0), 40);
    s.bdDecay = 10; const double longest = ringTime(hit(Voice::BD, s, 6.0), 40);
    check(shortest < 0.6 && longest > 2.0, "DECAY: a thud to a long boom", fmt("%.2f s to %.2f s (-40 dB)", shortest, longest));

    s = loud();
    s.bdTone = 0;  const double dark = brightness(hit(Voice::BD, s, 0.05), 0, at(0.01));
    s.bdTone = 10; const double click = brightness(hit(Voice::BD, s, 0.05), 0, at(0.01));
    check(click > dark * 1.5, "TONE brings in the click", fmt("attack brightness %.3f -> %.3f", dark, click));

    // accent: louder, and blooms - the clipped feedback eats the top of the bigger ring
    const auto a = hit(Voice::BD, loud(), 2.0, true);
    const double plainRatio = rms(v, at(0.4), at(0.5)) / rms(v, at(0.02), at(0.05));
    const double accentRatio = rms(a, at(0.4), at(0.5)) / rms(a, at(0.02), at(0.05));
    check(accentRatio < plainRatio, "an accent's top decays faster", fmt("tail/attack %.3f plain, %.3f accented", plainRatio, accentRatio));
}

void snare()
{
    std::printf("\nthe snare\n");

    Settings s = loud();
    s.sdSnappy = 0;

    s.sdTone = 0;
    auto v = hit(Voice::SD, s, 0.5);
    const double lowAt0 = toneAt(v, 238, 0.005, 0.06), highAt0 = toneAt(v, 476, 0.005, 0.06);
    s.sdTone = 10;
    v = hit(Voice::SD, s, 0.5);
    const double lowAt10 = toneAt(v, 238, 0.005, 0.06), highAt10 = toneAt(v, 476, 0.005, 0.06);
    check(highAt0 / lowAt0 < highAt10 / lowAt10 * 0.5, "TONE crosses 238 Hz over to 476 Hz",
          fmt("476/238 %.2f at 0, %.2f at 10", highAt0 / lowAt0, highAt10 / lowAt10));

    s = loud();
    s.sdSnappy = 0;  const double dry = brightness(hit(Voice::SD, s, 0.3), at(0.01), at(0.1));
    s.sdSnappy = 10; const double snares = brightness(hit(Voice::SD, s, 0.3), at(0.01), at(0.1));
    check(snares > dry * 3, "SNAPPY brings in the snares", fmt("brightness %.3f -> %.3f", dry, snares));
}

void tomsAndCongas()
{
    std::printf("\nthe toms and congas\n");

    struct Case { int voice; double hz; double from, to; };
    const Case cases[] = {
        { Voice::LT,  90, 0.12, 0.35 }, { Voice::MT, 135, 0.12, 0.30 }, { Voice::HT, 185, 0.12, 0.28 },
        { Voice::LC, 185, 0.05, 0.15 }, { Voice::MC, 250, 0.05, 0.13 }, { Voice::HC, 400, 0.05, 0.12 },
    };

    for (const auto& c : cases)
    {
        const auto v = hit(c.voice, loud(), 0.5);
        const double f = ringFrequency(v, c.from, c.to, c.hz * 1.5);
        const std::string what = std::string(voiceInfo(c.voice).name) + " at TUNING 5";
        check(std::fabs(f / c.hz - 1.0) < 0.03, what.c_str(), fmt("%.1f Hz (nominal %.0f)", f, c.hz));
    }

    // the sweep down at the start
    {
        const auto v = hit(Voice::LT, loud(), 0.5);
        const double start = ringFrequency(v, 0.0, 0.035, 200), settled = ringFrequency(v, 0.12, 0.35, 200);
        check(start > settled * 1.05, "a tom's pitch drops as it strikes", fmt("%.1f Hz -> %.1f Hz", start, settled));
    }

    Settings s = loud();
    s.ltTuning = 0;  const double down = ringFrequency(hit(Voice::LT, s, 0.5), 0.12, 0.35, 150);
    s.ltTuning = 10; const double up = ringFrequency(hit(Voice::LT, s, 0.5), 0.12, 0.35, 150);
    check(std::fabs(up / down - std::sqrt(2.0)) < 0.05, "TUNING spans half an octave", fmt("%.1f Hz to %.1f Hz", down, up));

    // one circuit: striking the conga takes the tom over
    {
        Engine e;
        e.prepare(rate);
        e.setSettings(loud());
        e.trigger(Voice::LT, false);
        std::vector<float> v(at(0.5));
        e.render(v.data(), (int) at(0.05));
        e.trigger(Voice::LC, false);
        e.render(v.data() + at(0.05), (int) (v.size() - at(0.05)));
        const double f = frequency(v, 0.11, 0.2);
        check(std::fabs(f / 185.0 - 1.0) < 0.03, "LC takes over LT's circuit", fmt("%.1f Hz after, no 90 Hz left", f));
    }
}

void rimAndClaves()
{
    std::printf("\nrim shot, claves, clap, maracas, cow bell\n");

    const auto cl = hit(Voice::CL, loud(), 0.2);
    const double f = frequency(cl, 0.001, 0.03);
    check(std::fabs(f / 2500 - 1) < 0.03, "claves ring at 2.5 kHz", fmt("%.0f Hz", f));
    check(ringTime(cl, 40) < 0.12, "claves are short", fmt("%.3f s (-40 dB)", ringTime(cl, 40)));

    const auto rs = hit(Voice::RS, loud(), 0.2);
    const double r455 = toneAt(rs, 455, 0, 0.02), r1667 = toneAt(rs, 1667, 0, 0.02), rOff = toneAt(rs, 1000, 0, 0.02);
    check(r455 > rOff && r1667 > rOff, "rim shot: 455 Hz and 1667 Hz", fmt("%.4f, %.4f against %.4f at 1 kHz", r455, r1667, rOff));
    check(ringTime(rs, 40) < 0.06, "rim shot is a crack", fmt("%.3f s (-40 dB)", ringTime(rs, 40)));

    // the clap: separate claps in its first few tens of milliseconds
    {
        const auto cp = hit(Voice::CP, loud(), 0.6);
        const size_t w = at(0.001);
        std::vector<double> env;
        for (size_t i = 0; i + w <= at(0.05); i += w) env.push_back(rms(cp, i, i + w));
        int bursts = 0;
        for (size_t i = 1; i + 1 < env.size(); ++i)
            if (env[i] > env[i - 1] * 1.8 && env[i] > peak(cp) * 0.08) ++bursts;
        check(bursts >= 3, "hand clap: several hands, then a tail", fmt("%.0f bursts in 50 ms, tail %.2f s", bursts, ringTime(cp, 40)));
    }

    {
        const auto ma = hit(Voice::MA, loud(), 0.3);
        check(brightness(ma, 0, at(0.05)) > 1.0, "maracas are bright", fmt("brightness %.2f", brightness(ma, 0, at(0.05))));
        check(ringTime(ma, 40) < 0.12, "maracas are short", fmt("%.3f s", ringTime(ma, 40)));
    }

    {
        const auto cb = hit(Voice::CB, loud(), 0.5);
        const double a = toneAt(cb, 540, 0.0, 0.1), b = toneAt(cb, 800, 0.0, 0.1), off = toneAt(cb, 670, 0.0, 0.1);
        check(a > off * 4 && b > off * 4, "cow bell: 540 Hz and 800 Hz", fmt("%.4f, %.4f against %.4f between", a, b, off));
    }
}

void metal()
{
    std::printf("\ncymbal and hi-hats\n");

    Settings s = loud();
    s.cyDecay = 0;  const double shortest = ringTime(hit(Voice::CY, s, 4.0), 40);
    s.cyDecay = 10; const double longest = ringTime(hit(Voice::CY, s, 4.0), 40);
    check(longest > shortest * 2.5, "cymbal DECAY", fmt("%.2f s to %.2f s (-40 dB)", shortest, longest));

    s = loud();
    s.cyTone = 0;  const double dull = brightness(hit(Voice::CY, s, 0.3), 0, at(0.1));
    s.cyTone = 10; const double bright = brightness(hit(Voice::CY, s, 0.3), 0, at(0.1));
    check(bright > dull * 1.1, "cymbal TONE brightens", fmt("%.3f -> %.3f", dull, bright));

    s = loud();
    s.ohDecay = 0;  const double ohShort = ringTime(hit(Voice::OH, s, 2.0), 40);
    s.ohDecay = 10; const double ohLong = ringTime(hit(Voice::OH, s, 2.0), 40);
    const double ch = ringTime(hit(Voice::CH, loud(), 1.0), 40);
    check(ch < ohShort && ohLong > ohShort * 4, "closed < open; OH DECAY", fmt("CH %.3f s; OH %.3f s to %.3f s", ch, ohShort, ohLong));

    // the choke: an open hat, closed 50 ms in
    {
        Engine e;
        e.prepare(rate);
        e.setSettings(loud());
        e.trigger(Voice::OH, false);
        std::vector<float> v(at(0.4));
        e.render(v.data(), (int) at(0.05));
        e.trigger(Voice::CH, false);
        e.render(v.data() + at(0.05), (int) (v.size() - at(0.05)));
        const auto open = hit(Voice::OH, loud(), 0.4);
        const double left = rms(v, at(0.15), at(0.25)), ringing = rms(open, at(0.15), at(0.25));
        check(left < ringing * 0.02, "CH chokes OH", fmt("%.1f dB of the open hat left", 20 * std::log10(left / ringing + 1e-12)));
    }

    // the metal is free-running: two hits are never the same
    {
        Engine e;
        e.prepare(rate);
        e.setSettings(loud());
        std::vector<float> a(at(0.05)), gap(at(0.4)), b(at(0.05));
        e.trigger(Voice::CY, false);
        e.render(a.data(), (int) a.size());
        e.render(gap.data(), (int) gap.size());
        e.trigger(Voice::CY, false);
        e.render(b.data(), (int) b.size());
        double diff = 0;
        for (size_t i = 0; i < a.size(); ++i) diff += std::fabs(a[i] - b[i]);
        check(diff / (double) a.size() > 0.01, "no two cymbals the same", fmt("mean difference %.3f", diff / (double) a.size()));
    }
}

void sampleRates()
{
    std::printf("\nother sample rates\n");

    const double saved = rate;
    for (double r : { 44100.0, 96000.0, 192000.0 })
    {
        rate = r;
        const double bdHz = frequency(hit(Voice::BD, loud(), 0.5), 0.08, 0.3);
        const double clHz = frequency(hit(Voice::CL, loud(), 0.1), 0.001, 0.03);
        const double ltHz = ringFrequency(hit(Voice::LT, loud(), 0.5), 0.12, 0.35, 135);
        const auto cy = hit(Voice::CY, loud(), 0.5);
        const bool ok = std::fabs(bdHz - 56) < 3 && std::fabs(clHz / 2500 - 1) < 0.03 && std::fabs(ltHz / 90 - 1) < 0.03
                     && finite(cy) && peak(cy) < 1.5;
        check(ok, fmt("%.0f Hz", r).c_str(), fmt("BD %.1f Hz, CL %.0f Hz, LT %.1f Hz", bdHz, clHz, ltHz));
    }
    rate = saved;
}

void stability()
{
    std::printf("\nstability\n");

    // every knob at each end, every sound accented at once, again and again
    double worst = 0;
    bool ok = true;
    for (int corner = 0; corner < 4; ++corner)
    {
        Settings s = loud();
        const float a = (corner & 1) ? 10.0f : 0.0f, b = (corner & 2) ? 10.0f : 0.0f;
        s.accent = 10; s.volume = 10;
        s.bdTone = a; s.bdDecay = b; s.sdTone = a; s.sdSnappy = b;
        s.ltTuning = s.mtTuning = s.htTuning = a; s.cyTone = a; s.cyDecay = b; s.ohDecay = b;

        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        std::vector<float> v(at(0.02));
        for (int rep = 0; rep < 100; ++rep)
        {
            for (int voice = 0; voice < Voice::count; voice += 1 + (rep & 1))
                e.trigger(voice, true);
            e.render(v.data(), (int) v.size());
            ok = ok && finite(v);
            worst = std::max(worst, peak(v));
        }
    }
    // Sixteen accented sounds at full volume do sum to well over 1 - that is the mix, not
    // a fault. What matters is that it stays finite and bounded.
    check(ok && worst < 16.0, "everything at once, every extreme", fmt("peak %.2f over 2 s of chaos", worst));

    // a fast roll piles into the ring rather than restarting it, and stays sane
    {
        Engine e;
        e.prepare(rate);
        Settings s = loud();
        s.bdDecay = 10;
        e.setSettings(s);
        std::vector<float> v(at(0.01));
        double p = 0;
        for (int rep = 0; rep < 300; ++rep)
        {
            e.trigger(Voice::BD, true);
            e.render(v.data(), (int) v.size());
            p = std::max(p, peak(v));
        }
        check(std::isfinite(p) && p < 4.0, "a 100 Hz roll on a long bass drum", fmt("peak %.2f", p));
    }
}

void performance()
{
    std::printf("\nperformance\n");

    Engine e;
    e.prepare(rate);
    e.setSettings(loud());
    const int block = 256;
    std::vector<float> v(block);
    const int blocks = (int) (20.0 * rate / block);

    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        // a busy 16th-note pattern at 120 BPM: everything, every step
        if (b % 12 == 0)
            for (int voice = 0; voice < Voice::count; ++voice) e.trigger(voice, (b / 12) % 4 == 0);
        e.render(v.data(), block);
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double load = secs / 20.0 * 100.0;
    check(load < 10.0, "all sixteen sounds, every 16th", fmt("%.2f%% of one core", load));
}

//------------------------------------------------------------------------------ demos

void writeWav(const std::string& path, const std::vector<float>& v)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (! f) { std::printf("  could not write %s\n", path.c_str()); return; }

    auto u32 = [f] (uint32_t x) { std::fwrite(&x, 4, 1, f); };
    auto u16 = [f] (uint16_t x) { std::fwrite(&x, 2, 1, f); };

    const uint32_t bytes = (uint32_t) v.size() * 2;
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32((uint32_t) rate); u32((uint32_t) rate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (float x : v)
    {
        const int16_t s = (int16_t) std::lround(clamp(x, -1.0f, 1.0f) * 32767.0f);
        std::fwrite(&s, 2, 1, f);
    }
    std::fclose(f);
    std::printf("  wrote %s\n", path.c_str());
}

// A pattern, one row per sound: x a hit, A an accented one, . nothing - sixteenths.
struct Row { int voice; const char* steps; };

std::vector<float> play(const std::vector<Row>& rows, double bpm, int bars, const Settings& s)
{
    Engine e;
    e.prepare(rate);
    e.setSettings(s);

    const size_t step = (size_t) (rate * 60.0 / bpm / 4.0);
    std::vector<float> v(step * 16 * (size_t) bars + at(2.0));

    size_t pos = 0;
    for (int bar = 0; bar < bars; ++bar)
        for (int i = 0; i < 16; ++i)
        {
            for (const auto& r : rows)
                if (r.steps[i] == 'x' || r.steps[i] == 'A') e.trigger(r.voice, r.steps[i] == 'A');
            e.render(v.data() + pos, (int) step);
            pos += step;
        }
    e.render(v.data() + pos, (int) (v.size() - pos));
    return v;
}

void demos(const std::string& dir)
{
    std::printf("\ndemos\n");

    // every sound in turn, plain then accented
    {
        Engine e;
        e.prepare(rate);
        e.setSettings(loud());
        const size_t gap = at(0.6);
        std::vector<float> v(gap * 2 * Voice::count);
        for (int voice = 0; voice < Voice::count; ++voice)
            for (int a = 0; a < 2; ++a)
            {
                e.trigger(voice, a == 1);
                e.render(v.data() + gap * (size_t) (voice * 2 + a), (int) gap);
            }
        writeWav(dir + "/mz808-every-sound.wav", v);
    }

    {
        Settings s;
        s.bdDecay = 7; s.volume = 7;
        writeWav(dir + "/mz808-beat.wav", play({
            { Voice::BD, "A.....x...A....." },
            { Voice::SD, "....A.......A..." },
            { Voice::CP, "............x..." },
            { Voice::CH, "x.x.x.x.x.x.x..x" },
            { Voice::OH, ".............x.." },
            { Voice::RS, "..........x....." },
            { Voice::CB, "...x............" },
        }, 98, 4, s));
    }

    {
        Settings s;
        s.bdDecay = 10; s.bdTone = 3; s.bdLevel = 9; s.volume = 7;
        writeWav(dir + "/mz808-boom.wav", play({
            { Voice::BD, "A.........x....." },
            { Voice::CH, "x.x.x.x.x.x.x.x." },
            { Voice::CP, "....A.......A..." },
            { Voice::CY, "x..............." },
        }, 70, 2, s));
    }

    {
        Settings s;
        s.volume = 7;
        writeWav(dir + "/mz808-congas.wav", play({
            { Voice::BD, "x.......x......." },
            { Voice::LC, "..x..x......x..." },
            { Voice::MC, "x.....x..x......" },
            { Voice::HC, "...x......x..xA." },
            { Voice::CL, "x..x..x...x.x..." },
            { Voice::MA, "xxxxxxxxxxxxxxxx" },
        }, 112, 4, s));
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::string wavDir;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--wav") == 0 && i + 1 < argc) wavDir = argv[++i];

    std::printf("Mz808 engine check, %.0f Hz\n", rate);

    everySound();
    accentFromVelocity();
    bassDrum();
    snare();
    tomsAndCongas();
    rimAndClaves();
    metal();
    sampleRates();
    stability();
    performance();

    if (! wavDir.empty()) demos(wavDir);

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : (std::to_string(failures) + " FAILED").c_str());
    return failures == 0 ? 0 : 1;
}
