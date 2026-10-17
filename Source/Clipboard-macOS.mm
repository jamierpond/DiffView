#include "Clipboard.h"

#import <AppKit/AppKit.h>

namespace diffview::clipboard
{
std::optional<std::string> imagePng()
{
    @autoreleasepool
    {
        auto* board = [NSPasteboard generalPasteboard];

        if (auto* png = [board dataForType:NSPasteboardTypePNG])
            return std::string {static_cast<const char*>(png.bytes), png.length};

        // Screenshots and most apps put TIFF up; convert it.
        auto* tiff = [board dataForType:NSPasteboardTypeTIFF];

        if (tiff == nil)
            return std::nullopt;

        auto* image = [NSBitmapImageRep imageRepWithData:tiff];
        auto* png = [image representationUsingType:NSBitmapImageFileTypePNG
                                        properties:@{}];

        if (png == nil)
            return std::nullopt;

        return std::string {static_cast<const char*>(png.bytes), png.length};
    }
}
} // namespace diffview::clipboard
