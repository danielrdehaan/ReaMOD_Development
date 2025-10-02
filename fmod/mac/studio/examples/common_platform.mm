/*==============================================================================
FMOD Example Framework
Copyright (c), Firelight Technologies Pty, Ltd 2012-2025.
==============================================================================*/
#import "common.h"
#import <Cocoa/Cocoa.h>
#include <atomic>

const Common_Button BTN_IDS[] = {BTN_ACTION1, BTN_ACTION2, BTN_ACTION3, BTN_ACTION4, BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_MORE};
const unsigned int BTN_COUNT = sizeof(BTN_IDS) / sizeof(BTN_IDS[0]);

@interface ExampleApplicationDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
{
    NSWindow *mWindow;
    NSButton *mButtons[BTN_COUNT];
    NSTextField *mOutputWindow;
}
@end

NSMutableString *gOutputBuffer;
std::atomic<uint32_t> gButtons;
uint32_t gButtonsRead;
ExampleApplicationDelegate *gAppDelegate;

void Common_Init(void **extraDriverData)
{
    gOutputBuffer = [NSMutableString stringWithCapacity:(NUM_COLUMNS * NUM_ROWS)];
}

void Common_Close()
{

}

void Common_Update()
{
    [gAppDelegate performSelectorOnMainThread:@selector(update) withObject:nil waitUntilDone:YES];

    do
    {
        gButtonsRead = gButtons.load();
    } while (!gButtons.compare_exchange_weak(gButtonsRead, 0));
}

void Common_Exit(int returnCode)
{
    exit(-1);
}

void Common_DrawText(const char *text)
{   
    [gOutputBuffer appendFormat:@"%s\n", text];
}

bool Common_BtnPress(Common_Button btn)
{
    return ((gButtonsRead & (1 << btn)) != 0);
}

bool Common_BtnDown(Common_Button btn)
{
    return Common_BtnPress(btn);
}

const char *Common_BtnStr(Common_Button btn)
{
    switch (btn)
    {
        case BTN_ACTION1: return "1";
        case BTN_ACTION2: return "2";
        case BTN_ACTION3: return "3";
        case BTN_ACTION4: return "4";
        case BTN_UP:      return "Up";
        case BTN_DOWN:    return "Down";
        case BTN_LEFT:    return "Left";
        case BTN_RIGHT:   return "Right";
        case BTN_MORE:    return "More";
        case BTN_QUIT:    return "X";
    }
}

const char *Common_MediaPath(const char *fileName)
{
    return [[NSString stringWithFormat:@"%@/media/%s", [[NSBundle mainBundle] resourcePath], fileName] UTF8String];
}

const char *Common_WritePath(const char *fileName)
{
    return [[NSString stringWithFormat:@"%@/%s", NSTemporaryDirectory(), fileName] UTF8String];
}

@implementation ExampleApplicationDelegate : NSObject
- (id)init
{
    if (self = [super init])
    {
        const unsigned int BUTTON_WIDTH  = 50;
        const unsigned int BUTTON_HEIGHT = 20;
        const unsigned int WINDOW_WIDTH  = BUTTON_WIDTH * BTN_COUNT;
        const unsigned int WINDOW_HEIGHT = 500;
        
        mOutputWindow = [[NSTextField alloc] init];
        [mOutputWindow setFont:[NSFont userFixedPitchFontOfSize:12]];
        [mOutputWindow setEditable:NO];
        [mOutputWindow setBezeled:NO];
        [mOutputWindow setBackgroundColor:[NSColor blackColor]];
        [mOutputWindow setTextColor:[NSColor whiteColor]];

        for (unsigned int i = 0; i < BTN_COUNT; i++)
        {
            mButtons[i] = [[NSButton alloc] initWithFrame:NSMakeRect(i * BUTTON_WIDTH, WINDOW_HEIGHT - BUTTON_HEIGHT, BUTTON_WIDTH, BUTTON_HEIGHT)];
            [mButtons[i] setAction:@selector(buttonClick:)];
            [mButtons[i] setTag:BTN_IDS[i]];
            [mButtons[i] setTitle:[NSString stringWithUTF8String:Common_BtnStr(BTN_IDS[i])]];
            [mButtons[i] setContinuous:true];
            [mOutputWindow addSubview:mButtons[i]];
        }

        mWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable) backing:NSBackingStoreBuffered defer:NO];
        [mWindow setTitle:@"FMOD Example"];
        [mWindow setContentView:mOutputWindow];
        [mWindow setDelegate:self];
        
        [NSThread detachNewThreadSelector:@selector(threadMain:) toTarget:self withObject:nil];
    }
    
    return self;
}

- (void)dealloc
{
    for (unsigned int i = 0; i < BTN_COUNT; i++)
    {
        [mButtons[i] release];
    }

    [mOutputWindow release];
    [mWindow release];

    [super dealloc];
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    [mWindow center];
    [mWindow makeKeyAndOrderFront:self];
}

- (BOOL)windowShouldClose:(id)sender
{
    gButtons.fetch_or(1 << BTN_QUIT);
    return NO;
}

- (void)buttonClick:(id)sender
{
    gButtons.fetch_or(1 << [sender tag]);
}

- (void)update
{
    [mOutputWindow setStringValue:gOutputBuffer];
    [gOutputBuffer setString:@""];
}

int FMOD_Main();
- (void)threadMain:(id)arg
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    FMOD_Main();
    [pool drain];
    
    exit(0);
}
@end

int main(int argc, char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *application = [NSApplication sharedApplication];
    gAppDelegate = [[[ExampleApplicationDelegate alloc] init] autorelease];
    
    [application setDelegate:gAppDelegate];
    [application activateIgnoringOtherApps:YES];
    [application run];
    [pool drain];
    
    return EXIT_SUCCESS;
}
