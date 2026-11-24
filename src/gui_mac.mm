// RPVerb Reconstruction: a minimal macOS GUI laid out along the signal flow.
// Top row, left to right, is the main path: IN, Ensemble, Distortion, EQ (pre), Pre-Delay, the three
// reverbs, EQ (post), the envelope's gain, Dry/Wet, OUT. The bottom row holds the side branches
// (Early, Late, Envelope); their wires go to whichever point the routing switches select. The EQ
// panel moves between its pre and post slots with the Post Audio switch.
// Objective-C++ with manual retain/release. Everything runs on the host's main thread.
#include "gui.hpp"
#include "params.hpp"

#import <AppKit/AppKit.h>

#include <cmath>
#include <functional>
#include <vector>

using namespace rpverb_clap;

namespace {
NSColor* RGB(int hex, CGFloat a = 1.0) {
    return [NSColor colorWithCalibratedRed:((hex >> 16) & 0xff) / 255.0 green:((hex >> 8) & 0xff) / 255.0
                                      blue:(hex & 0xff) / 255.0 alpha:a];
}
NSColor* CBack() { return RGB(0x15171d); }
NSColor* CPanel() { return RGB(0x1f222b); }
NSColor* CEdge() { return RGB(0x2e3240); }
NSColor* CTrack() { return RGB(0x363b4a); }
NSColor* CAccent() { return RGB(0x5fb7ff); }
NSColor* CMod() { return RGB(0xb48cff); }
NSColor* CText() { return RGB(0xe6eaf2); }
NSColor* CLabel() { return RGB(0xa3abbb); }
NSColor* CWireOn() { return RGB(0x5fb7ff, 0.85); }
NSColor* CLedOn() { return RGB(0x5fb7ff); }
NSColor* CLedOff() { return RGB(0x2c3140); }

NSFont* LabelFont() { return [NSFont systemFontOfSize:9.5 weight:NSFontWeightMedium]; }
NSFont* ValueFont() { return [NSFont monospacedDigitSystemFontOfSize:9.0 weight:NSFontWeightRegular]; }
NSFont* HeaderFont() { return [NSFont systemFontOfSize:10 weight:NSFontWeightBold]; }

void DrawText(NSString* s, NSRect r, NSFont* f, NSColor* c, NSTextAlignment align = NSTextAlignmentCenter) {
    NSMutableParagraphStyle* st = [[NSMutableParagraphStyle alloc] init];
    st.alignment = align;
    st.lineBreakMode = NSLineBreakByClipping;
    [s drawInRect:r withAttributes:@{NSFontAttributeName: f, NSForegroundColorAttributeName: c, NSParagraphStyleAttributeName: st}];
    [st release];
}
NSString* Str(const char* s) { return [NSString stringWithUTF8String:s]; }
int Index(uint32_t id) { return paramIndex(id); }
double Hi(int i) { return kParams[i].steps ? kParams[i].steps - 1 : 1.0; }
}  // namespace

// ---------------------------------------------------------------- one control
enum CtlStyle { kKnob, kBigKnob, kLed, kRadio, kSegments, kMenu, kButton };

@interface RvControl : NSView {
@public
    int mIndex;
    CtlStyle mStyle;
    NSString* mLabel;
    UiHost* mHost;
    std::function<bool()> mDim;
    double mShown, mShownMod;
    BOOL mShownDim, mDragging;
    double mDrag;
    NSPoint mLast;
}
@end

@implementation RvControl
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)e { (void)e; return YES; }
- (void)dealloc { [mLabel release]; [super dealloc]; }

- (void)refresh {
    const double v = mDragging ? mShown : mHost->uiValue(mIndex), m = mHost->uiMod(mIndex);
    const BOOL d = mDim ? (mDim() ? YES : NO) : NO;
    if (v != mShown || m != mShownMod || d != mShownDim) { mShown = v; mShownMod = m; mShownDim = d; [self setNeedsDisplay:YES]; }
}
- (void)set:(double)v {
    v = std::fmin(std::fmax(v, 0.0), Hi(mIndex));
    if (kParams[mIndex].steps) v = std::floor(v + 0.5);
    if (v != mHost->uiValue(mIndex)) mHost->uiSet(mIndex, v);
    mShown = mHost->uiValue(mIndex);
    [self setNeedsDisplay:YES];
}
- (void)tap:(double)v { mHost->uiBegin(mIndex); [self set:v]; mHost->uiEnd(mIndex); }

