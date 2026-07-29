// RetroLookAndFeel.h — the Casio F91W-inspired skin: black body, amber
// LCD-style text, blocky rectangular bargraph sliders and toggle
// switches instead of JUCE's default rounded/gradient controls.
//
// This is the second pass at this file. The first pass only set basic
// colour IDs and assumed JUCE's default painting would pick them up
// consistently everywhere -- it didn't. Three real, confirmed gaps from
// building the (differently-themed, but structurally identical) sibling
// standalone app, all fixed here the same way:
//   1. A ComboBox's dropdown LIST is a separate PopupMenu component with
//      its own colour IDs, not inherited from ComboBox's own colours --
//      left unset, it fell back to JUCE's default dark theme, which
//      incidentally looked fine against a dark theme by coincidence, but
//      used none of the actual amber/black palette and had mismatched
//      corner/border styling from everything else.
//   2. JUCE's built-in slider value text box has its own multi-layered
//      colour propagation (Slider -> internal Label -> internal
//      TextEditor on click) that proved unreliable to fully theme via
//      colour IDs alone -- replaced with self-drawn value Labels the
//      plugin now fully controls directly (see PluginEditor.h).
//   3. ComboBox's default drawComboBox() draws JUCE's standard rounded/
//      gradient chevron box regardless of the colours set on it -- now
//      explicitly overridden to match the blocky, flat aesthetic used
//      everywhere else in this theme.
#pragma once
#include <JuceHeader.h>

namespace RetroColours
{
    static const juce::Colour background   { 0xff0a0a0a };
    static const juce::Colour bodyBorder    { 0xff1c1c1c };
    static const juce::Colour panelBg       { 0xff111111 };
    static const juce::Colour panelBorder   { 0xff2a2a2a };
    static const juce::Colour switchBorder  { 0xff3a3a3a };
    static const juce::Colour amberBright   { 0xffffb000 }; // values, fills, active thumbs
    static const juce::Colour amberMid      { 0xffc98a00 }; // labels
    static const juce::Colour amberDim      { 0xff8a6000 }; // secondary/hint text
    static const juce::Colour amberDimmer   { 0xff7a5000 }; // least prominent (e.g. channel badge)
}

