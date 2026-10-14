// make_icon.swift - draws the app icon: original pixel art (a jet climbing in
// front of a sunset), no artwork from the game.
//
//   swift tools/make_icon.swift <out.png> <size> [--full]
//
// Default: macOS icon layout (artwork inset in a rounded square, transparent
// margin). --full: artwork fills the square (Windows .ico).
import AppKit

let args = CommandLine.arguments
let outPath = args[1]
let size = Int(args[2])!
let full = args.contains("--full")

func rgb(_ hex: UInt32) -> NSColor {
    NSColor(srgbRed: CGFloat((hex >> 16) & 255) / 255, green: CGFloat((hex >> 8) & 255) / 255,
            blue: CGFloat(hex & 255) / 255, alpha: 1)
}

// 32 x 32 grid. Sky bands top to bottom, then the sun, clouds and the jet.
let bands: [(Int, UInt32)] = [
    (0, 0x1D2140), (9, 0x2E2660), (13, 0x58307E), (16, 0x8E3388), (19, 0xC83C78),
    (22, 0xEE5C52), (25, 0xF89A44), (28, 0x0E3B6E), (30, 0x0A2A52),
]
func sky(_ row: Int) -> UInt32 { bands.last { $0.0 <= row }!.1 }

var grid = [[UInt32?]](repeating: [UInt32?](repeating: nil, count: 32), count: 32)
for r in 0..<32 { for c in 0..<32 { grid[r][c] = sky(r) } }

// Sun: a disc sitting on the horizon (row 28), stepped like NES art.
let sunRows: [(Int, Int, Int)] = [(19, 13, 18), (20, 11, 20), (21, 10, 21), (22, 9, 22), (23, 9, 22),
                                  (24, 8, 23), (25, 8, 23), (26, 8, 23), (27, 8, 23)]
for (r, a, b) in sunRows { for c in a...b { grid[r][c] = r < 23 ? 0xFCE46C : 0xFCC44C } }
// Sun reflection on the sea.
for (r, a, b) in [(28, 10, 21), (29, 12, 19), (30, 13, 18), (31, 15, 16)] { for c in a...b { grid[r][c] = 0xF89A44 } }

// Clouds.
for (r, a, b) in [(5, 3, 7), (6, 2, 9), (7, 22, 26), (8, 21, 29), (12, 4, 6)] { for c in a...b { grid[r][c] = 0x6A5A9A } }

// The jet, top-down, nose up: left half, mirrored. O outline, L light,
// G body, C canopy, D shadow, R/F afterburner.
let jetLeft = [
    ".......O", "......OL", "......OL", ".....OLC", ".....OLC", ".....OGC", ".....OGG",
    "....OOGG", "...OLGGG", "..OLGGGG", ".OLGGGGG", "OLGGGGGG", "OOOOOGGG", ".....OGG",
    ".....OGG", "....OOGG", "...OLGGG", "...OOOOD", "......RF", ".......F",
]
let ink: [Character: UInt32] = ["O": 0x23233A, "L": 0xF4F6FC, "G": 0xB8BECE, "C": 0x3CBCFC,
                                "D": 0x6A7088, "R": 0xF8783C, "F": 0xFCE46C]
let top = 4, left = 8
for (i, row) in jetLeft.enumerated() {
    let chars = Array(row) + Array(row).reversed()
    for (j, ch) in chars.enumerated() where ch != "." { grid[top + i][left + j] = ink[ch]! }
}

// Render: whole pixels only, so the art stays crisp.
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size, bitsPerSample: 8,
                           samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                           bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
let ctx = NSGraphicsContext(bitmapImageRep: rep)!
NSGraphicsContext.current = ctx
ctx.imageInterpolation = .none
ctx.shouldAntialias = false

let art = full ? CGFloat(size) : CGFloat(size) * 832 / 1024   // Apple's grid: 824-ish content in 1024
let cell = art / 32
let origin = (CGFloat(size) - art) / 2
let radius = art * (full ? 0.18 : 0.225)
let frame = NSRect(x: origin, y: origin, width: art, height: art)
ctx.shouldAntialias = true
NSBezierPath(roundedRect: frame, xRadius: radius, yRadius: radius).addClip()
ctx.shouldAntialias = false
for r in 0..<32 {
    for c in 0..<32 {
        guard let color = grid[r][c] else { continue }
        rgb(color).setFill()
        // AppKit's y axis points up; row 0 is the top.
        let x = (origin + CGFloat(c) * cell).rounded(.down)
        let y = (origin + art - CGFloat(r + 1) * cell).rounded(.down)
        let x2 = (origin + CGFloat(c + 1) * cell).rounded(.down)
        let y2 = (origin + art - CGFloat(r) * cell).rounded(.down)
        NSRect(x: x, y: y, width: x2 - x, height: y2 - y).fill()
    }
}
NSGraphicsContext.restoreGraphicsState()
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: outPath))
