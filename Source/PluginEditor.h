// PluginEditor.h — the Casio F91W-styled RetroSurround editor, second
// pass. Visual design unchanged from the approved mockup; what's fixed
// here is the actual rendering reliability, ported from the same fixes
// made to the (differently-themed but structurally identical) sibling
// standalone app:
//   - Slider values are now self-drawn Labels this editor fully
//     controls, not JUCE's built-in slider text box, whose colour
//     propagation through Slider -> internal Label -> internal
//     TextEditor-on-click proved unreliable to theme completely via
//     colour IDs alone.
//   - A Reset button, resetting every parameter to its own built-in
//     default value (via getDefaultValue(), not a second hardcoded copy
//     of those defaults that could drift out of sync with the
//     constructor).
// RetroLookAndFeel.h carries the rest: PopupMenu colours (the dropdown
// list is a separate component from the ComboBox itself) and an explicit
// drawComboBox() override, since JUCE's default box ignored the blocky/
// flat aesthetic even with the right colours set on it.
//
// Channel setup remains an active dropdown (setupCombo) that calls
// proc.requestChannelSetup() directly -- matching the standalone app's
// and the original FreeSurround DSP's UX. Same Legacy Transform toggle
// as before (the one configuration choice that's safely a plugin-internal
// parameter, since it doesn't change channel count).
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

struct SetupChoice { channel_setup cs; const char* name; };
static const SetupChoice kSetupChoices[] = {
    { cs_stereo, "STEREO + LFE (2.1)" },
    { cs_4point1, "4.1 (FRONT+REAR, NO CENTER)" },
    { cs_5point1, "5.1 STANDARD (FRONT+REAR)" },
    { cs_6point1, "6.1 (SIDE+BACK CENTER)" },
    { cs_7point1, "7.1 STANDARD (SIDE+BACK)" },
    { cs_7point1_panorama, "7.1 PANORAMA (5-FRONT+SIDE)" },
    { cs_7point1_tricenter, "7.1 TRI-CENTER (5-FRONT+BACK)" },
};
static constexpr int kNumSetupChoices = sizeof(kSetupChoices) / sizeof(kSetupChoices[0]);

class RetroSurroundEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RetroSurroundEditor(RetroSurroundProcessor& p)
        : AudioProcessorEditor(&p), proc(p)
    {
        setLookAndFeel(&lookAndFeel);
        setSize(700, 540);
        setResizable(true, true);
        // Confirmed from a real screenshot: the editor was never marked
        // resizable, so when a host opened it in a window larger than
        // this fixed size, the content just sat in the top-left corner
        // with the rest of the host's window left empty. Min matches
        // the size this layout was actually designed at; max is
        // generous headroom without inviting an absurdly stretched
        // window that's mostly empty space.
        setResizeLimits(700, 540, 1050, 900);

        titleLabel.setText("RETROSURROUND", juce::dontSendNotification);
        titleLabel.setFont(retroMonoFont(15.0f));
        titleLabel.setColour(juce::Label::textColourId, RetroColours::amberBright);
        addAndMakeVisible(titleLabel);

        channelBadge.setJustificationType(juce::Justification::centredRight);
        channelBadge.setFont(retroMonoFont(11.0f));
        channelBadge.setColour(juce::Label::textColourId, RetroColours::amberDimmer);
        addAndMakeVisible(channelBadge);

        for (int i = 0; i < kNumSetupChoices; i++) setupCombo.addItem(kSetupChoices[i].name, i + 1);
        setupCombo.onChange = [this] {
            int idx = setupCombo.getSelectedId() - 1;
            if (idx >= 0 && idx < kNumSetupChoices)
                proc.requestChannelSetup(kSetupChoices[idx].cs); // host-dependent -- see PluginProcessor.h
        };
        addAndMakeVisible(setupCombo);

        resetButton.setButtonText("RESET");
        resetButton.onClick = [this] { resetToDefaults(); };
        addAndMakeVisible(resetButton);

        legacyLabel.setText("LEGACY XFORM (NO FOCUS CTRL)", juce::dontSendNotification);
        legacyLabel.setFont(retroMonoFont(11.0f));
        addAndMakeVisible(legacyLabel);
        legacyToggle.setToggleState(proc.legacyTransform->get(), juce::dontSendNotification);
        legacyToggle.onClick = [this] { *proc.legacyTransform = legacyToggle.getToggleState(); };
        addAndMakeVisible(legacyToggle);

        addSlider(centerImageLabel, centerImageSlider, centerImageValue, centerImageMin, centerImageMax,
                 "CENTER IMAGE", proc.centerImage, "MIN", "NORMAL");
        addSlider(frontSepLabel, frontSepSlider, frontSepValue, frontSepMin, frontSepMax,
                 "FRONT SEP", proc.frontSeparation, "MONO", "WIDE");
        addSlider(rearSepLabel, rearSepSlider, rearSepValue, rearSepMin, rearSepMax,
                 "REAR SEP", proc.rearSeparation, "MONO", "WIDE");
        addSlider(wrapLabel, wrapSlider, wrapValue, wrapMin, wrapMax,
                 "CIRCULAR WRAP", proc.circularWrap, "0 DEG", "360 DEG");
        addSlider(shiftLabel, shiftSlider, shiftValue, shiftMin, shiftMax,
                 "SHIFT", proc.shift, "FRONT", "REAR");
        addSlider(depthLabel, depthSlider, depthValue, depthMin, depthMax,
                 "DEPTH", proc.depth, "NEAR", "FAR");
        addSlider(focusLabel, focusSlider, focusValue, focusMin, focusMax,
                 "FOCUS", proc.focus, "AMBIENT", "LOCALIZED");
        addSlider(lowCutoffLabel, lowCutoffSlider, lowCutoffValue, lowCutoffMin, lowCutoffMax,
                 "BASS LOW (HZ)", proc.lowCutoffHz);
        addSlider(highCutoffLabel, highCutoffSlider, highCutoffValue, highCutoffMin, highCutoffMax,
                 "BASS HIGH (HZ)", proc.highCutoffHz);

        bassRedirLabel.setText("BASS REDIRECT", juce::dontSendNotification);
        bassRedirLabel.setFont(retroMonoFont(11.0f));
        addAndMakeVisible(bassRedirLabel);
        bassRedirToggle.setToggleState(proc.bassRedirection->get(), juce::dontSendNotification);
        bassRedirToggle.onClick = [this] { *proc.bassRedirection = bassRedirToggle.getToggleState(); };
        addAndMakeVisible(bassRedirToggle);

        updateSetupLabel();
        startTimerHz(2);
    }

    ~RetroSurroundEditor() override { setLookAndFeel(nullptr); }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        auto row = [&](int h) { return area.removeFromTop(h).reduced(0, 2); };
        auto layout = [&](juce::Label& l, juce::Slider& s, juce::Label& v, juce::Label& mn, juce::Label& mx) {
            auto r = row(28);
            l.setBounds(r.removeFromLeft(140));
            v.setBounds(r.removeFromRight(55));
            r.removeFromRight(4);
            mx.setBounds(r.removeFromRight(85));
            mn.setBounds(r.removeFromLeft(85));
            s.setBounds(r);
        };

        { auto r = row(24); titleLabel.setBounds(r.removeFromLeft(280)); channelBadge.setBounds(r); }
        area.removeFromTop(8);
        setupCombo.setBounds(row(28));
        area.removeFromTop(8);
        { auto r = row(26); resetButton.setBounds(r.removeFromLeft(80)); }
        area.removeFromTop(6);
        { auto r = row(24); legacyLabel.setBounds(r.removeFromLeft(230)); legacyToggle.setBounds(r); }
        area.removeFromTop(6);

        layout(centerImageLabel, centerImageSlider, centerImageValue, centerImageMin, centerImageMax);
        layout(frontSepLabel, frontSepSlider, frontSepValue, frontSepMin, frontSepMax);
        layout(rearSepLabel, rearSepSlider, rearSepValue, rearSepMin, rearSepMax);
        layout(wrapLabel, wrapSlider, wrapValue, wrapMin, wrapMax);
        layout(shiftLabel, shiftSlider, shiftValue, shiftMin, shiftMax);
        layout(depthLabel, depthSlider, depthValue, depthMin, depthMax);
        layout(focusLabel, focusSlider, focusValue, focusMin, focusMax);
        layout(lowCutoffLabel, lowCutoffSlider, lowCutoffValue, lowCutoffMin, lowCutoffMax);
        layout(highCutoffLabel, highCutoffSlider, highCutoffValue, highCutoffMin, highCutoffMax);

        area.removeFromTop(4);
        { auto r = row(28); bassRedirLabel.setBounds(r.removeFromLeft(160)); bassRedirToggle.setBounds(r); }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(RetroColours::background);
        g.setColour(RetroColours::bodyBorder);
        g.drawRect(getLocalBounds(), 3);
        g.setColour(RetroColours::bodyBorder);
        g.drawLine(16.0f, 46.0f, float(getWidth() - 16), 46.0f, 1.0f);
    }

