// Writes a picture as HEIC carrying an EXIF/TIFF Orientation tag, then reads the tag back (macOS only; used by run.sh):
//   make_heic <in.png> <out.heic> <orientation 1-8>
import Foundation
import ImageIO

func die(_ message: String, _ code: Int32) -> Never {
  FileHandle.standardError.write(Data((message + "\n").utf8))
  exit(code)
}

let args = CommandLine.arguments
guard args.count == 4, let orientation = UInt32(args[3]), (1...8).contains(orientation) else {
  die("usage: make_heic <in.png> <out.heic> <orientation 1-8>", 2)
}
guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: args[1]) as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
  die("cannot read \(args[1])", 1)
}
let outURL = URL(fileURLWithPath: args[2]) as CFURL
guard let destination = CGImageDestinationCreateWithURL(outURL, "public.heic" as CFString, 1, nil) else {
  die("this system cannot write HEIC", 1)
}
let properties: [CFString: Any] = [kCGImagePropertyOrientation: orientation,
                                   kCGImageDestinationLossyCompressionQuality: 0.95]
CGImageDestinationAddImage(destination, image, properties as CFDictionary)
guard CGImageDestinationFinalize(destination) else {
  die("cannot write \(args[2])", 1)
}

guard let written = CGImageSourceCreateWithURL(outURL, nil),
      let info = CGImageSourceCopyPropertiesAtIndex(written, 0, nil) as? [CFString: Any] else {
  die("cannot read back \(args[2])", 1)
}
let width = (info[kCGImagePropertyPixelWidth] as? Int) ?? 0
let height = (info[kCGImagePropertyPixelHeight] as? Int) ?? 0
let tag = (info[kCGImagePropertyOrientation] as? Int) ?? 1
print("\(args[2]): \(width) x \(height) stored, orientation tag \(tag)")
if tag != Int(orientation) {
  die("the orientation tag was not kept (wanted \(orientation))", 3)
}
