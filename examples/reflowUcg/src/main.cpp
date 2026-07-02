/**
 * @brief azoreanFlow building-block demo, Ucglib variant — same MAX31855 +
 * ST7735 TFT + rotary encoder + OneMenu as ../reflowDemo, but the TFT goes
 * through Ucglib (real RGB) instead of our hand-rolled st7735.h (fg/bg byte
 * only), so the reference profile curve and the live thermocouple trace are
 * drawn in two distinct colours at full pixel resolution.
 */

#if defined(__AVR__) && defined(IOP)
  unsigned long millis();
#endif

#include <oneMenu/oneMenu.h>
#include <oneMenu/menu/IO/IOP/encIn.h>
#include <oneMenu/menu/IO/IOP/btnIn.h>
#include <oneMenu/menu/IO/IOP/oledOut.h>
#include <oneInput/oneInput.h>
#include <chips/avr/avrDevice.h>
#include <oneIO/display/ucgSpi.h>
#include <oneIO/sensor/max31855.h>

using namespace hapi;
using namespace oneMenu;
using namespace oneData;
using namespace onePin;
using namespace oneBit;
using namespace hw::avr;
using namespace oneIO::display;
using namespace oneIO::sensor;

// ── Board / SysTick ───────────────────────────────────────────────────────────
using SysTick = chip::SysTick0<>;
using Board   = AVR::Board<Boot<SysTick>>;
#ifdef IOP
IOP_TIMER0_ISR(Board)
#endif

namespace hw { uint32_t millis() { return SysTick::millis(); } }
unsigned long millis() { return (unsigned long)SysTick::millis(); }

// ── Hardware ──────────────────────────────────────────────────────────────────
// Shared hardware SPI bus (fixed): MOSI=D11, SCK=D13, MISO=D12.
using Spi = chip::SpiMaster;

// ST7735 TFT: CS=D10(PB2), DC=D9(PB1), RST=D8(PB0) — driven via Ucglib.
using TftCs  = AVR::OutPin<Pins<PB2>, chip::PortB>;
using TftDc  = AVR::OutPin<Pins<PB1>, chip::PortB>;
using TftRst = AVR::OutPin<Pins<PB0>, chip::PortB>;
using Ucg    = UcgSpiDisplay<ucg_dev_st7735_18x128x160, ucg_ext_st7735_18,
                             Spi, TftCs, TftDc, TftRst>;

// MAX31855 thermocouple amplifier: CS=D7(PD7), shares the SPI bus with the TFT.
using TcCs = AVR::OutPin<Pins<PD7>, chip::PortD>;
using Tc   = MAX31855<Spi, TcCs>::Api;

// Heater SSR control line: D6(PD6), active-high.
using Heater = AVR::OutPin<Pins<PD6>, chip::PortD>;

// Rotary encoder: A=A0(PC0), B=A1(PC1); push switch SW=A2(PC2). PCINT1 group.
using EncHW = oneInput::InputDef<
  oneInput::Encoder,
  oneInput::avr::AvrEncPins</*group*/1, chip::PortC, /*bitA*/0, /*bitB*/1>
>;
using BtnHW = oneInput::InputDef<
  oneInput::BtnCapture,
  oneInput::Hold<800>,
  oneInput::Click<300>,
  oneInput::Debounce<20>,
  oneInput::avr::AvrBtnPin</*group*/1, chip::PortC, /*bit*/2>
>;
ISR(PCINT1_vect) { EncHW::dispatch(); BtnHW::dispatch(); }

using Enc = oneMenu::EncIn<EncHW, 4>;
using Btn = oneMenu::BtnIn<BtnHW>;

InDef<Enc, Btn> in;
OledDisplay<Ucg> display;

// ── Solder profiles ───────────────────────────────────────────────────────────
struct ProfilePt { uint16_t t_s; uint8_t tempC; };
struct Profile   { const char* name; const ProfilePt* pts; uint8_t n; uint16_t totalS; };

static constexpr ProfilePt leadedPts[] = {
  {0, 25}, {60, 150}, {120, 183}, {165, 225}, {195, 183}, {240, 50}
};
static constexpr ProfilePt leadFreePts[] = {
  {0, 25}, {90, 150}, {180, 200}, {225, 245}, {255, 217}, {300, 50}
};
static constexpr Profile profiles[] = {
  {"Leaded",    leadedPts,   6, 240},
  {"Lead-Free", leadFreePts, 6, 300},
};
static constexpr uint8_t nProfiles = sizeof(profiles) / sizeof(profiles[0]);

uint8_t profileIdx = 0;