private:
    void addSlider(juce::Label& label, juce::Slider& slider, juce::Label& valueLabel,
                   juce::Label& minLabel, juce::Label& maxLabel,
                   const juce::String& text, juce::AudioParameterFloat* param,
                   const juce::String& minText = {}, const juce::String& maxText = {})
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(retroMonoFont(11.0f));
        addAndMakeVisible(label);

        // Min/max descriptive labels -- what each end of the slider
        // actually means, not just a bare number. Confirmed missing
        // against the original FreeSurround's own configuration dialog,
        // which shows exactly this (e.g. "Mono"/"Wide", "Ambient"/
        // "Localized") at both ends of each slider.
        for (auto* l : { &minLabel, &maxLabel }) {
            l->setFont(retroMonoFont(9.0f));
            l->setColour(juce::Label::textColourId, RetroColours::amberDim);
            l->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(l);
        }
        minLabel.setText(minText, juce::dontSendNotification);
        maxLabel.setText(maxText, juce::dontSendNotification);

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(param->range.start, param->range.end, 0.01);
        addAndMakeVisible(slider);

        valueLabel.setFont(retroMonoFont(12.0f));
        valueLabel.setColour(juce::Label::textColourId, RetroColours::amberBright);
        valueLabel.setColour(juce::Label::backgroundColourId, RetroColours::panelBg);
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setEditable(true);
        valueLabel.onTextChange = [&slider, &valueLabel] {
            slider.setValue(valueLabel.getText().getDoubleValue(), juce::sendNotificationSync);
        };
        addAndMakeVisible(valueLabel);

        auto refresh = [&valueLabel](double v) { valueLabel.setText(juce::String(v, 2), juce::dontSendNotification); };
        refresh(param->get());
        slider.setValue(param->get(), juce::dontSendNotification); // fine here -- refresh() above already set the visible text directly
        slider.onValueChange = [&slider, param, refresh] { *param = float(slider.getValue()); refresh(slider.getValue()); };
    }

    void resetToDefaults()
    {
        // Deliberately typed as RangedAudioParameter*, not
        // AudioParameterFloat*/AudioParameterBool* -- confirmed from a
        // real build error that JUCE re-declares getDefaultValue() in a
        // private section of those derived classes, even though the
        // base class declares it public. C++ access control is checked
        // against the static type used at the call site, not the
        // dynamic type, so calling through the base class pointer here
        // (an implicit, always-safe upcast) reaches the public version.
        auto resetParam = [](juce::RangedAudioParameter* p) { p->setValueNotifyingHost(p->getDefaultValue()); };
        resetParam(proc.centerImage);
        resetParam(proc.frontSeparation);
        resetParam(proc.rearSeparation);
        resetParam(proc.circularWrap);
        resetParam(proc.shift);
        resetParam(proc.depth);
        resetParam(proc.focus);
        resetParam(proc.lowCutoffHz);
        resetParam(proc.highCutoffHz);
        resetParam(proc.bassRedirection);
        // Reset now covers everything, including channel setup and the
        // legacy toggle -- deliberately reversing an earlier design
        // choice to leave those alone, per direct feedback that a
        // "reset" which skips two visible controls isn't really a full
        // reset.
        resetParam(proc.legacyTransform);
        for (auto pair : { std::pair<juce::Slider*, juce::AudioParameterFloat*>{&centerImageSlider, proc.centerImage},
                           {&frontSepSlider, proc.frontSeparation}, {&rearSepSlider, proc.rearSeparation},
                           {&wrapSlider, proc.circularWrap}, {&shiftSlider, proc.shift},
                           {&depthSlider, proc.depth}, {&focusSlider, proc.focus},
                           {&lowCutoffSlider, proc.lowCutoffHz}, {&highCutoffSlider, proc.highCutoffHz} })
            pair.first->setValue(pair.second->get(), juce::sendNotificationSync); // triggers each slider's own refresh() via onValueChange
        bassRedirToggle.setToggleState(proc.bassRedirection->get(), juce::dontSendNotification);
        legacyToggle.setToggleState(proc.legacyTransform->get(), juce::dontSendNotification);
        proc.requestChannelSetup(cs_5point1); // the actual default channel setup -- see PluginProcessor.h constructor
        updateSetupLabel(); // immediate UI refresh rather than waiting up to 0.5s for the next timer tick
    }

    void updateSetupLabel()
    {
        auto current = proc.getCurrentSetup();
        int wantId = 0;
        for (int i = 0; i < kNumSetupChoices; i++) {
            // cs_legacy shares 5.1's channel set/id but is driven by the
            // Legacy toggle, not the combo -- treat it as "5.1" here so
            // the combo doesn't fight the toggle for the same slot.
            auto matchCs = kSetupChoices[i].cs;
            if ((current == cs_legacy && matchCs == cs_5point1) || current == matchCs) { wantId = i + 1; break; }
        }
        if (setupCombo.getSelectedId() != wantId)
            setupCombo.setSelectedId(wantId, juce::dontSendNotification);

        channelBadge.setText(juce::String(int(proc.getNumOutputChannels())) + "CH", juce::dontSendNotification);
        bool show5point1Toggle = (current == cs_5point1 || current == cs_legacy);
        legacyLabel.setVisible(show5point1Toggle);
        legacyToggle.setVisible(show5point1Toggle);
    }

    void timerCallback() override { updateSetupLabel(); }

    RetroSurroundProcessor& proc;
    RetroLookAndFeel lookAndFeel;

    juce::Label titleLabel, channelBadge;
    juce::ComboBox setupCombo;
    juce::TextButton resetButton;
    juce::Label legacyLabel;
    juce::ToggleButton legacyToggle;

    juce::Label centerImageLabel, frontSepLabel, rearSepLabel, wrapLabel, shiftLabel,
                depthLabel, focusLabel, lowCutoffLabel, highCutoffLabel, bassRedirLabel;
    juce::Slider centerImageSlider, frontSepSlider, rearSepSlider, wrapSlider, shiftSlider,
                 depthSlider, focusSlider, lowCutoffSlider, highCutoffSlider;
    juce::Label centerImageValue, frontSepValue, rearSepValue, wrapValue, shiftValue,
                depthValue, focusValue, lowCutoffValue, highCutoffValue;
    juce::Label centerImageMin, centerImageMax, frontSepMin, frontSepMax, rearSepMin, rearSepMax,
                wrapMin, wrapMax, shiftMin, shiftMax, depthMin, depthMax, focusMin, focusMax,
                lowCutoffMin, lowCutoffMax, highCutoffMin, highCutoffMax; // last two stay empty/unused text -- Hz is already self-explanatory
    juce::ToggleButton bassRedirToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RetroSurroundEditor)
};