- (void)mouseDown:(NSEvent*)e {
    const ParamDef& d = kParams[mIndex];
    const NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    const NSRect b = self.bounds;
    switch (mStyle) {
    case kLed: [self tap:mHost->uiValue(mIndex) >= 0.5 ? 0 : 1]; return;
    case kButton: mHost->uiBegin(mIndex); [self set:1]; return;
    case kRadio: [self tap:std::floor(p.y / (b.size.height / d.steps))]; return;
    case kSegments: [self tap:std::floor(p.x / (b.size.width / d.steps))]; return;
    case kMenu: {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
        menu.autoenablesItems = NO;
        for (int k = 0; k < d.steps; k++) {
            NSMenuItem* it = [[NSMenuItem alloc] initWithTitle:Str(d.labels[k]) action:@selector(pick:) keyEquivalent:@""];
            it.target = self; it.tag = k; it.state = k == int(mShown + 0.5) ? NSControlStateValueOn : NSControlStateValueOff;
            [menu addItem:it];
            [it release];
        }
        [menu popUpMenuPositioningItem:[menu itemAtIndex:int(mShown + 0.5)] atLocation:NSMakePoint(4, b.size.height - 3) inView:self];
        [menu release];
        return;
    }
    default: break;
    }
    mHost->uiBegin(mIndex);
    if (e.clickCount >= 2) { [self set:d.def]; mHost->uiEnd(mIndex); return; }  // double click: default
    mDragging = YES;
    mDrag = mHost->uiValue(mIndex);
    mLast = [e locationInWindow];
}
- (void)pick:(NSMenuItem*)it { [self tap:double(it.tag)]; }
- (void)mouseDragged:(NSEvent*)e {
    if (!mDragging) return;
    const NSPoint l = [e locationInWindow];
    double per = 1.0 / 200.0;
    if (e.modifierFlags & NSEventModifierFlagShift) per /= 8.0;
    mDrag = std::fmin(std::fmax(mDrag + (l.y - mLast.y) * per, 0.0), 1.0);
    mLast = l;
    [self set:mDrag];
}
- (void)mouseUp:(NSEvent*)e {
    (void)e;
    if (mStyle == kButton) { [self set:0]; mHost->uiEnd(mIndex); return; }
    if (!mDragging) return;
    mDragging = NO;
    mHost->uiEnd(mIndex);
}
- (void)scrollWheel:(NSEvent*)e {
    if (mStyle != kKnob && mStyle != kBigKnob) return;
    double d = e.scrollingDeltaY / (e.hasPreciseScrollingDeltas ? 400.0 : 40.0);
    if (e.modifierFlags & NSEventModifierFlagShift) d *= 0.125;
    if (d == 0) return;
    mHost->uiBegin(mIndex);
    [self set:mHost->uiValue(mIndex) + d];
    mHost->uiEnd(mIndex);
}

