/**
 * @brief u8g2-as-Transport proof of concept — OneMenu drawn through a
 * u8g2 SPI SSD1306 driver using the smallest ("_1") tile buffer, so every
 * redraw runs the whole menu tree once per 8px tile (FirstPage/NextPage).
 * See oneIO/display/u8g2Spi.h for the HAL glue.
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
#include <oneIO/display/u8g2Spi.h>

using namespace hapi;
using namespace oneMenu;
using namespace oneData;
using namespace onePin;
using namespace oneBit;
using namespace hw::avr;
using namespace oneIO::display;

// ── Board / SysTick ───────────────────────────────────────────────────────────
using SysTick = chip::SysTick0<>;
using Board   = AVR::Board<Boot<SysTick>>;
#ifdef IOP
IOP_TIMER0_ISR(Board)
#endif

namespace hw { uint32_t millis() { return SysTick::millis(); } }
unsigned long millis() { return (unsigned long)SysTick::millis(); }

// ── Hardware ──────────────────────────────────────────────────────────────────
using Spi = chip::SpiMaster;

using OledCs  = AVR::OutPin<Pins<PB2>, chip::PortB>;
using OledDc  = AVR::OutPin<Pins<PB1>, chip::PortB>;
using OledRst = AVR::OutPin<Pins<PB0>, chip::PortB>;

// "_1" tile Setup — 8px-row buffer, most redraw passes, least RAM.
using Oled = U8g2SpiDisplay<u8g2_Setup_ssd1306_128x64_noname_1, Spi, OledCs, OledDc, OledRst>;

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
OledDisplay<Oled> display;

// ── Menu ──────────────────────────────────────────────────────────────────────
namespace text {
  static constexpr CText title  {"u8g2 tile demo"};
  static constexpr CText count  {"Count"};
}

using Count = NumFieldDef<
  AsLabel<StaticText<&text::count>>,
  NumField<
    StaticNumRange<StaticRange<0,100,false>>,
    AsField<Watch<Default<Int,10>>>
  >
>;

auto mainMenu = menuDef<WrapNav>(
  ItemDef<StaticText<&text::title>>{},
  staticBody(
    Count{}
  )
);

NavDef<TreeNav, Root<decltype(mainMenu), mainMenu>> nav;

// ── Run ───────────────────────────────────────────────────────────────────────
bool running = true;

void setup() {
  Board::begin();
  Oled::begin();
  EncHW::begin();
  BtnHW::begin();

  display.lockMode(LockMode::None);
  Oled::renderPages([]{ nav.printTo(display); });
}

bool run() {
  static SysTick::Period<30> fps;
  nav.in(in);
  if (fps) {
    fps.reset();
    if (nav.changed(display) || nav.navMode() != NavMode::Nav) {
      display.lockMode(LockMode::None);
      Oled::renderPages([]{ nav.printTo(display); });
      nav.sync(display);
    }
  }
  if (!fps) hw::delay_ms(fps.when() - hw::millis());
  return running;
}

int main() {
  setup();
  while (run());
}
