/**
 * @brief azoreanFlow building-block demo — OneMenu picks a solder profile,
 * "Start" hands the screen to a process that plots the profile curve and
 * overlays the live thermocouple trace. Any button click/hold stops the
 * process and returns to the menu.
 */

// OneInput's Debounce/Click/Hold call millis() as a non-dependent name —
// declare it globally before headers are pulled in so phase-1 lookup succeeds.
#if defined(__AVR__) && defined(IOP)
  unsigned long millis();
#endif

#include <oneMenu/oneMenu.h>
#include <oneMenu/menu/IO/IOP/encIn.h>
#include <oneMenu/menu/IO/IOP/btnIn.h>
#include <oneMenu/menu/IO/IOP/oledOut.h>
#include <oneInput/oneInput.h>
#include <chips/avr/avrDevice.h>
#include <oneIO/display/st7735.h>
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

// ST7735 TFT: CS=D10(PB2), DC=D9(PB1), RST=D8(PB0).
using TftCs  = AVR::OutPin<Pins<PB2>, chip::PortB>;
using TftDc  = AVR::OutPin<Pins<PB1>, chip::PortB>;
using TftRst = AVR::OutPin<Pins<PB0>, chip::PortB>;
using Tft    = SpiSt7735<Spi, TftCs, TftDc, TftRst>;

// MAX31855 thermocouple amplifier: CS=D7(PD7), shares the SPI bus with the TFT.
using TcCs = AVR::OutPin<Pins<PD7>, chip::PortD>;
using Tc   = MAX31855<Spi, TcCs>::Api;

// Rotary encoder: A=A0(PC0), B=A1(PC1); push switch SW=A2(PC2). PCINT1 group.
using EncHW = hapi::APIOf<oneInput::InputDef,
  oneInput::Encoder,
  oneInput::avr::AvrEncPins</*group*/1, chip::PortC, /*bitA*/0, /*bitB*/1>
>;
using BtnHW = hapi::APIOf<oneInput::InputDef,
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
OledDisplay<Tft> display;

// ── Solder profiles ───────────────────────────────────────────────────────────
// A handful of (time, target-°C) breakpoints — enough to draw a recognisable
// reflow curve (preheat/soak/reflow/cool). Linear interpolation fills the rest.
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
  static constexpr CText title    {"Reflow Demo"};
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
// Header text on page 0; plot fills the remaining pages. Temp axis 0-260°C,
// hottest at the top. fillRect() is page-granular (8px), so the plot reads as
// a coarse scope trace rather than a smooth line — plenty for this showcase.
static constexpr uint8_t  PLOT_W        = Tft::kWidth;
static constexpr uint8_t  PLOT_TOP_PAGE = 1;
static constexpr uint8_t  PLOT_PAGES    = (Tft::kHeight / 8) - PLOT_TOP_PAGE;
static constexpr int16_t  MAX_T         = 260;

static uint8_t pageForTemp(int16_t tC) {
  if (tC < 0)     tC = 0;
  if (tC > MAX_T) tC = MAX_T;
  uint16_t span = PLOT_PAGES - 1;
  return uint8_t(PLOT_TOP_PAGE + (uint16_t(MAX_T - tC) * span) / MAX_T);
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
  for (uint8_t col = 0; col < PLOT_W; ++col) {
    uint16_t t_s = uint16_t((uint32_t(col) * p.totalS) / (PLOT_W - 1));
    Tft::fillRect(col, pageForTemp(int16_t(targetTempAt(p, t_s))), 1, 1, 0xFF);
  }
}

static void printNum(int32_t n) {
  if (n < 0) { Tft::print('-'); n = -n; }
  char buf[11]; uint8_t i = 10; buf[10] = '\0';
  if (!n) { Tft::print('0'); return; }
  while (n && i) { buf[--i] = '0' + (n % 10); n /= 10; }
  Tft::print(&buf[i]);
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
        uint8_t col = uint8_t((uint32_t(elapsed_s) * (PLOT_W - 1)) / p.totalS);
        if (col >= PLOT_W) col = PLOT_W - 1;
        Tft::fillRect(col, pageForTemp(int16_t(r.tc_raw / 4)), 1, 1, 0xFF);
      }
    }

    Tft::setCursor(0, 0);
    Tft::print(p.name); Tft::print(' ');
    if (elapsed_s >= p.totalS) {
      processDone = true;
      Tft::print("Done - press btn");
    } else {
      printNum(elapsed_s); Tft::print('/'); printNum(p.totalS); Tft::print("s   ");
    }
  }
  return running;
}

bool action::start(Sz) {
  activeRun       = processRun;
  processStartMs  = SysTick::millis();
  processDone     = false;
  Tft::clear();
  drawReferenceCurve(profiles[profileIdx]);
  return true;
}

// ── Setup / loop ──────────────────────────────────────────────────────────────
void setup() {
  Board::begin();
  Tft::begin();
  Tc::begin();
  EncHW::begin();
  BtnHW::begin();

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