- (void)led:(NSRect)r on:(BOOL)on {
    [(on ? CLedOn() : CLedOff()) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:r xRadius:2.5 yRadius:2.5] fill];
}
static void Arc(NSBezierPath* p, NSPoint c, double r, double a0, double a1) {  // degrees, flipped view
    if (a1 < a0) std::swap(a0, a1);
    if (a1 - a0 < 1e-3) return;
    [p moveToPoint:NSMakePoint(c.x + r * cos(a0 * M_PI / 180), c.y + r * sin(a0 * M_PI / 180))];
    for (double a = a0 + 3; a < a1; a += 3) [p lineToPoint:NSMakePoint(c.x + r * cos(a * M_PI / 180), c.y + r * sin(a * M_PI / 180))];
    [p lineToPoint:NSMakePoint(c.x + r * cos(a1 * M_PI / 180), c.y + r * sin(a1 * M_PI / 180))];
}
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const ParamDef& d = kParams[mIndex];
    const NSRect b = self.bounds;
    const CGFloat W = b.size.width, H = b.size.height;
    char buf[64];
    [NSGraphicsContext saveGraphicsState];
    if (mShownDim) CGContextSetAlpha([[NSGraphicsContext currentContext] CGContext], 0.35);
    switch (mStyle) {
    case kKnob: case kBigKnob: {
        const CGFloat r = mStyle == kBigKnob ? 17 : 12;
        const NSPoint c = NSMakePoint(W / 2, r + 4);
        const double a0 = 135, span = 270, f = std::fmin(std::fmax(mShown, 0.0), 1.0);
        NSBezierPath* t = [NSBezierPath bezierPath]; t.lineWidth = 3; t.lineCapStyle = NSLineCapStyleRound;
        Arc(t, c, r, a0, a0 + span); [CTrack() setStroke]; [t stroke];
        NSBezierPath* a = [NSBezierPath bezierPath]; a.lineWidth = 3; a.lineCapStyle = NSLineCapStyleRound;
        Arc(a, c, r, a0, a0 + span * f); [CAccent() setStroke]; [a stroke];
        if (mShownMod != 0) {  // CV from host modulation: added to the knob, clipped to 0..1 in the device
            const double fm = std::fmin(std::fmax(mShown + mShownMod, 0.0), 1.0);
            NSBezierPath* m = [NSBezierPath bezierPath]; m.lineWidth = 2; m.lineCapStyle = NSLineCapStyleRound;
            Arc(m, c, r + 4, a0 + span * f, a0 + span * fm); [CMod() setStroke]; [m stroke];
        }
        [RGB(0x2a2e39) setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(c.x - r + 4, c.y - r + 4, 2 * r - 8, 2 * r - 8)] fill];
        const double an = (a0 + span * f) * M_PI / 180;
        NSBezierPath* n = [NSBezierPath bezierPath]; n.lineWidth = 2; n.lineCapStyle = NSLineCapStyleRound;
        [n moveToPoint:NSMakePoint(c.x + (r - 9) * cos(an), c.y + (r - 9) * sin(an))];
        [n lineToPoint:NSMakePoint(c.x + (r - 4) * cos(an), c.y + (r - 4) * sin(an))];
        [CText() setStroke]; [n stroke];
        DrawText(mLabel, NSMakeRect(-4, 2 * r + 7, W + 8, 13), LabelFont(), CLabel());
        formatValue(mIndex, mShown, buf, sizeof buf);
        DrawText(Str(buf), NSMakeRect(-6, 2 * r + 19, W + 12, 12), ValueFont(), CText());
        break;
    }
    case kLed:
        [self led:NSMakeRect(1, (H - 10) / 2, 16, 10) on:mShown >= 0.5];
        if (mLabel.length) DrawText(mLabel, NSMakeRect(22, (H - 13) / 2, W - 22, 13), LabelFont(), CLabel(), NSTextAlignmentLeft);
        break;
    case kButton: {
        NSBezierPath* p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 0.5, 0.5) xRadius:4 yRadius:4];
        [(mShown >= 0.5 ? CAccent() : RGB(0x2a2e39)) setFill]; [p fill];
        p.lineWidth = 1; [CEdge() setStroke]; [p stroke];
        DrawText(mLabel, NSMakeRect(0, (H - 13) / 2, W, 13), LabelFont(), mShown >= 0.5 ? CBack() : CText());
        break;
    }
    case kRadio: case kSegments: {
        const int n = d.steps, cur = int(mShown + 0.5);
        for (int k = 0; k < n; k++) {
            if (mStyle == kRadio) {
                const CGFloat rh = H / n, y = k * rh;
                [self led:NSMakeRect(1, y + (rh - 10) / 2, 14, 10) on:k == cur];
                DrawText(Str(d.labels[k]), NSMakeRect(20, y + (rh - 13) / 2, W - 20, 13), LabelFont(), k == cur ? CText() : CLabel(), NSTextAlignmentLeft);
            } else {
                const CGFloat sw = W / n;
                const NSRect r = NSMakeRect(k * sw + 1, 0.5, sw - 2, H - 1);
                [(k == cur ? CAccent() : RGB(0x2a2e39)) setFill];
                [[NSBezierPath bezierPathWithRoundedRect:r xRadius:3 yRadius:3] fill];
                DrawText(Str(d.labels[k]), NSMakeRect(r.origin.x, (H - 12) / 2, r.size.width, 12), LabelFont(), k == cur ? CBack() : CText());
            }
        }
        break;
    }
    case kMenu: {
        NSBezierPath* p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 0.5, 0.5) xRadius:3 yRadius:3];
        [RGB(0x2a2e39) setFill]; [p fill];
        p.lineWidth = 1; [CEdge() setStroke]; [p stroke];
        formatValue(mIndex, mShown, buf, sizeof buf);
        DrawText([NSString stringWithFormat:@"%@  %s", mLabel, buf], NSMakeRect(6, (H - 13) / 2, W - 20, 13), LabelFont(), CText(), NSTextAlignmentLeft);
        DrawText(@"▾", NSMakeRect(W - 14, (H - 13) / 2, 10, 13), LabelFont(), CLabel());
        break;
    }
    }
    [NSGraphicsContext restoreGraphicsState];
}
@end

// ---------------------------------------------------------------- panel: sections, nodes, wires
struct Box { NSRect r; NSString* title; std::function<bool()> active; };
struct Wire { std::vector<NSPoint> pts; std::function<bool()> show, on; NSString* label; bool arrow; };  // shown while show(); dashed unless on()

@interface RvPanel : NSView {
@public
    std::vector<Box> mBoxes;   // module frames and drawn nodes (IN, ×ENV, OUT)
    std::vector<Wire> mWires;
    NSRect mEqSlots[2];
    std::function<int()> mEqSlot;
    std::function<int()> mReverbs;  // bit k: reverb k+1 runs
    NSPoint mReverbsAt;
    NSString* mStatus;
}
@end