// ── Menu text ─────────────────────────────────────────────────────────────────
namespace text {
  static constexpr CText title    {"Reflow (Ucg)"};
  static constexpr CText profLbl  {"Profile"};
  static constexpr CText leaded   {"Leaded"};
  static constexpr CText leadfree {"Lead-Free"};
  static constexpr CText start    {"Start"};
}

// ── Actions ───────────────────────────────────────────────────────────────────
bool running = true;
bool processRun();  // forward — used by action::start

namespace action {
  bool selectProfile(Sz i) { profileIdx = i; return true; }
  bool start(Sz);
}

// ── Menu tree ─────────────────────────────────────────────────────────────────
using ProfileSelect = SelectFieldDef<
  ItemDef<
    AsLabel<StaticText<&text::profLbl>>,
    AsEditMode<>,
    BodyAction<action::selectProfile>
  >,
  StaticBody<
    ItemDef<AsField<StaticText<&text::leaded>>>,
    ItemDef<AsField<StaticText<&text::leadfree>>>
  >,
  WrapNav
>;

using StartItem = ItemDef<Action<action::start>, StaticText<&text::start>>;

auto mainMenu = menuDef<WrapNav>(
  ItemDef<StaticText<&text::title>>{},
  staticBody(
    ProfileSelect{},
    StartItem{}
  )
);

NavDef<TreeNav, Root<decltype(mainMenu), mainMenu>> nav;

// ── Curve plot ────────────────────────────────────────────────────────────────
// Header text occupies the top 2 char-rows (16px); the plot below it draws at
// full pixel resolution (not page-granular like the st7735.h-backed version),
// with the reference profile curve and the live trace in two distinct colours.
static constexpr uint8_t PLOT_TOP_Y = 16;
static constexpr uint8_t PLOT_H     = Ucg::kHeight - PLOT_TOP_Y;
static constexpr int16_t MAX_T      = 260;

static constexpr uint8_t REF_COLOR[3]  = {60, 110, 255};   // dim blue — target curve
static constexpr uint8_t LIVE_COLOR[3] = {255, 60, 60};    // bright red — live trace

static uint8_t yForTemp(int16_t tC) {
  if (tC < 0)     tC = 0;
  if (tC > MAX_T) tC = MAX_T;
  uint16_t span = PLOT_H - 1;
  return uint8_t(PLOT_TOP_Y + (uint16_t(MAX_T - tC) * span) / MAX_T);
}

// Linear-interpolate the profile's target temp at elapsed time t_s.
static uint16_t targetTempAt(const Profile& p, uint16_t t_s) {
  for (uint8_t i = 0; i + 1 < p.n; ++i) {
    if (t_s <= p.pts[i + 1].t_s) {
      uint16_t span = p.pts[i + 1].t_s - p.pts[i].t_s;
      if (!span) return p.pts[i].tempC;
      int32_t dTemp = int32_t(p.pts[i + 1].tempC) - p.pts[i].tempC;
      return uint16_t(p.pts[i].tempC + (dTemp * int32_t(t_s - p.pts[i].t_s)) / span);
    }
  }
  return p.pts[p.n - 1].tempC;
}

static void drawReferenceCurve(const Profile& p) {
  Ucg::setColor(0, REF_COLOR[0], REF_COLOR[1], REF_COLOR[2]);
  for (uint8_t col = 0; col < Ucg::kWidth; ++col) {
    uint16_t t_s = uint16_t((uint32_t(col) * p.totalS) / (Ucg::kWidth - 1));
    Ucg::drawPixel(col, yForTemp(int16_t(targetTempAt(p, t_s))));
  }
}

static void printNum(int32_t n) {
  if (n < 0) { Ucg::print('-'); n = -n; }
  char buf[11]; uint8_t i = 10; buf[10] = '\0';
  if (!n) { Ucg::print('0'); return; }
  while (n && i) { buf[--i] = '0' + (n % 10); n /= 10; }
  Ucg::print(&buf[i]);
}

// ── PID + SSR time-proportioning control ─────────────────────────────────────
// Gains below are placeholders, not tuned against a real oven — the thermal
// mass/lag of an actual heater+PCB stack needs real-world tuning (start with
// Kp, add Ki once the steady-state offset is visible, add Kd last to tame
// overshoot). Output is a 0-100% duty cycle; a real SSR can't be fast-PWMed,
// so it's applied via time-proportioning: ON for duty% of each PID_WINDOW_MS
// window rather than actually switching at PID_HZ.
struct PID {
  float kp, ki, kd, maxOut;
  float integral   = 0;
  float prevError  = 0;

  float update(float target, float current, float dt_s) {
    float error = target - current;
    integral += error * dt_s;
    float iTerm = ki * integral;
    if (iTerm > maxOut)     { iTerm = maxOut; integral = iTerm / ki; }
    else if (iTerm < 0.0f)  { iTerm = 0.0f;   integral = 0.0f; }
    float dTerm = kd * (error - prevError) / dt_s;
    prevError = error;
    float out = kp * error + iTerm + dTerm;
    if (out > maxOut) out = maxOut;
    if (out < 0.0f)   out = 0.0f;
    return out;
  }
};

