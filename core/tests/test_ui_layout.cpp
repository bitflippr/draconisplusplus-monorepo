#include <boost/ut.hpp>
#include <sstream>

#include <Drac++/Utils/Types.hpp>

#include "../src/CLI/Core/SystemInfo.hpp"
#include "../src/CLI/UI/UI.hpp"

using namespace draconis::utils::types;
using draconis::core::system::SystemInfo;
using draconis::ui::CreateUI;
using draconis::ui::detail::GetVisualWidth;
using draconis::ui::detail::TruncateToWidth;
using draconis::ui::detail::WordWrap;

namespace {
  auto SplitLines(const String& text) -> Vec<String> {
    Vec<String>       lines;
    std::stringstream stream(text);
    String            line;

    while (std::getline(stream, line, '\n'))
      lines.push_back(line);

    if (!lines.empty() && lines.back().empty())
      lines.pop_back();

    return lines;
  }

  auto WidestLine(const String& text) -> usize {
    usize widest = 0;

    for (const String& line : SplitLines(text))
      widest = std::max(widest, GetVisualWidth(line));

    return widest;
  }

  // Width of the bordered box alone, ignoring any logo to its left.
  auto BoxWidth(const String& text) -> usize {
    for (const String& line : SplitLines(text))
      if (const usize corner = line.find("╭"); corner != String::npos)
        return GetVisualWidth(StringView(line).substr(corner));

    return 0;
  }

  auto Contains(const String& haystack, const StringView needle) -> bool {
    return haystack.contains(needle);
  }
} // namespace

auto main() -> int {
  using namespace boost::ut;

  draconis::config::Config             config {};
  draconis::utils::cache::CacheManager cache;

  // A fixed layout keeps the assertions independent of the host's readouts. The
  // long label override pins the natural box well above the narrow-terminal
  // floor, so the widths where a logo competes with the box are actually
  // exercised rather than skipped.
  config.ui.layout = {
    { .name = "system",
     .rows = { { .key = "date", .label = "A Deliberately Long Row Label For Layout" },
                { .key = "kernel" },
                { .key = "shell" } } },
    {   .name = "misc",                          .rows = { { .key = "uptime" } } }
  };

  const SystemInfo info(cache, config);

  "Visual width ignores ANSI escape sequences"_test = [] -> void {
    expect(GetVisualWidth("plain") == 5_ul);
    expect(GetVisualWidth("\033[38;5;6mplain\033[0m") == 5_ul);
    expect(GetVisualWidth("") == 0_ul);
  };

  "Truncation fits the requested width and marks the cut"_test = [] -> void {
    expect(TruncateToWidth("short", 10) == String("short"));

    const String cut = TruncateToWidth("a rather long value", 8);
    expect(GetVisualWidth(cut) <= 8_ul);
    expect(Contains(cut, "…"));

    // A styled string must not leak its color past the cut.
    const String styledCut = TruncateToWidth("\033[38;5;6mlong enough to cut\033[0m", 6);
    expect(GetVisualWidth(styledCut) <= 6_ul);
    expect(styledCut.ends_with("\033[0m"));
  };

  "Word wrapping never exceeds the wrap width"_test = [] -> void {
    for (const usize width : { 4UZ, 7UZ, 12UZ, 40UZ })
      for (const String& line : WordWrap("NVIDIA GeForce RTX 3050 Laptop GPU", width))
        expect(GetVisualWidth(line) <= width) << "width " << width << " line '" << line << "'";
  };

  "Tokens longer than the line are broken instead of overflowing"_test = [] -> void {
    const Vec<String> wrapped = WordWrap("supercalifragilisticexpialidocious", 8);

    expect(wrapped.size() > 1_ul);

    for (const String& line : wrapped)
      expect(GetVisualWidth(line) <= 8_ul);
  };

  "Layout fits inside the requested terminal width"_test = [&] -> void {
    for (const usize width : { 16UZ, 24UZ, 30UZ, 45UZ, 60UZ, 78UZ, 100UZ, 160UZ }) {
      const String rendered = CreateUI(config, info, false, width);

      expect(WidestLine(rendered) <= width) << "width " << width << " produced " << WidestLine(rendered);
    }
  };

  "A width of zero opts out of adapting the layout"_test = [&] -> void {
    const String unconstrained = CreateUI(config, info, false, 0);
    const String narrow        = CreateUI(config, info, false, 40);

    expect(WidestLine(unconstrained) > 40_ul);
    expect(WidestLine(narrow) <= 40_ul);
  };

  "Disabling responsive layout ignores the terminal size"_test = [&] -> void {
    draconis::config::Config fixed = config;
    fixed.ui.responsive            = false;

    // No override and no adaptation: the box keeps its natural width.
    expect(WidestLine(CreateUI(fixed, info, true, None)) == WidestLine(CreateUI(fixed, info, true, 0)));

    // An explicit width still wins, so scripted output stays reproducible.
    expect(WidestLine(CreateUI(fixed, info, true, 40)) <= 40_ul);
  };

  "The logo is dropped rather than crushing the box"_test = [&] -> void {
    const usize natural = BoxWidth(CreateUI(config, info, true, 0));

    // Given room for both, the logo sits beside a box of its natural width.
    const String roomy = CreateUI(config, info, false, 400);
    expect(WidestLine(roomy) > natural);
    expect(BoxWidth(roomy) == natural);

    // Given room for exactly the box, the logo goes and the box is untouched.
    const String snug = CreateUI(config, info, false, natural);
    expect(WidestLine(snug) == natural);
    expect(SplitLines(snug).at(0).starts_with("╭"));
  };

  "Neither the output nor the box ever shrinks as the terminal grows"_test = [&] -> void {
    // The box is sized from the terminal alone and the logo is dropped whole,
    // so widening the terminal can never narrow either one. Squeezing the box
    // to keep the logo breaks this: the box would dip at the width where the
    // logo starts fitting, then climb back as the terminal grows.
    usize previousLine = 0;
    usize previousBox  = 0;

    for (usize width = 16; width <= 200; ++width) {
      const String rendered = CreateUI(config, info, false, width);
      const usize  line     = WidestLine(rendered);
      const usize  box      = BoxWidth(rendered);

      expect(line >= previousLine) << "output narrowed from " << previousLine << " to " << line
                                   << " going from width " << (width - 1) << " to " << width;
      expect(box >= previousBox) << "box narrowed from " << previousBox << " to " << box
                                 << " going from width " << (width - 1) << " to " << width;

      previousLine = line;
      previousBox  = box;
    }
  };

  "The box holds its natural width until the terminal forces it narrower"_test = [&] -> void {
    // Every width at or above the natural box width must render that same box,
    // whether or not the logo also fits. Squeezing the box to make room for the
    // logo would show up here as a narrower box at some width in this range.
    const usize natural = BoxWidth(CreateUI(config, info, true, 0));

    for (usize width = natural; width <= natural + 60; ++width)
      expect(BoxWidth(CreateUI(config, info, false, width)) == natural)
        << "width " << width << " rendered a " << BoxWidth(CreateUI(config, info, false, width))
        << "-wide box instead of its natural " << natural;
  };

  "Very narrow terminals fall back to a borderless list"_test = [&] -> void {
    const String rendered = CreateUI(config, info, false, 16);

    expect(WidestLine(rendered) <= 16_ul);
    expect(!Contains(rendered, "╭"));
    expect(!Contains(rendered, "│"));
  };

  return 0;
}