@implementation RvPanel
- (BOOL)isFlipped { return YES; }
- (void)dealloc {
    for (auto& b : mBoxes) [b.title release];
    for (auto& w : mWires) [w.label release];
    [mStatus release];
    [super dealloc];
}
- (void)drawWire:(const Wire&)w {
    const bool on = !w.on || w.on();
    NSBezierPath* p = [NSBezierPath bezierPath];
    p.lineWidth = on ? 1.6 : 1.0;
    p.lineJoinStyle = NSLineJoinStyleRound;
    if (!on) { const CGFloat dash[2] = {3, 3}; [p setLineDash:dash count:2 phase:0]; }
    [p moveToPoint:w.pts[0]];
    for (size_t i = 1; i < w.pts.size(); i++) [p lineToPoint:w.pts[i]];
    [(on ? CWireOn() : RGB(0x3a4052)) setStroke];
    [p stroke];
    if (w.arrow && on && w.pts.size() >= 2) {  // arrowhead at the end
        const NSPoint a = w.pts[w.pts.size() - 2], z = w.pts.back();
        const double ang = atan2(z.y - a.y, z.x - a.x);
        NSBezierPath* h = [NSBezierPath bezierPath];
        [h moveToPoint:z];
        [h lineToPoint:NSMakePoint(z.x - 7 * cos(ang - 0.45), z.y - 7 * sin(ang - 0.45))];
        [h lineToPoint:NSMakePoint(z.x - 7 * cos(ang + 0.45), z.y - 7 * sin(ang + 0.45))];
        [h closePath];
        [CWireOn() setFill]; [h fill];
    }
    if (w.label && on) {  // at the first corner, or beside a straight wire's middle
        const NSPoint a = w.pts.size() > 2 ? w.pts[1] : NSMakePoint((w.pts[0].x + w.pts[1].x) / 2, (w.pts[0].y + w.pts[1].y) / 2);
        DrawText(w.label, NSMakeRect(a.x + 6, a.y - 13, 200, 12), ValueFont(), CLabel(), NSTextAlignmentLeft);
    }
}
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const NSRect b = self.bounds;
    [CBack() setFill]; NSRectFill(b);
    DrawText(@"RPVERB", NSMakeRect(16, 12, 120, 24), [NSFont systemFontOfSize:20 weight:NSFontWeightHeavy], CAccent(), NSTextAlignmentLeft);
    DrawText(@"reconstruction  ·  signal flows left to right", NSMakeRect(112, 20, 300, 13), ValueFont(), CLabel(), NSTextAlignmentLeft);
    if (mStatus) DrawText(mStatus, NSMakeRect(420, 20, 330, 13), ValueFont(), CText(), NSTextAlignmentRight);
    [CEdge() setFill]; NSRectFill(NSMakeRect(0, 47, b.size.width, 1));
    for (auto& w : mWires) if ((!w.show || w.show()) && w.on && !w.on()) [self drawWire:w];  // unused routes first, dashed
    for (auto& w : mWires) if ((!w.show || w.show()) && (!w.on || w.on())) [self drawWire:w];
    const int slot = mEqSlot ? mEqSlot() : 0;
    const NSRect empty = mEqSlots[1 - slot];  // the EQ's other slot: signal passes straight through
    NSBezierPath* e = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(empty, 6, 40) xRadius:6 yRadius:6];
    const CGFloat dash[2] = {3, 3}; [e setLineDash:dash count:2 phase:0]; e.lineWidth = 1;
    [CEdge() setStroke]; [e stroke];
    DrawText(slot == 0 ? @"EQ is pre" : @"EQ is post", NSMakeRect(empty.origin.x, NSMidY(empty) - 24, empty.size.width, 12), ValueFont(), CLabel());
    for (auto& bx : mBoxes) {
        const bool act = !bx.active || bx.active();
        NSBezierPath* p = [NSBezierPath bezierPathWithRoundedRect:bx.r xRadius:7 yRadius:7];
        [CPanel() setFill]; [p fill];
        p.lineWidth = act ? 1.2 : 1; [(act ? CEdge() : RGB(0x262a35)) setStroke]; [p stroke];
        DrawText(bx.title, NSMakeRect(bx.r.origin.x + 8, bx.r.origin.y + 6, bx.r.size.width - 16, 13), HeaderFont(),
                 act ? CText() : CLabel(), bx.r.size.width < 60 ? NSTextAlignmentCenter : NSTextAlignmentLeft);
    }
    [self drawReverbs];
}
- (void)drawReverbs {
    if (!mReverbs) return;
    const int m = mReverbs();
    DrawText(@"in parallel:", NSMakeRect(mReverbsAt.x, mReverbsAt.y + 3, 70, 12), ValueFont(), CLabel(), NSTextAlignmentLeft);
    for (int k = 0; k < 3; k++) {
        const NSRect r = NSMakeRect(mReverbsAt.x + 70 + k * 34, mReverbsAt.y, 28, 18);
        const bool on = (m >> k) & 1;
        [(on ? CAccent() : RGB(0x2a2e39)) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:r xRadius:3 yRadius:3] fill];
        DrawText([NSString stringWithFormat:@"%d", k + 1], NSMakeRect(r.origin.x, r.origin.y + 3, r.size.width, 12), LabelFont(), on ? CBack() : CLabel());
    }
    if (!((m >> 1) & 1) && (m & 5)) DrawText(@"HQ off: x1.5", NSMakeRect(mReverbsAt.x + 176, mReverbsAt.y + 3, 80, 12), ValueFont(), CLabel(), NSTextAlignmentLeft);
}
- (void)setStatus:(NSString*)s {
    if ([s isEqualToString:mStatus]) return;
    [mStatus release]; mStatus = [s retain];
    [self setNeedsDisplay:YES];
}
@end

