#pragma once

#include "../PluginProcessor.h"
#include "../splitter/DrumSplitter.h"
#include "../splitter/LeftoversExporter.h"
#include "CrockPotLookAndFeel.h"

/*  SplitPage — "Cook" mode's front door (M4b, first workings).

    Drop a drum loop (or browse) → COOK IT → the M4a engine runs on a
    BACKGROUND juce::Thread (never the audio thread — invariant #4), with
    live progress + cancel → stems + Leftovers land in a folder next to the
    source file. Copy is honest per D12: rough creative split, toms/cymbals
    roughest.                                                                 */
class SplitPage final : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    SplitPage()
    {
        formats.registerBasicFormats();

        auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                                  float alpha, juce::Justification just)
        {
            l.setText (text, juce::dontSendNotification);
            l.setJustificationType (just);
            l.setColour (juce::Label::textColourId, crockpot::cream.withAlpha (alpha));
            addAndMakeVisible (l);
        };

        setupLabel (fileLabel,   "no loop loaded", 0.6f,  juce::Justification::centredLeft);
        setupLabel (outLabel,    "",               0.6f,  juce::Justification::centredLeft);
        setupLabel (statusLabel, "",               0.85f, juce::Justification::centred);
        setupLabel (honestLabel,
                    "v1 DSP split · rough + creative on purpose · kick and snare land best,"
                    " toms and cymbals are the roughest cut",
                    0.45f, juce::Justification::centred);

        loadButton.setButtonText ("Load drum loop...");
        loadButton.onClick = [this] { browseForLoop(); };
        addAndMakeVisible (loadButton);

        chooseOutButton.setButtonText ("Folder...");
        chooseOutButton.onClick = [this] { browseForOutputDir(); };
        addAndMakeVisible (chooseOutButton);

        cookButton.setButtonText ("COOK IT");
        cookButton.setColour (juce::TextButton::buttonColourId, crockpot::ember);
        cookButton.onClick = [this] { startCooking(); };
        addAndMakeVisible (cookButton);

        cancelButton.setButtonText ("Cancel");
        cancelButton.onClick = [this] { cancelCooking(); };
        addAndMakeVisible (cancelButton);

        revealButton.setButtonText ("Open Leftovers folder");
        revealButton.onClick = [this] { if (outputDir.isDirectory()) outputDir.revealToUser(); };
        addAndMakeVisible (revealButton);

        addAndMakeVisible (progressBar);

        setUiState (UiState::idle);
        startTimerHz (15);
    }

    ~SplitPage() override
    {
        cancelCooking();
    }

    //==========================================================================
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files)
            if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif")
             || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac")
             || f.endsWithIgnoreCase (".mp3"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        if (! files.isEmpty())
            loadLoop (juce::File (files[0]));
    }

    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { dragOver = false; repaint(); }

    //==========================================================================
    void resized() override
    {
        auto r = getLocalBounds().reduced (16);

        honestLabel.setBounds (r.removeFromBottom (18));
        r.removeFromBottom (4);

        dropZone = r.removeFromTop (juce::jmax (80, r.getHeight() / 3));
        r.removeFromTop (10);

        auto fileRow = r.removeFromTop (30);
        loadButton.setBounds (fileRow.removeFromLeft (150).reduced (0, 1));
        fileRow.removeFromLeft (10);
        fileLabel.setBounds (fileRow);

        auto outRow = r.removeFromTop (30);
        chooseOutButton.setBounds (outRow.removeFromLeft (150).reduced (0, 1));
        outRow.removeFromLeft (10);
        outLabel.setBounds (outRow);

        r.removeFromTop (8);
        auto actionRow = r.removeFromTop (40);
        cookButton.setBounds (actionRow.removeFromLeft (juce::jmax (140, actionRow.getWidth() / 3)));
        actionRow.removeFromLeft (10);
        cancelButton.setBounds (actionRow.removeFromLeft (100));
        actionRow.removeFromLeft (10);
        revealButton.setBounds (actionRow);

        r.removeFromTop (10);
        progressBar.setBounds (r.removeFromTop (22));
        statusLabel.setBounds (r.removeFromTop (24));
    }

    void paint (juce::Graphics& g) override
    {
        using namespace crockpot;

        // drop zone: dashed warm border, brightens on drag-over
        auto z = dropZone.toFloat().reduced (2.0f);
        g.setColour (bgTop.withAlpha (dragOver ? 0.75f : 0.45f));
        g.fillRoundedRectangle (z, 12.0f);

        juce::Path border;
        border.addRoundedRectangle (z, 12.0f);
        juce::Path dashed;
        const float dashes[] = { 8.0f, 6.0f };
        juce::PathStrokeType (dragOver ? 2.5f : 1.5f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour ((dragOver ? ember : lidShine).withAlpha (0.8f));
        g.fillPath (dashed);

        g.setColour (cream.withAlpha (dragOver ? 0.95f : 0.7f));
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.drawText (dragOver ? "drop it in the pot!"
                             : "drag a drum loop here (wav / aiff / flac / mp3)",
                    dropZone, juce::Justification::centred, false);
    }

private:
    enum class UiState { idle, loaded, cooking, done, failed };

    //==========================================================================
    class SplitThread final : public juce::Thread
    {
    public:
        SplitThread (juce::AudioBuffer<float>&& audioBuffer, double rate,
                     juce::File dir, juce::String base,
                     std::function<void (bool, juce::String)> onFinished)
            : juce::Thread ("CrockPot Splitter"),
              buffer (std::move (audioBuffer)), sampleRate (rate),
              outDir (std::move (dir)), baseName (std::move (base)),
              finished (std::move (onFinished)) {}

        DrumSplitter::Progress progress;

        void run() override
        {
            auto result = DrumSplitter::split (buffer, sampleRate, &progress);

            if (threadShouldExit() || progress.cancelled.load())
                return;

            const auto exported = LeftoversExporter::exportAll (result, outDir, baseName);

            const bool ok = exported.wasOk();
            const auto msg = ok ? juce::String (result.oneShots.size())
                                    + " leftovers + 5 stems served"
                                : exported.getErrorMessage();

            auto cb = finished;
            juce::MessageManager::callAsync ([cb, ok, msg] { if (cb) cb (ok, msg); });
        }

    private:
        juce::AudioBuffer<float> buffer;
        double sampleRate;
        juce::File outDir;
        juce::String baseName;
        std::function<void (bool, juce::String)> finished;
    };

    //==========================================================================
    void browseForLoop()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Choose a drum loop", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().existsAsFile())
                    loadLoop (fc.getResult());
            });
    }

    void browseForOutputDir()
    {
        chooser = std::make_unique<juce::FileChooser> ("Where should the Leftovers go?");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().isDirectory())
                {
                    outputDir = fc.getResult();
                    outLabel.setText (outputDir.getFullPathName(), juce::dontSendNotification);
                }
            });
    }

    void loadLoop (const juce::File& file)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
        {
            statusLabel.setText ("couldn't read that file", juce::dontSendNotification);
            return;
        }

        const auto maxLen = (juce::int64) (60.0 * reader->sampleRate);   // 60 s cap
        if (reader->lengthInSamples > maxLen)
        {
            statusLabel.setText ("that's a whole song, not a loop - 60 s max for v1",
                                 juce::dontSendNotification);
            return;
        }

        const int numCh  = juce::jlimit (1, 2, (int) reader->numChannels);
        const int length = (int) reader->lengthInSamples;

        loopBuffer.setSize (numCh, length);
        reader->read (&loopBuffer, 0, length, 0, true, numCh > 1);
        loopRate = reader->sampleRate;
        loopFile = file;

        outputDir = file.getParentDirectory()
                        .getChildFile (file.getFileNameWithoutExtension() + "_Leftovers");

        fileLabel.setText (file.getFileName() + "  ("
                           + juce::String (length / loopRate, 1) + " s)",
                           juce::dontSendNotification);
        outLabel.setText (outputDir.getFullPathName(), juce::dontSendNotification);

        setUiState (UiState::loaded);
    }

    void startCooking()
    {
        if (loopBuffer.getNumSamples() == 0 || thread != nullptr)
            return;

        juce::AudioBuffer<float> copy;
        copy.makeCopyOf (loopBuffer);

        juce::Component::SafePointer<SplitPage> safe (this);
        thread = std::make_unique<SplitThread> (
            std::move (copy), loopRate, outputDir,
            loopFile.getFileNameWithoutExtension(),
            [safe] (bool ok, juce::String msg)
            {
                if (auto* self = safe.getComponent())
                    self->onCookFinished (ok, msg);
            });

        thread->startThread();
        setUiState (UiState::cooking);
    }

    void cancelCooking()
    {
        if (thread != nullptr)
        {
            thread->progress.cancelled.store (true);
            thread->stopThread (5000);
            thread.reset();
            if (isShowing())
                setUiState (loopBuffer.getNumSamples() > 0 ? UiState::loaded : UiState::idle);
        }
    }

    void onCookFinished (bool ok, const juce::String& msg)
    {
        if (thread != nullptr)
        {
            thread->waitForThreadToExit (2000);
            thread.reset();
        }
        statusLabel.setText (msg, juce::dontSendNotification);
        setUiState (ok ? UiState::done : UiState::failed);
    }

    void setUiState (UiState s)
    {
        state = s;
        const bool cooking = (s == UiState::cooking);

        cookButton.setEnabled (s == UiState::loaded || s == UiState::done || s == UiState::failed);
        cancelButton.setVisible (cooking);
        progressBar.setVisible (cooking);
        revealButton.setVisible (s == UiState::done);
        loadButton.setEnabled (! cooking);
        chooseOutButton.setEnabled (! cooking);

        if (s == UiState::idle)   statusLabel.setText ("", juce::dontSendNotification);
        if (s == UiState::loaded) statusLabel.setText ("ready to cook", juce::dontSendNotification);
        if (cooking)              statusLabel.setText ("cooking - low and slow...", juce::dontSendNotification);
    }

    void timerCallback() override
    {
        if (thread != nullptr)
            progressValue = (double) thread->progress.ratio.load();
    }

    //==========================================================================
    juce::AudioFormatManager formats;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<SplitThread> thread;

    juce::AudioBuffer<float> loopBuffer;
    double loopRate = 44100.0;
    juce::File loopFile, outputDir;

    juce::Rectangle<int> dropZone;
    bool dragOver = false;
    UiState state = UiState::idle;

    juce::TextButton loadButton, chooseOutButton, cookButton, cancelButton, revealButton;
    juce::Label fileLabel, outLabel, statusLabel, honestLabel;
    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplitPage)
};