inline juce::Font retroMonoFont(float size, bool bold = false)
{
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain);
}

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, RetroColours::background);
        setColour(juce::Label::textColourId, RetroColours::amberMid);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        setColour(juce::Slider::textBoxTextColourId, RetroColours::amberBright);
        setColour(juce::Slider::textBoxBackgroundColourId, RetroColours::panelBg);
        setColour(juce::Slider::textBoxOutlineColourId, RetroColours::panelBorder);

        setColour(juce::TextButton::buttonColourId, RetroColours::panelBg);
        setColour(juce::TextButton::textColourOffId, RetroColours::amberMid);

        setColour(juce::ComboBox::backgroundColourId, RetroColours::panelBg);
        setColour(juce::ComboBox::outlineColourId, RetroColours::panelBorder);
        setColour(juce::ComboBox::textColourId, RetroColours::amberBright);
        setColour(juce::ComboBox::arrowColourId, RetroColours::amberMid);

        // Fix #1 -- see class comment.
        setColour(juce::PopupMenu::backgroundColourId, RetroColours::panelBg);
        setColour(juce::PopupMenu::textColourId, RetroColours::amberBright);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, RetroColours::amberBright);
        setColour(juce::PopupMenu::highlightedTextColourId, RetroColours::background);

        // For the self-drawn, self-managed value Labels (editable on
        // click, so they do create an internal TextEditor -- covered
        // directly here rather than relying on Slider's propagation path).
        setColour(juce::TextEditor::backgroundColourId, RetroColours::panelBg);
        setColour(juce::TextEditor::textColourId, RetroColours::amberBright);
        setColour(juce::TextEditor::highlightColourId, RetroColours::amberBright.withAlpha(0.3f));
        setColour(juce::TextEditor::highlightedTextColourId, RetroColours::amberBright);
        setColour(juce::TextEditor::outlineColourId, RetroColours::panelBorder);
        setColour(juce::TextEditor::focusedOutlineColourId, RetroColours::amberBright);
    }

    juce::Font getLabelFont(juce::Label&) override { return retroMonoFont(12.0f); }
    juce::Font getSliderPopupFont(juce::Slider&) override { return retroMonoFont(12.0f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return retroMonoFont(12.0f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return retroMonoFont(12.0f); }

    // Fix #3 -- flat, blocky box instead of JUCE's default rounded/
    // gradient ComboBox, matching the rest of this theme. Chevron drawn
    // directly (no glyph/font dependency, so it can't mis-render the way
    // a raw Unicode escape byte sequence once did elsewhere in this project).
    void drawComboBox(juce::Graphics& g, int width, int height, bool isMouseOver,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, float(width), float(height));
        g.setColour(RetroColours::panelBg);
        g.fillRect(bounds);
        g.setColour(isMouseOver ? RetroColours::amberBright : RetroColours::panelBorder);
        g.drawRect(bounds, 1.0f);

        float cx = width - 20.0f, cy = height * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(cx - 5, cy - 3);
        arrow.lineTo(cx, cy + 3);
        arrow.lineTo(cx + 5, cy - 3);
        g.setColour(RetroColours::amberMid);
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Explicit rather than assumed -- see PluginEditor.h's self-drawn
    // slider value Labels, which need a bordered "LCD box" look that
    // JUCE's default Label painting isn't guaranteed to produce just
    // from colour IDs alone (learned the hard way on the sibling app).
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        auto bounds = label.getLocalBounds().toFloat();
        auto bg = label.findColour(juce::Label::backgroundColourId);
        if (!bg.isTransparent()) {
            g.setColour(bg);
            g.fillRect(bounds);
            g.setColour(RetroColours::panelBorder);
            g.drawRect(bounds, 1.0f);
        }
        if (!label.isBeingEdited()) {
            g.setColour(label.findColour(juce::Label::textColourId));
            g.setFont(retroMonoFont(12.0f));
            g.drawText(label.getText(), label.getLocalBounds().reduced(4, 0), label.getJustificationType(), true);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        g.setColour(RetroColours::panelBg);
        g.fillRect(bounds);
        g.setColour(isButtonDown ? RetroColours::amberBright
                    : isMouseOverButton ? RetroColours::amberDim : RetroColours::panelBorder);
        g.drawRect(bounds, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour(RetroColours::amberMid);
        g.setFont(retroMonoFont(12.0f));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
    }

    // Bargraph style: a filled rectangular bar showing the value, not a
    // thumb-on-a-track -- matches the approved mockup exactly.
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider&) override
    {
        auto track = juce::Rectangle<float>(float(x), float(y + height / 2 - 4), float(width), 8.0f);
        g.setColour(RetroColours::panelBg);
        g.fillRect(track);
        g.setColour(RetroColours::panelBorder);
        g.drawRect(track, 1.0f);

        float lo = juce::jmin(minSliderPos, maxSliderPos);
        float fillWidth = juce::jlimit(0.0f, track.getWidth() - 2.0f, sliderPos - lo);
        auto fill = track.reduced(1.0f).withWidth(fillWidth);
        g.setColour(RetroColours::amberBright);
        g.fillRect(fill);
    }

    // Blocky rectangular switch, not an iOS-style pill -- matches the mockup.
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto switchBounds = juce::Rectangle<float>(bounds.getRight() - 36.0f,
                                                    bounds.getCentreY() - 8.0f, 36.0f, 16.0f);
        g.setColour(RetroColours::panelBg);
        g.fillRect(switchBounds);
        g.setColour(RetroColours::switchBorder);
        g.drawRect(switchBounds, 1.0f);

        auto thumb = switchBounds.reduced(1.0f).withWidth(14.0f);
        if (button.getToggleState())
            thumb = thumb.withX(switchBounds.getRight() - 15.0f);
        g.setColour(RetroColours::amberBright);
        g.fillRect(thumb);

        if (button.getButtonText().isNotEmpty()) {
            g.setColour(RetroColours::amberMid);
            g.setFont(retroMonoFont(11.0f));
            g.drawText(button.getButtonText(), bounds.withWidth(bounds.getWidth() - 44.0f),
                       juce::Justification::centredLeft, true);
        }
    }
};