// ---------------------------------------------------------------- controller
@interface RvGui : NSObject {
@public
    UiHost* mHost;
    RvPanel* mPanel;
    NSMutableArray* mControls;
    RvControl* mEq[5];
    NSTimer* mTimer;
    int mEqShown;
}
@end

@implementation RvGui
- (RvControl*)add:(uint32_t)id style:(CtlStyle)style x:(CGFloat)x y:(CGFloat)y w:(CGFloat)w h:(CGFloat)h label:(NSString*)label {
    RvControl* c = [[RvControl alloc] initWithFrame:NSMakeRect(x, y, w, h)];
    c->mIndex = Index(id);
    c->mStyle = style;
    c->mLabel = [label retain];
    c->mHost = mHost;
    c->mShown = mHost->uiValue(c->mIndex);
    c.toolTip = Str(kParams[c->mIndex].name);
    [mPanel addSubview:c];
    [mControls addObject:c];
    [c release];
    return c;
}
- (void)box:(NSString*)title r:(NSRect)r active:(std::function<bool()>)active {
    mPanel->mBoxes.push_back({r, [title retain], active});
}
- (void)wire:(std::vector<NSPoint>)pts show:(std::function<bool()>)show on:(std::function<bool()>)on label:(NSString*)label arrow:(bool)arrow {
    mPanel->mWires.push_back({pts, show, on, [label retain], arrow});
}
- (double)v:(uint32_t)id { return mHost->uiValue(Index(id)); }

