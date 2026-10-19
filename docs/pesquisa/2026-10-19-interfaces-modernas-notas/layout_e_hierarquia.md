# Layout and information hierarchy of simple / assistant-driven audio plugins (2022-2026)

Scope note: research was limited to ~17 web searches and fetches. Several vendor manuals and reviews could not be fetched in full (truncated pages), so claims below are based on search-result summaries unless marked "fetched". Items marked **Gap** were not verified. Screenshots were not inspected, so all layout descriptions are from text sources.

## Q1. How do the listed plugins structure their main screen?

### Takeaway
The simplest tools (Soundgoodizer, Smooth Operator, LANDR, pure:limit) put one dominant control or a short list of modes on the main screen and hide everything else. The assistant-style tools (Ozone Master Assistant, smart:limit) put a larger chain or analysis panel on screen and rely on a sensible default to keep it readable. Dense, information-heavy screens (Pro-L 2, smart:limit) are praised when metering is organized into one view.

### Cited Findings
- **FL Studio Soundgoodizer:** described as a "one-knob plugin" with a single wet/dry knob that balances original against processed sound, four presets labeled A-D, and the text "Powered By Maximus" at the top — [Slooply](https://slooply.com/blog/fl-studios-soundgoodizer-explained-how-it-works/)
- Same plugin: "four preset buttons and one Amount knob, with no hidden panel or advanced settings" — [Audeobox](https://www.audeobox.com/learn/fl-studio/how-to-use-soundgoodizer-in-fl-studio/). Note the sources differ on whether presets are buttons or labeled A-D; both agree on one main knob and no advanced panel.
- Soundgoodizer's knob is a front for FL Studio's Maximus multiband processor, combining compression, limiting, saturation and stereo manipulation into one wet/dry control — [Audeobox](https://www.audeobox.com/learn/fl-studio/how-to-use-soundgoodizer-in-fl-studio/)
- Slooply advises setting input level with an external tool (channel volume or Fruity Balance) rather than inside the plugin — [Slooply](https://slooply.com/blog/fl-studios-soundgoodizer-explained-how-it-works/)
- **LANDR Mastering Plugin:** three basic modes (Warm, Balanced, Open), plus a 3-band EQ, a presence control, a de-esser, and a frequency spectrum display — [GearNews](https://www.gearnews.com/?p=165502) (search summary)
- LANDR has no manual; controls are explained by tooltips embedded in the GUI, and the user plays the loudest part of the song with no genre input — [Mix Online](https://www.mixonline.com/technology/landr-mastering-plug-in-a-mix-real-world-review) (search summary)
- **Baby Audio Smooth Operator (original):** the main control is a curve. The middle node is Threshold (dragged up or down), with four outer nodes shaping the curve around it; priced at $69 — [MusicRadar](https://www.musicradar.com/reviews/baby-audio-smooth-operator) (search summary; page fetch was truncated)
- **Smooth Operator Pro:** a large global Threshold knob adjusts all threshold nodes at once; the Pro version replaced the 2021 original — [Mix Online](https://www.mixonline.com/technology/reviews/plug-ins/baby-audio-smooth-operator-pro-a-mix-real-world-review) (search summary)
- **iZotope Ozone 11 Master Assistant:** analyzes the track, optionally matches a reference, and builds a module chain (EQ, Dynamic EQ, multiband dynamics, Imager, Exciter, Maximizer) that can be adjusted by hand; modules can be used alone or in sequence — [Violet Recording](https://violetrecording.com/izotope-ozone-11-review/) (fetched)
- Ozone meters include integrated LUFS, true peak and spectrum; Tonal Balance visual guides show low/mid/high energy against genre ranges — [Violet Recording](https://violetrecording.com/izotope-ozone-11-review/) (fetched)
- **Sonible smart:limit:** a combined limiter with a Distortion Monitoring graph, Constant Gain option, and loudness/dynamics monitoring. Mix found the interface "crowded but well designed" and said the GUI can be enlarged to about 35% — [Mix Online](https://www.mixonline.com/technology/reviews/plug-ins/sonible-smartlimit-a-real-world-review) (search summary). MusicTech praised a "clear GUI and visual feedback" but said the distortion graph is small — [MusicTech](https://musictech.com/reviews/plug-ins/sonible-smartlimit-review/) (search summary)
- **Sonible pure:limit:** a simpler sibling with a drop-down menu and a pair of Style and Inflate parameters — [MusicRadar](https://www.musicradar.com/news/sonible-pure-limit-ai-plugin) (launch news, search summary)
- **Mastering The Mix LEVELS:** small, non-resizable window with a central scope surrounded by mode buttons (Headroom, Dynamic Range, Stereo Field, Bass Space; some sources add mono compatibility). A button turns red when another mode detects a problem — [MusicTech](https://www.musictech.net/reviews/levels-review/) (search summary). Bass Space requires the user to mute kick and bass first — same source.
- **FabFilter Pro-L 2:** the interface is designed to eliminate clutter; the main display shows input (gray/light blue), output (dark blue) and gain reduction (red) levels with a thin loudness line — [FabFilter help](https://www.fabfilter.com/help/pro-l/using/overview) (search summary). Tape Op calls it "one of the most pleasing graphical user interfaces on the market" — [Tape Op](https://tapeop.com/reviews/gear/127/pro-l-2-limiter-plug-in) (search summary)
- **Waves OneKnob series:** seven plugins, each with one knob — [Sound On Sound](https://www.soundonsound.com/reviews/waves-oneknob) (search summary)
- **Kelvin Tone Shaper (Tone Projects):** not one-knob. Mixdown calls it "not overloaded with switches and knobs" and lists input/output, a mix knob and a four-position spread switch — [Mixdown](https://mixdownmag.com.au/reviews/review-tone-projects-kelvin-tone-shaper/) (search summary). GearNews lists eight transformer/tube/diode models and a 3-band EQ — [GearNews](https://www.gearnews.com/tone-projects-kelvin-tone-shaper-a-dual-stage-saturation-and-passive-eq-plug-in) (search summary)

### Inferences
- The "one knob" pattern works by hiding a chain behind one control (Soundgoodizer, Smooth Operator Pro). The vendor's internal chain is not exposed on the main screen.
- A product that is simple in the main view still gets a second layer (LANDR's EQ/presence/de-esser; Ozone's module rack; smart:limit's graphs). The second layer is visible at once in some cases, not always hidden.

### Gaps
- Window sizes (in pixels) were not verified for any listed plugin except LEVELS (non-resizable) and smart:limit (enlargeable to ~35%).
- Xfer OTT: a search returned only streaming-video (OTT) UI articles. No reliable review of Xfer's OTT interface was found.
- Not researched in this pass: Neutron, Kilohearts, Minimal Audio, Output (Portal, Thermal), Cableguys ShaperBox, Arturia FX, Baby Audio Spaced Out / IHNY-2, Tone Projects Unlimited, Waves One Knob layout details, Arturia and Output screenshots.

## Q2. Which plugins use one dominant control, and how is the advanced layer revealed?

### Takeaway
Soundgoodizer (one Amount knob, no hidden panel) and Smooth Operator Pro (one global Threshold knob) are the clearest single-control examples. The advanced layer is either absent (Soundgoodizer) or shown in a fixed secondary area (LANDR, Ozone module chain). Pro-L 2 keeps advanced options "tucked away" to keep the main view simple.

### Cited Findings
- Soundgoodizer: "no hidden panel or advanced settings" — [Audeobox](https://www.audeobox.com/learn/fl-studio/how-to-use-soundgoodizer-in-fl-studio/)
- Waves OneKnob: each plugin has a single knob; reviewers say it is easy to set up by inserting it and adjusting until the effect is audible — [Sound On Sound](https://www.soundonsound.com/reviews/waves-oneknob) (search summary)
- Waves OneKnob criticism: limited control means "not very much control" — [Digital Recording Arts](https://digitalrecordingarts.com/waves-oneknob-plugin-bundle-review/) (search summary)
- Smooth Operator (original): MusicRadar criticizes "limited advanced parameter settings" — [MusicRadar](https://www.musicradar.com/reviews/baby-audio-smooth-operator) (search summary)
- Pro-L 2: MusicRadar notes that advanced controls are tucked away, keeping the main view simple — [MusicRadar](https://www.musicradar.com/reviews/tech/fabfilter-pro-l-362414) (search summary; this URL may be the original Pro-L review, not Pro-L 2, unverified)
- Pro-L 2 criticism: no knee control and no DC filter — same source (see note above)

### Inferences
- Reviewers see a single control as a strength for beginners and a limit for engineers. Several reviews ask for a deeper parameter set beneath the simple view (LANDR per Mix; Smooth Operator per MusicRadar).

### Gaps
- No verified source describes exactly how each plugin reveals its advanced layer (for example a toggle, a separate tab, or a gear icon). Needs screenshot or manual check.

## Q3. How do they show assistant/analysis state and loudness meters?

### Takeaway
Assistant state is shown mostly through the result (a chain or a set of values) rather than a detailed progress display. Ozone's assistant takes about 8 seconds, with a CPU spike. Loudness meters are most often shown as LUFS, true peak and a loudness line, with Pro-L 2 combining these on one screen.

### Cited Findings
- Ozone Master Assistant analysis takes about 8 seconds and briefly raised CPU usage by roughly 40% — [GearNews](https://gearnews.com/izotope-ozone-11-review-still-the-industry-standard-for-mastering) (search summary)
- Ozone Assistant output: a "sensible, conservative chain"; "treat the chain as a first draft" and bypass modules to hear each one — [Violet Recording](https://violetrecording.com/izotope-ozone-11-review/) (fetched)
- **Conflict:** a second summary of the same Violet Recording page says the Assistant "frequently overcooks the low-mid EQ and pushes the maximizer harder than a streaming target needs", and recommends pulling loudness back to about -14 LUFS with true peak at or below -1 dBTP — [Violet Recording](https://violetrecording.com/izotope-ozone-11-review/) (search summary). These two accounts of the same article do not agree and should be checked against the full text.
- Pro-L 2 output meter offers Loudness, K-12, K-14 and K-20 scales plus a true-peak toggle — [FabFilter help](https://www.fabfilter.com/help/pro-l/using/overview) (search summary; attribution of this exact detail is uncertain)
- Pro-L 2 Loudness Metering shows limiting over time, release behavior, peaks, momentary loudness, minimum values, target level and target range on one screen; an Infinite mode builds a loudness profile for a whole track — [Tape Op](https://tapeop.com/reviews/gear/127/pro-l-2-limiter-plug-in) (search summary)
- Pro-L 2 has an inter-sample peak option; MusicRadar found many sources exceeded digital maximum with it enabled at 44.1 kHz — [MusicRadar](https://www.musicradar.com/reviews/tech/fabfilter-pro-l-362414) (search summary; same URL caveat)
- smart:limit's Distortion Monitoring graph is small, per MusicTech — [MusicTech](https://musictech.com/reviews/plug-ins/sonible-smartlimit-review/) (search summary)
- LEVELS: a mode button turns red as a warning, even when another mode is on screen — [MusicTech](https://www.musictech.net/reviews/levels-review/) (search summary)
- LANDR: a frequency spectrum is shown; the process takes about 30 seconds — [GearNews](https://www.gearnews.com/?p=165502), [Mix Online](https://www.mixonline.com/technology/landr-mastering-plug-in-a-mix-real-world-review) (search summary)

### Inferences
- The clearest "assistant state" pattern is a result shown immediately after a single action, with no visible multi-step progress. Only Ozone's ~8 s wait is reported, and no source describes a progress bar or "learning" state.
- Target vs measured values are shown in Pro-L 2 (target range) and Ozone (Tonal Balance guides); no source confirmed that the assistant shows a target-vs-measured readout in its own panel.

### Gaps
- No verified description of a progress indicator, "learning" state, or Freeze/Analyze/Auto mode display in any plugin. This needs direct testing or screenshots.
- No verified source on how Soundgoodizer or LANDR display gain reduction or loudness numerically.

## Q4. Recurring patterns (window size, resizing, presets, A/B, bypass, undo)

### Takeaway
Reviewers mention fixed or small windows (LEVELS non-resizable; smart:limit enlargeable), preset drop-downs or simple modes (pure:limit, LANDR), and embedded tooltips in place of manuals (LANDR). A/B, bypass and undo were not covered by any source fetched in this pass.

### Cited Findings
- LEVELS: small, non-resizable window — [MusicTech](https://www.musictech.net/reviews/levels-review/) (search summary)
- smart:limit: GUI can be enlarged to about 35% — [Mix Online](https://www.mixonline.com/technology/reviews/plug-ins/sonible-smartlimit-a-real-world-review) (search summary)
- pure:limit: drop-down menu and Style/Inflate parameters — [MusicRadar](https://www.musicradar.com/news/sonible-pure-limit-ai-plugin) (search summary)
- LANDR: tooltips in the GUI replace a manual — [Mix Online](https://www.mixonline.com/technology/landr-mastering-plug-in-a-mix-real-world-review) (search summary)
- Ozone: module chains can be saved and reused across projects — [Violet Recording](https://violetrecording.com/izotope-ozone-11-review/) (fetched)
- Ozone reviewer advice: bypass modules to hear what each one does — [Violet Recording](https://violetrecording.com/izotope-ozone-11-review/) (fetched)
- Smooth Operator design praised for visuals — [MixdownMag](https://mixdownmag.com.au/reviews/review-baby-audio-smooth-operator/) (search summary); rated 9 with "easy to use" — [MusicTech](https://musictech.com/reviews/baby-audio-smooth-operator-review/) (search summary), with a warning that it is "tempting to overuse"
- Soundgoodizer criticism: polarizing; experienced producers avoid it because it "gives up the precise control that mixing needs" — [Audeobox](https://www.audeobox.com/learn/fl-studio/how-to-use-soundgoodizer-in-fl-studio/) (search summary)

### Inferences
- Reviewers value a fast first result but ask for a way to see and override what the assistant did (Ozone bypass advice, LANDR's requested deeper parameters).

### Gaps
- A/B buttons, bypass placement and undo behavior were not verified for any listed plugin.
- No verified window dimensions in pixels for any plugin.
- Design-case-study sources (Behance, Dribbble) were not searched in this pass.

## What reviewers say makes them feel simple (summary)
- One main control, or a few clear modes, with the rest hidden or absent (Soundgoodizer, Smooth Operator Pro, LANDR) — Audeobox, Mix Online, MusicRadar (search summaries)
- Tooltips instead of a manual (LANDR) — Mix Online (search summary)
- A single loudness screen rather than several meters (Pro-L 2) — Tape Op (search summary)
- Clear visual feedback and a graph that shows the processing (smart:limit) — MusicTech (search summary)
- Restraint: reviewers warn these tools are easy to overuse, so the design should discourage extreme settings (Smooth Operator, Soundgoodizer) — MusicTech, Audeobox (search summaries)

## Open items for the Finale design
- Check Violet Recording's Ozone review directly, because the two summaries conflict on whether the assistant overcooks low-mid EQ.
- Verify Soundgoodizer's preset selection (buttons vs A-D) by looking at a screenshot.
- Look for the "learning" or progress state of Ozone, LANDR or smart:limit in video walkthroughs; none was found in text sources.
- Research the remaining plugins in the gap list, and Xfer OTT under a more specific query (for example "OTT preset Serum" or "Xfer Records OTT plugin review").
