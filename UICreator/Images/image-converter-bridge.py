"""UICreator adapter. LVGL byte packing/compression is provided by LVGLImage.py."""
import base64
import io
import json
import logging
import re
import struct
import sys
from pathlib import Path
from PIL import Image, ImageSequence
import resvg_py
import LVGLImage as lv

# Upstream declares RGB565A8 as 16 bpp (the color plane), but its PNG
# unpacker handles that format under 24 bpp. Account for the separate alpha
# plane here so previews and PNG export contain every pixel.
_unpack_colors = lv.unpack_colors
def unpack_colors(data, cf, width):
    if cf != lv.ColorFormat.RGB565A8:
        return _unpack_colors(data,cf,width)
    count=len(data)//3
    colors,alpha=data[:count*2],data[count*2:]
    result=[]
    for index,a in enumerate(alpha):
        pixel=colors[index*2] | (colors[index*2+1]<<8)
        result += [lv.bit_extend((pixel>>11)&31,5),lv.bit_extend((pixel>>5)&63,6),lv.bit_extend(pixel&31,5),a]
    return result
lv.unpack_colors=unpack_colors


def encoded(data):
    return base64.b64encode(data).decode("ascii")


def convert(request, job):
    options = request.get("options", {})
    if options.get("verbose",False):
        logging.basicConfig(level=logging.INFO)
    cf_name = options.get("colorFormat", "ARGB8888")
    if cf_name not in lv.ColorFormat.__members__ and cf_name != "AUTO":
        raise ValueError("Unknown LVGL color format")
    compression = lv.CompressMethod[options.get("compression", "NONE")]
    fmt = options.get("format", "C")
    if fmt not in ("C", "BIN", "PNG"):
        raise ValueError("Choose C, BIN, or PNG output")
    align = int(options.get("align", 1))
    if align < 1 or align > 4096:
        raise ValueError("Stride alignment must be between 1 and 4096 bytes")
    if options.get("bigEndian", False):
        if cf_name not in ("RGB565", "RGB565_SWAPPED"):
            raise ValueError("LVGL 9 big-endian pixel output uses RGB565_SWAPPED; select RGB565")
        cf_name = "RGB565_SWAPPED"
    if cf_name in ("RAW", "RAW_ALPHA") and (compression != lv.CompressMethod.NONE or align != 1 or options.get("premultiply")):
        raise ValueError("RAW images cannot use pixel compression, stride alignment, or premultiplication")
    if options.get("nemaGfx") and cf_name != "I8":
        raise ValueError("NEMA palette layout requires I8")
    if options.get("chromaKey") and cf_name in ("RGB565","RGB565_SWAPPED","RGB888","XRGB8888","L8"):
        raise ValueError("Chroma-key transparency requires an alpha or indexed color format")
    sources = []
    for source in request["sources"]:
        path = Path(source)
        if path.is_dir():
            sources += sorted(p for p in path.rglob("*") if p.suffix.lower() in (".png", ".jpg", ".jpeg", ".bmp", ".gif", ".svg"))
        else:
            sources.append(path)
    if not sources:
        raise ValueError("Select at least one image file or a folder containing images")
    override = options.get("name", "")
    keywords=set("auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while _Bool _Complex _Imaginary _Atomic _Generic _Noreturn _Static_assert _Thread_local".split())
    if override and len(sources) != 1:
        raise ValueError("A custom output name is only valid with one input file")
    if override and not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", override):
        raise ValueError("Image symbol must be a C identifier")
    if override in keywords or override.startswith("__"):
        raise ValueError("Image symbol cannot be a C keyword or reserved identifier")

    class PaletteQuantizer:
        def __init__(self, ncolors=256, dither=True, exec_path=""):
            self.colors = ncolors
        def convert(self, filename):
            image = Image.open(filename).convert("RGBA")
            unique=image.getcolors(self.colors+1)
            count=min(self.colors,max(2,len(unique))) if unique else self.colors
            palette = image.quantize(colors=count, method=Image.Quantize.FASTOCTREE,
                                     dither=Image.Dither.FLOYDSTEINBERG if options.get("dither", True) else Image.Dither.NONE)
            output = io.BytesIO()
            bits=1 if count<=2 else 2 if count<=4 else 4 if count<=16 else 8
            palette.save(output, format="PNG", bits=bits)
            return output.getvalue()
    lv.PngQuant = PaletteQuantizer  # Avoid an external pngquant shell command.
    assets, files, symbols = [], [], set()
    def stage(destination, data):
        name = "output-" + str(len(files))
        (job/name).write_bytes(data)
        files.append({"name": destination, "staged": name})

    for source_index, source in enumerate(sources):
        raw = source.read_bytes()
        name = override or re.sub(r"[^A-Za-z0-9_]", "_", source.stem)
        if not name or name[0].isdigit() or name in keywords or name.startswith("__"):
            name = "image_" + name
        stem = name
        suffix = 2
        while name.casefold() in symbols:
            name = stem + "_" + str(suffix)
            suffix += 1
        symbols.add(name.casefold())
        raster = resvg_py.svg_to_bytes(svg_string=raw.decode("utf-8"), skip_system_fonts=False) if source.suffix.lower() == ".svg" else raw
        image = Image.open(io.BytesIO(raster))
        asset = {"name": source.name, "sourceData": encoded(raw), "conversion": options,
                 "deviceFrames": [], "previewFrames": [], "delays": []}
        # Pillow frame iterators share the decoder, so seek/copy each frame explicitly.
        for frame_index in range(getattr(image, "n_frames", 1)):
            image.seek(frame_index)
            frame = image.convert("RGBA")
            if frame.width > 65535 or frame.height > 65535 or frame.width*frame.height > 16*1024*1024:
                raise ValueError("Image dimensions are too large")
            if not options.get("includeAlpha", True):
                color = int(options.get("background", 0))
                background = Image.new("RGBA", frame.size, ((color>>16)&255, (color>>8)&255, color&255, 255))
                background.alpha_composite(frame)
                frame = background
            if options.get("chromaKey", False):
                key = int(options.get("chromaColor", 0x00FF00))
                pixels = [(r,g,b,0 if ((r<<16)|(g<<8)|b)==key else a) for r,g,b,a in frame.getdata()]
                frame.putdata(pixels)
            png_path = job/f"input-{source_index}-{frame_index}.png"
            frame.save(png_path)
            device_path = job/f"device-{source_index}-{frame_index}.bin"
            preview_path = job/f"preview-{source_index}-{frame_index}.png"
            frame_name = name if getattr(image, "n_frames", 1) == 1 else f"{name}_frame_{frame_index}"
            if cf_name in ("RAW", "RAW_ALPHA"):
                # For animation store independent PNG frames; still images retain original bytes.
                payload = raw if getattr(image, "n_frames", 1) == 1 and source.suffix.lower() not in (".svg", ".bmp") else png_path.read_bytes()
                cf = lv.ColorFormat[cf_name]
                asset["deviceFrames"].append({"formatVersion":9,"cf":cf.value[0],"w":frame.width,"h":frame.height,"stride":0,"flags":0,"data":encoded(payload)})
                export_path=job/(frame_name+".c")
                lv.RAWImage(cf, payload).to_c_array(str(export_path),outputname=frame_name)
                stage(frame_name+".c",export_path.read_bytes())
                preview_path.write_bytes(png_path.read_bytes())
            else:
                cf = None if cf_name == "AUTO" else lv.ColorFormat[cf_name]
                converted = lv.LVGLImage().from_png(str(png_path),cf,background=int(options.get("background",0)),rgb565_dither=bool(options.get("dither",True)),nema_gfx=bool(options.get("nemaGfx",False)))
                # Preview the quantized result before alpha premultiplication or accelerator palette layout.
                visual_cf=lv.ColorFormat.ARGB8888 if cf==lv.ColorFormat.ARGB8888_PREMULTIPLIED else cf
                visual = lv.LVGLImage().from_png(str(png_path),visual_cf,background=int(options.get("background",0)),rgb565_dither=bool(options.get("dither",True)),nema_gfx=False)
                visual.to_png(str(preview_path))
                converted.adjust_stride(align=align)
                if options.get("premultiply",False) and cf_name != "ARGB8888_PREMULTIPLIED":
                    converted.premultiply()
                converted.to_bin(str(device_path),compress=compression)
                data = device_path.read_bytes()
                magic, color, flags, width, height, stride, reserved = struct.unpack("<BBHHHHH",data[:12])
                asset["deviceFrames"].append({"formatVersion":9,"cf":color,"w":width,"h":height,"stride":stride,"flags":flags,"data":encoded(data[12:])})
                if fmt == "C":
                    export_path=job/(frame_name+".c")
                    converted.to_c_array(str(export_path),compress=compression,outputname=frame_name)
                    stage(frame_name+".c",export_path.read_bytes())
                elif fmt == "BIN":
                    stage(frame_name+".bin",data)
                else:
                    stage(frame_name+".png",preview_path.read_bytes())
            asset["previewFrames"].append(encoded(preview_path.read_bytes()))
            asset["delays"].append(max(20,int(image.info.get("duration",100))))
        assets.append(asset)
    (job/"result.json").write_text(json.dumps({"assets":assets,"files":files}),encoding="utf-8")


if __name__ == "__main__":
    request_path=Path(sys.argv[-1])
    try:
        convert(json.loads(request_path.read_text(encoding="utf-8")),request_path.parent)
    except Exception as error:
        print(str(error),file=sys.stderr)
        sys.exit(1)