- (id)initWithHost:(UiHost*)host {
    self = [super init];
    if (!self) return nil;
    mHost = host;
    mControls = [[NSMutableArray alloc] init];
    const CGFloat W = 1114, H = 560, y1 = 96, h1 = 236, y2 = 372, h2 = 176, mid = y1 + h1 / 2;
    mPanel = [[RvPanel alloc] initWithFrame:NSMakeRect(0, 0, W, H)];
    RvGui* me = self;
    auto on = [me](uint32_t id) { return [me v:id] >= 0.5; };
    auto reverbOn = [on] { return on(54); };

    // main row: x positions of the modules
    const CGFloat xIn = 12, xEns = 74, xDist = 210, xEq1 = 312, xPre = 414, xRev = 516, xEq2 = 796, xEnv = 898, xMix = 964, xOut = 1058;
    // header: power, reset, reset on stop
    [self add:kIdPower style:kSegments x:770 y:14 w:150 h:20 label:@""];
    [self add:kIdReset style:kButton x:930 y:14 w:56 h:20 label:@"Reset"];
    [self add:105 style:kLed x:996 y:15 w:110 h:18 label:@"Reset on Stop"];

    // wires first so they sit under the panels
    auto mainOn = [] { return true; };
    const CGFloat xs[] = {xIn + 44, xEns, xEns + 118, xDist, xDist + 84, xEq1, xEq1 + 84, xPre, xPre + 84, xRev, xRev + 262, xEq2, xEq2 + 84, xEnv, xEnv + 48, xMix, xMix + 76, xOut};
    for (size_t k = 0; k + 1 < sizeof xs / sizeof xs[0]; k += 2)
        [self wire:{NSMakePoint(xs[k], mid), NSMakePoint(xs[k + 1], mid)} show:nullptr on:mainOn label:nil arrow:true];
    for (CGFloat x : {xEq1, xEq2})  // pass-through line across the EQ slot that is not in use
        [self wire:{NSMakePoint(x, mid), NSMakePoint(x + 84, mid)} show:nullptr on:mainOn label:nil arrow:false];
    // dry path and ensemble direct, above the main row
    [self wire:{NSMakePoint(xIn + 22, mid - 26), NSMakePoint(xIn + 22, 66), NSMakePoint(xMix + 30, 66), NSMakePoint(xMix + 30, y1)}
            show:nullptr on:mainOn label:@"dry" arrow:true];
    [self wire:{NSMakePoint(xEns + 59, y1), NSMakePoint(xEns + 59, 82), NSMakePoint(xMix + 46, 82), NSMakePoint(xMix + 46, y1)}
            show:[on] { return on(28); } on:[on] { return on(29); } label:@"ensemble direct (Direct + Reverb)" arrow:true];
    // early reflections: fed after the ensemble, out into the reverb or to the reverb output
    const CGFloat jEns = xEns + 118 + 9, jRevIn = xRev - 9, jRevOut = xEq2 - 9;
    [self wire:{NSMakePoint(jEns, mid), NSMakePoint(jEns, y2)} show:[on] { return on(70); } on:nullptr label:nil arrow:true];
    [self wire:{NSMakePoint(374, y2 + 40), NSMakePoint(394, y2 + 40), NSMakePoint(394, 346), NSMakePoint(jRevIn, 346), NSMakePoint(jRevIn, mid)}
            show:[on] { return on(70); } on:[me] { return [me v:71] >= 0.5; } label:nil arrow:true];
    [self wire:{NSMakePoint(374, y2 + 40), NSMakePoint(394, y2 + 40), NSMakePoint(394, 346), NSMakePoint(jRevOut - 4, 346), NSMakePoint(jRevOut - 4, mid)}
            show:[on] { return on(70); } on:[me] { return [me v:71] < 0.5; } label:nil arrow:true];
    // late reflection: a tap after the pre-delay, out into the reverb or to the reverb output
    [self wire:{NSMakePoint(xPre + 42, y1 + h1), NSMakePoint(xPre + 42, y2)} show:[on] { return on(83); } on:nullptr label:nil arrow:true];
    [self wire:{NSMakePoint(604, y2 + 40), NSMakePoint(612, y2 + 40), NSMakePoint(612, 360), NSMakePoint(jRevIn + 4, 360), NSMakePoint(jRevIn + 4, mid)}
            show:[on] { return on(83); } on:[me] { return [me v:84] >= 0.5; } label:nil arrow:true];
    [self wire:{NSMakePoint(604, y2 + 40), NSMakePoint(612, y2 + 40), NSMakePoint(612, 360), NSMakePoint(jRevOut + 4, 360), NSMakePoint(jRevOut + 4, mid)}
            show:[on] { return on(83); } on:[me] { return [me v:84] < 0.5; } label:nil arrow:true];
    // envelope: audio gain (modes 0 and 4) or the reverb's size / length (modes 1, 2, 3)
    auto mode = [me] { return int([me v:43] + 0.5); };
    [self wire:{NSMakePoint(xEnv + 24, y2), NSMakePoint(xEnv + 24, mid + 26)} show:[on] { return on(37); } on:[mode] { return (mode() | 4) == 4; }
           label:@"gain" arrow:true];
    [self wire:{NSMakePoint(700, y2), NSMakePoint(700, y1 + h1)} show:[on] { return on(37); } on:[mode] { const int m = mode(); return m >= 1 && m <= 3; }
           label:@"size / length" arrow:true];

    // nodes and module frames
    [self box:@"IN" r:NSMakeRect(xIn, mid - 26, 44, 52) active:nullptr];
    [self box:@"OUT" r:NSMakeRect(xOut, mid - 26, 44, 52) active:nullptr];
    [self box:@"× ENV" r:NSMakeRect(xEnv, mid - 26, 48, 52) active:[on, mode] { return on(37) && (mode() | 4) == 4; }];
    [self box:@"      ENSEMBLE" r:NSMakeRect(xEns, y1, 118, h1) active:[on] { return on(28); }];
    [self box:@"      DIST" r:NSMakeRect(xDist, y1, 84, h1) active:[on] { return on(50); }];
    [self box:@"PRE-DELAY" r:NSMakeRect(xPre, y1, 84, h1) active:reverbOn];
    [self box:@"      REVERB  ×3" r:NSMakeRect(xRev, y1, 262, h1) active:reverbOn];
    [self box:@"MIX" r:NSMakeRect(xMix, y1, 76, h1) active:nullptr];
    [self box:@"      EARLY REFLECTIONS" r:NSMakeRect(74, y2, 300, h2) active:[on] { return on(70); }];
    [self box:@"      LATE REFLECTION" r:NSMakeRect(414, y2, 190, h2) active:[on] { return on(83); }];
    [self box:@"      ENVELOPE" r:NSMakeRect(622, y2, 480, h2) active:[on] { return on(37); }];
    mPanel->mEqSlots[0] = NSMakeRect(xEq1, y1, 84, h1);
    mPanel->mEqSlots[1] = NSMakeRect(xEq2, y1, 84, h1);
    mPanel->mEqSlot = [on] { return on(38) ? 1 : 0; };
    mPanel->mReverbs = [on] { return on(54) ? (on(100) ? 7 : 5) : 0; };  // HQ off drops reverb 2
    mPanel->mReverbsAt = NSMakePoint(xRev + 12, y1 + h1 - 30);
    [self box:@"" r:NSZeroRect active:nullptr];  // placeholder; the EQ frame is drawn with its controls below

    RvControl* c;
    // ensemble
    [self add:28 style:kLed x:xEns + 6 y:y1 + 4 w:18 h:16 label:@""];
    c = [self add:30 style:kBigKnob x:xEns + 29 y:y1 + 34 w:60 h:72 label:@"Volume"]; c->mDim = [on] { return !on(28); };
    c = [self add:29 style:kRadio x:xEns + 8 y:y1 + 126 w:106 h:40 label:@""]; c->mDim = [on] { return !on(28); };
    // distortion
    [self add:50 style:kLed x:xDist + 6 y:y1 + 4 w:18 h:16 label:@""];
    c = [self add:53 style:kBigKnob x:xDist + 12 y:y1 + 34 w:60 h:72 label:@"Amount"]; c->mDim = [on] { return !on(50); };
    // pre-delay
    c = [self add:2 style:kBigKnob x:xPre + 12 y:y1 + 34 w:60 h:72 label:@"Time"]; c->mDim = [reverbOn] { return !reverbOn(); };
    c = [self add:13 style:kKnob x:xPre + 14 y:y1 + 120 w:56 h:60 label:@"Disorder"]; c->mDim = [reverbOn] { return !reverbOn(); };
    // reverb
    [self add:54 style:kLed x:xRev + 6 y:y1 + 4 w:18 h:16 label:@""];
    [self add:100 style:kLed x:xRev + 172 y:y1 + 4 w:84 h:16 label:@"HQ Mode"];
    c = [self add:94 style:kMenu x:xRev + 10 y:y1 + 28 w:146 h:20 label:@"Space"]; c->mDim = [reverbOn] { return !reverbOn(); };
    c = [self add:95 style:kMenu x:xRev + 162 y:y1 + 28 w:90 h:20 label:@"Colour"]; c->mDim = [reverbOn] { return !reverbOn(); };
    const uint32_t rv[8] = {3, 9, 4, 5, 49, 6, 7, 93};
    NSString* rn[8] = {@"Space Size", @"Disorder", @"Length", @"Damping", @"Diffusion", @"Low-Pass", @"High-Pass", @"Volume"};
    for (int k = 0; k < 8; k++) {
        c = [self add:rv[k] style:kKnob x:xRev + 11 + (k % 4) * 60 y:y1 + 58 + (k / 4) * 66 w:60 h:60 label:rn[k]];
        c->mDim = [reverbOn] { return !reverbOn(); };
    }
    // EQ: one panel that sits in the pre or the post slot
    mEq[0] = [self add:39 style:kLed x:0 y:0 w:18 h:16 label:@""];
    const uint32_t eq[3] = {40, 41, 42};
    NSString* en[3] = {@"Low", @"Mid", @"High"};
    for (int k = 0; k < 3; k++) {
        mEq[1 + k] = [self add:eq[k] style:kKnob x:0 y:0 w:56 h:60 label:en[k]];
        mEq[1 + k]->mDim = [on] { return !on(39); };
    }
    mEq[4] = [self add:38 style:kLed x:0 y:0 w:70 h:16 label:@"Post"];
    mEqShown = -1;
    // mix
    [self add:8 style:kBigKnob x:xMix + 8 y:y1 + 34 w:60 h:72 label:@"Dry/Wet"];
    // early reflections
    const CGFloat xE = 74;
    [self add:70 style:kLed x:xE + 6 y:y2 + 4 w:18 h:16 label:@""];
    const uint32_t er[6] = {73, 72, 75, 74, 90, 92};
    NSString* ern[6] = {@"Volume", @"Length", @"Damping", @"Feedback", @"Side", @"Cross"};
    for (int k = 0; k < 6; k++) {
        c = [self add:er[k] style:kKnob x:xE + 8 + (k % 4) * 58 y:y2 + 28 + (k / 4) * 70 w:56 h:60 label:ern[k]];
        c->mDim = [on] { return !on(70); };
    }
    c = [self add:71 style:kRadio x:xE + 128 y:y2 + 108 w:110 h:40 label:@""]; c->mDim = [on] { return !on(70); };
    // late reflection
    const CGFloat xL = 414;
    [self add:83 style:kLed x:xL + 6 y:y2 + 4 w:18 h:16 label:@""];
    const uint32_t lr[3] = {86, 85, 87};
    NSString* lrn[3] = {@"Volume", @"Length", @"Damping"};
    for (int k = 0; k < 3; k++) {
        c = [self add:lr[k] style:kKnob x:xL + 8 + k * 58 y:y2 + 28 w:56 h:60 label:lrn[k]];
        c->mDim = [on] { return !on(83); };
    }
    c = [self add:84 style:kRadio x:xL + 12 y:y2 + 108 w:110 h:40 label:@""]; c->mDim = [on] { return !on(83); };
    // envelope
    const CGFloat xV = 622;
    auto envOff = [on] { return !on(37); };
    [self add:37 style:kLed x:xV + 6 y:y2 + 4 w:18 h:16 label:@""];
    c = [self add:43 style:kRadio x:xV + 12 y:y2 + 28 w:150 h:100 label:@""]; c->mDim = envOff;
    c = [self add:48 style:kLed x:xV + 12 y:y2 + 140 w:96 h:16 label:@"Mono Input"]; c->mDim = envOff;
    c = [self add:47 style:kLed x:xV + 112 y:y2 + 140 w:70 h:16 label:@"Latch"]; c->mDim = envOff;
    c = [self add:kIdSideInput style:kLed x:xV + 190 y:y2 + 140 w:150 h:16 label:@"Side Input drives it"]; c->mDim = envOff;
    const uint32_t ev[4] = {44, 45, 46, 97};
    NSString* evn[4] = {@"Attack", @"Hold", @"Release", @"Amount"};
    for (int k = 0; k < 4; k++) {
        c = [self add:ev[k] style:kBigKnob x:xV + 176 + k * 72 y:y2 + 34 w:64 h:72 label:evn[k]];
        c->mDim = envOff;
    }
    [self tick:nil];
    mTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0 / 30.0 target:self selector:@selector(tick:) userInfo:nil repeats:YES] retain];
    [[NSRunLoop mainRunLoop] addTimer:mTimer forMode:NSRunLoopCommonModes];
    return self;
}

