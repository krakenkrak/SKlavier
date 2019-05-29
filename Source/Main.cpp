// V 2.1.1

#include "../JuceLibraryCode/JuceHeader.h"
#include "MainComponent.h"

class Application    : public JUCEApplication
{
public:
    //==============================================================================
    Application() {}

    const String getApplicationName() override       { return "SKlavier"; }
    const String getApplicationVersion() override    { return "2.1.1"; }

    void initialise (const String&) override         { mainWindow.reset (new MainWindow ("SKlavier", new MainContentComponent(), *this)); }
    void shutdown() override                         { mainWindow = nullptr; }

private:
    class MainWindow    : public DocumentWindow
    {
    public:
        MainWindow (const String& name, Component* c, JUCEApplication& a)
            : DocumentWindow (name, Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (ResizableWindow::backgroundColourId),
                              DocumentWindow::closeButton),
              app (a)
        {
            setUsingNativeTitleBar (false);
            setContentOwned (c, true);

           #if JUCE_ANDROID || JUCE_IOS
            setFullScreen (true);
           #else
            setResizable (true, false);
            setResizeLimits (300, 250, 1229, 500);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }


    private:
        JUCEApplication& app;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (Application)
