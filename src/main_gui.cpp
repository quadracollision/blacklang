#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioEngine.h"
#include "gui/MainComponent.h"

class BlackLangGUIApplication : public juce::JUCEApplication {
public:
    BlackLangGUIApplication() = default;

    const juce::String getApplicationName() override { return "BlackLang"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override {
        if (!audioEngine.initialize()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Audio Error",
                "Failed to initialize audio engine");
            quit();
            return;
        }
        
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), audioEngine);
    }

    void shutdown() override {
        mainWindow = nullptr;
        audioEngine.shutdown();
    }

    void systemRequestedQuit() override {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name, AudioEngine& engine)
            : DocumentWindow(name, 
                juce::Colour(0xff1a1a1a),
                DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(engine), true);

            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    AudioEngine audioEngine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(BlackLangGUIApplication)