- (void)placeEq:(int)slot {
    const NSRect s = mPanel->mEqSlots[slot];
    [mEq[0] setFrameOrigin:NSMakePoint(s.origin.x + 6, s.origin.y + 4)];
    for (int k = 0; k < 3; k++) [mEq[1 + k] setFrameOrigin:NSMakePoint(s.origin.x + 14, s.origin.y + 26 + k * 62)];
    [mEq[4] setFrameOrigin:NSMakePoint(s.origin.x + 10, s.origin.y + s.size.height - 22)];
    auto& eqBox = mPanel->mBoxes.back();  // the EQ frame
    [eqBox.title release];
    eqBox.title = [@"      EQ" retain];
    eqBox.r = s;
    RvGui* me = self;
    eqBox.active = [me] { return [me v:39] >= 0.5; };
    mEqShown = slot;
    [mPanel setNeedsDisplay:YES];
}

- (void)tick:(NSTimer*)t {
    (void)t;
    for (RvControl* c in mControls) [c refresh];
    const int slot = [self v:38] >= 0.5 ? 1 : 0;
    if (slot != mEqShown) [self placeEq:slot];
    const int power = int([self v:kIdPower] + 0.5), counter = mHost->uiResetCounter(), quiet = mHost->uiQuietBatches();
    NSString* s;
    if (power == 0) s = @"off: output silent";
    else if (power == 2) s = @"bypass: input passes through";
    else if (counter <= 9998) s = @"muted while clearing (after a reset)";
    else if (quiet > 1999) s = @"asleep: silent input";
    else s = @"running";
    [mPanel setStatus:[NSString stringWithFormat:@"%@   ·   latency 64 samples", s]];
    [mPanel setNeedsDisplay:YES];
}

