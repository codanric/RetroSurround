// Standard RBJ cookbook biquad, reused from earlier work in this project.
#pragma once
#include <cmath>

// M_PI is a POSIX/GNU extension, not standard C++ -- MSVC only exposes it
// with _USE_MATH_DEFINES defined BEFORE <cmath>'s first inclusion anywhere
// in the translation unit, which is fragile to include order (this is the
// same MSVC portability issue already solved once before in this project,
// for simple_fft.h's SIMPLE_FFT_PI -- missed applying the same fix here).
// A local constant sidesteps the problem entirely.
constexpr double BIQUAD_PI = 3.14159265358979323846;

struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    double z1=0,z2=0;
    double process(double x) {
        double y = b0*x + z1;
        z1 = b1*x + z2 - a1*y;
        z2 = b2*x - a2*y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// 4th-order Linkwitz-Riley crossover = two cascaded 2nd-order Butterworth
// sections at the same cutoff. Standard technique for a clean, matched
// lowpass/highpass pair that sums back to flat if you ever needed to
// (we don't here, but it's the right tool regardless).
struct LRCrossover {
    Biquad lp1, lp2, hp1, hp2;
    void design(double cutoffHz, double sr) {
        double w0 = 2*BIQUAD_PI*cutoffHz/sr;
        double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
        double alpha = sinw0/std::sqrt(2.0); // Q=0.7071 (Butterworth)

        double a0;
        // lowpass -- set coefficients on lp1 and lp2 INDIVIDUALLY. This
        // used to be `lp2 = lp1;`, a whole-struct copy that also
        // overwrote lp2's z1/z2 (its persistent filter memory) with
        // lp1's current memory every time design() ran. Since design()
        // is called from setLowCutoffHz()/setHighCutoffHz(), which get
        // called EVERY BLOCK from applyAllParameters() regardless of
        // whether the cutoff actually changed, this was corrupting the
        // cascade's second stage on every single block boundary --
        // confirmed as the actual cause of a persistent "robotic"/
        // buzzing artifact that only appeared with bass redirection on
        // (the only place this crossover is used).
        double lb0=(1-cosw0)/2, lb1=1-cosw0, lb2=(1-cosw0)/2;
        double la0=1+alpha, la1=-2*cosw0, la2=1-alpha;
        a0=la0;
        lp1.b0=lb0/a0; lp1.b1=lb1/a0; lp1.b2=lb2/a0; lp1.a1=la1/a0; lp1.a2=la2/a0;
        lp2.b0=lb0/a0; lp2.b1=lb1/a0; lp2.b2=lb2/a0; lp2.a1=la1/a0; lp2.a2=la2/a0;

        // highpass -- same fix, coefficients only, z-state untouched
        double hb0=(1+cosw0)/2, hb1=-(1+cosw0), hb2=(1+cosw0)/2;
        a0=la0; // same denominator coefficients as lowpass at same cutoff/Q
        hp1.b0=hb0/a0; hp1.b1=hb1/a0; hp1.b2=hb2/a0; hp1.a1=la1/a0; hp1.a2=la2/a0;
        hp2.b0=hb0/a0; hp2.b1=hb1/a0; hp2.b2=hb2/a0; hp2.a1=la1/a0; hp2.a2=la2/a0;
    }
    double lowpass(double x) { return lp2.process(lp1.process(x)); }
    double highpass(double x) { return hp2.process(hp1.process(x)); }
};