static constexpr float    SAFETY_MAX_C   = 280.0f;  // hard cutoff regardless of PID
static constexpr uint16_t PID_WINDOW_MS  = 2000;    // SSR on/off window
PID      pid{/*kp*/4.0f, /*ki*/0.02f, /*kd*/6.0f, /*maxOut*/100.0f};
float    dutyPercent  = 0;
uint32_t windowStartMs;

// Slices the current PID_WINDOW_MS window into an ON/OFF period per dutyPercent.
// Call every loop tick (not just once per control update) so the SSR actually
// switches mid-window.
static void driveHeater() {
  uint32_t now = SysTick::millis();
  if (now - windowStartMs >= PID_WINDOW_MS) windowStartMs = now;
  uint32_t dutyMs = uint32_t(dutyPercent * (PID_WINDOW_MS / 100.0f));
  if ((now - windowStartMs) < dutyMs) Heater::on(); else Heater::off();
}

// ── Run modes ─────────────────────────────────────────────────────────────────
using RunFn = bool(*)();
RunFn activeRun;

bool mainRun() {
  nav.in(in);
  if (nav.changed(display) || nav.navMode() != NavMode::Nav) {
    display.lockMode(LockMode::None);
    display.clear();
    nav.printTo(display);
    nav.sync(display);
  }
  return running;
}

uint32_t processStartMs;
bool     processDone;

void showMenu() {
  Heater::off();
  activeRun = mainRun;
  display.lockMode(LockMode::None);
  display.clear();
  nav.printTo(display);
}

bool processRun() {
  BtnHW::available();              // polls Hold's time threshold
  if (BtnHW::pending()) {           // any click or hold — stop and return to menu
    BtnHW::take();
    showMenu();
    return running;
  }

  static SysTick::Period<1000> tick;
  if (tick()) {
    tick.reset();
    const Profile& p = profiles[profileIdx];
    uint16_t elapsed_s = uint16_t((SysTick::millis() - processStartMs) / 1000);

    if (!processDone) {
      auto r = Tc::read();
      if (r.ok) {
        float currentC = r.tc_raw / 4.0f;
        uint8_t col = uint8_t((uint32_t(elapsed_s) * (Ucg::kWidth - 1)) / p.totalS);
        if (col >= Ucg::kWidth) col = Ucg::kWidth - 1;
        Ucg::setColor(0, LIVE_COLOR[0], LIVE_COLOR[1], LIVE_COLOR[2]);
        Ucg::drawPixel(col, yForTemp(int16_t(r.tc_raw / 4)));

        if (currentC >= SAFETY_MAX_C) dutyPercent = 0;  // hard cutoff, bypasses PID
        else dutyPercent = pid.update(float(targetTempAt(p, elapsed_s)), currentC, 1.0f);
      } else {
        dutyPercent = 0;  // sensor fault — heater off
      }
    } else {
      dutyPercent = 0;  // profile finished — let it cool
    }

    Ucg::setCursor(0, 0);
    Ucg::print(p.name); Ucg::print(' ');
    if (elapsed_s >= p.totalS) {
      processDone = true;
      Ucg::print("Done - press btn");
    } else {
      printNum(elapsed_s); Ucg::print('/'); printNum(p.totalS);
      Ucg::print(" H:"); printNum(int32_t(dutyPercent)); Ucg::print("%  ");
    }
  }

  driveHeater();  // every tick, not just once a second — this is what actually
                   // slices dutyPercent into the SSR's on/off window.
  return running;
}

bool action::start(Sz) {
  activeRun       = processRun;
  processStartMs  = SysTick::millis();
  windowStartMs   = processStartMs;
  processDone     = false;
  dutyPercent     = 0;
  pid.integral    = 0;
  pid.prevError   = 0;
  Ucg::clear();
  drawReferenceCurve(profiles[profileIdx]);
  return true;
}

// ── Setup / loop ──────────────────────────────────────────────────────────────
void setup() {
  Board::begin();
  Ucg::begin();
  Tc::begin();
  EncHW::begin();
  BtnHW::begin();
  Heater::begin();
  Heater::off();

  display.lockMode(LockMode::None);
  display.clear();
  activeRun = mainRun;
  nav.printTo(display);
}

bool run() {
  static SysTick::Period<30> fps;
  if (fps) {
    fps.reset();
    activeRun();
  }
  if (!fps) hw::delay_ms(fps.when() - hw::millis());
  return running;
}

int main() {
  setup();
  while (run());
}