- (void)shutdown {
    [mTimer invalidate];
    [mTimer release];
    mTimer = nil;
    [mPanel removeFromSuperview];
}
- (void)dealloc {
    [mTimer invalidate];
    [mTimer release];
    [mControls release];
    [mPanel release];
    [super dealloc];
}
@end

// ---------------------------------------------------------------- facade
namespace rpverb_clap {
namespace gui {
void* create(UiHost& host) { return [[RvGui alloc] initWithHost:&host]; }
void destroy(void* g) {
    if (!g) return;
    RvGui* c = static_cast<RvGui*>(g);
    [c shutdown];
    [c release];
}
bool size(void* g, uint32_t* w, uint32_t* h) {
    if (!g || !w || !h) return false;
    const NSSize s = static_cast<RvGui*>(g)->mPanel.frame.size;
    *w = uint32_t(s.width); *h = uint32_t(s.height);
    return true;
}
bool setParent(void* g, const void* nsView) {
    if (!g || !nsView) return false;
    NSView* parent = static_cast<NSView*>(const_cast<void*>(nsView));
    if (![parent isKindOfClass:[NSView class]]) return false;
    RvPanel* p = static_cast<RvGui*>(g)->mPanel;
    [p removeFromSuperview];
    [parent addSubview:p];
    return true;
}
bool show(void* g, bool shown) {
    if (!g) return false;
    static_cast<RvGui*>(g)->mPanel.hidden = !shown;
    return true;
}
}  // namespace gui
}  // namespace rpverb_clap
