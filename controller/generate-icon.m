#import <AppKit/AppKit.h>

static NSData *renderIcon(NSInteger pixels)
{
    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
        pixelsWide:pixels
        pixelsHigh:pixels
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bytesPerRow:0
        bitsPerPixel:0];
    if (bitmap == nil) {
        return nil;
    }

    NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:context];
    CGContextRef cg = context.CGContext;
    CGContextScaleCTM(cg, (CGFloat)pixels / 512.0, (CGFloat)pixels / 512.0);

    [[NSColor clearColor] setFill];
    NSRectFill(NSMakeRect(0, 0, 512, 512));

    NSBezierPath *background = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(28, 28, 456, 456)
        xRadius:108 yRadius:108];
    [[NSColor colorWithCalibratedRed:0.24 green:0.30 blue:0.72 alpha:1.0] setFill];
    [background fill];

    NSBezierPath *screen = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(92, 120, 328, 232)
        xRadius:28 yRadius:28];
    screen.lineWidth = 28;
    [[NSColor whiteColor] setStroke];
    [screen stroke];

    NSBezierPath *stand = [NSBezierPath bezierPath];
    stand.lineWidth = 28;
    stand.lineCapStyle = NSLineCapStyleRound;
    [stand moveToPoint:NSMakePoint(256, 120)];
    [stand lineToPoint:NSMakePoint(256, 82)];
    [stand moveToPoint:NSMakePoint(198, 78)];
    [stand lineToPoint:NSMakePoint(314, 78)];
    [stand stroke];

    NSBezierPath *shield = [NSBezierPath bezierPath];
    [shield moveToPoint:NSMakePoint(342, 288)];
    [shield lineToPoint:NSMakePoint(412, 262)];
    [shield lineToPoint:NSMakePoint(412, 202)];
    [shield curveToPoint:NSMakePoint(342, 144)
        controlPoint1:NSMakePoint(412, 170)
        controlPoint2:NSMakePoint(380, 152)];
    [shield curveToPoint:NSMakePoint(272, 202)
        controlPoint1:NSMakePoint(304, 152)
        controlPoint2:NSMakePoint(272, 170)];
    [shield lineToPoint:NSMakePoint(272, 262)];
    [shield closePath];
    [[NSColor colorWithCalibratedRed:0.20 green:0.78 blue:0.50 alpha:1.0] setFill];
    [shield fill];
    shield.lineWidth = 12;
    [[NSColor whiteColor] setStroke];
    [shield stroke];

    NSBezierPath *check = [NSBezierPath bezierPath];
    check.lineWidth = 16;
    check.lineCapStyle = NSLineCapStyleRound;
    check.lineJoinStyle = NSLineJoinStyleRound;
    [check moveToPoint:NSMakePoint(307, 219)];
    [check lineToPoint:NSMakePoint(332, 191)];
    [check lineToPoint:NSMakePoint(379, 242)];
    [check stroke];

    [NSGraphicsContext restoreGraphicsState];
    return [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
}

static void appendBigEndianUInt32(NSMutableData *data, uint32_t value)
{
    const uint8_t bytes[] = {
        (uint8_t)((value >> 24U) & 0xffU),
        (uint8_t)((value >> 16U) & 0xffU),
        (uint8_t)((value >> 8U) & 0xffU),
        (uint8_t)(value & 0xffU),
    };
    [data appendBytes:bytes length:sizeof(bytes)];
}

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        if (argc != 2) {
            fprintf(stderr, "Usage: %s output.icns\n", argv[0]);
            return 64;
        }

        struct IconRecord {
            const char type[5];
            NSInteger pixels;
        };
        static const struct IconRecord records[] = {
            { "icp4", 16 },
            { "icp5", 32 },
            { "icp6", 64 },
            { "ic07", 128 },
            { "ic08", 256 },
            { "ic09", 512 },
            { "ic10", 1024 },
            { "ic11", 32 },
            { "ic12", 64 },
            { "ic13", 256 },
            { "ic14", 512 },
        };

        NSMutableDictionary<NSNumber *, NSData *> *rendered = [NSMutableDictionary dictionary];
        NSMutableData *body = [NSMutableData data];
        for (size_t index = 0; index < sizeof(records) / sizeof(records[0]); ++index) {
            NSNumber *key = @(records[index].pixels);
            NSData *png = rendered[key];
            if (png == nil) {
                png = renderIcon(records[index].pixels);
                if (png != nil) {
                    rendered[key] = png;
                }
            }
            if (png == nil || png.length > UINT32_MAX - 8U) {
                fprintf(stderr, "Could not render icon size %ld\n", (long)records[index].pixels);
                return 65;
            }
            [body appendBytes:records[index].type length:4U];
            appendBigEndianUInt32(body, (uint32_t)png.length + 8U);
            [body appendData:png];
        }

        if (body.length > UINT32_MAX - 8U) {
            fprintf(stderr, "Generated icon is too large\n");
            return 65;
        }

        NSMutableData *icns = [NSMutableData data];
        [icns appendBytes:"icns" length:4U];
        appendBigEndianUInt32(icns, (uint32_t)body.length + 8U);
        [icns appendData:body];

        NSString *output = [NSString stringWithUTF8String:argv[1]];
        if (output.length == 0 || ![icns writeToFile:output atomically:YES]) {
            fprintf(stderr, "Could not write ICNS output\n");
            return 65;
        }
    }
    return 0;
}
