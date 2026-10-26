[CmdletBinding()]
param(
    [string]$OutputDirectory = $PSScriptRoot,
    [string]$ProjectRoot = (Split-Path $PSScriptRoot -Parent),
    [switch]$PageArtOnly
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$rendererSource = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;

namespace ItchCoverBuild
{
    public static class CoverRenderer
    {
        private static Color Ink(string hex, int opacity = 255)
        {
            Color color = ColorTranslator.FromHtml(hex);
            return Color.FromArgb(opacity, color);
        }

        private static void Fill(Graphics graphics, string color, int left, int top,
            int width, int height, int opacity = 255)
        {
            using (var brush = new SolidBrush(Ink(color, opacity)))
                graphics.FillRectangle(brush, left, top, width, height);
        }

        private static void Line(Graphics graphics, string color, int startX, int startY,
            int endX, int endY, int width = 1, int opacity = 255)
        {
            using (var pen = new Pen(Ink(color, opacity), width))
                graphics.DrawLine(pen, startX, startY, endX, endY);
        }

        private static void Polygon(Graphics graphics, string color, int opacity,
            params Point[] points)
        {
            using (var brush = new SolidBrush(Ink(color, opacity)))
                graphics.FillPolygon(brush, points);
        }

        private static void Configure(Graphics graphics)
        {
            graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
            graphics.PixelOffsetMode = PixelOffsetMode.Half;
            graphics.SmoothingMode = SmoothingMode.None;
            graphics.CompositingMode = CompositingMode.SourceOver;
            graphics.TextRenderingHint = TextRenderingHint.SingleBitPerPixelGridFit;
        }

        private static Bitmap Frame(Bitmap sheet, int row, int column)
        {
            using (var frame = sheet.Clone(new Rectangle(column * 64, row * 64, 64, 64),
                PixelFormat.Format32bppArgb))
            {
                int left = 64, top = 64, right = -1, bottom = -1;
                for (int pixelY = 0; pixelY < 64; pixelY++)
                for (int pixelX = 0; pixelX < 64; pixelX++)
                {
                    if (frame.GetPixel(pixelX, pixelY).A == 0)
                        continue;
                    left = Math.Min(left, pixelX);
                    top = Math.Min(top, pixelY);
                    right = Math.Max(right, pixelX);
                    bottom = Math.Max(bottom, pixelY);
                }
                if (right < left)
                    throw new InvalidDataException("The selected sprite frame is empty.");
                return frame.Clone(new Rectangle(left, top, right - left + 1,
                    bottom - top + 1), PixelFormat.Format32bppArgb);
            }
        }

        private static void Sprite(Graphics graphics, Bitmap frame, int centerX, int feetY,
            int scale, string tint = null, int opacity = 255)
        {
            var destination = new Rectangle(centerX - frame.Width * scale / 2,
                feetY - frame.Height * scale, frame.Width * scale, frame.Height * scale);
            using (var attributes = new ImageAttributes())
            {
                var matrix = new ColorMatrix();
                matrix.Matrix33 = opacity / 255f;
                if (tint != null)
                {
                    Color color = Ink(tint);
                    matrix.Matrix00 = matrix.Matrix11 = matrix.Matrix22 = 0;
                    matrix.Matrix40 = color.R / 255f;
                    matrix.Matrix41 = color.G / 255f;
                    matrix.Matrix42 = color.B / 255f;
                }
                attributes.SetColorMatrix(matrix);
                attributes.SetWrapMode(WrapMode.TileFlipXY);
                graphics.DrawImage(frame, destination, 0, 0, frame.Width, frame.Height,
                    GraphicsUnit.Pixel, attributes);
            }
        }

        private static void Stamp(Graphics graphics, Bitmap atlas, Rectangle source,
            int left, int top, int scale)
        {
            graphics.DrawImage(atlas, new Rectangle(left, top, source.Width * scale,
                source.Height * scale), source, GraphicsUnit.Pixel);
        }

        private static void Environment(Graphics graphics, Bitmap atlas)
        {
            graphics.Clear(Ink("#101c1b"));
            Fill(graphics, "#172a29", 0, 130, 630, 192);
            Fill(graphics, "#1e3633", 196, 145, 238, 190);
            Fill(graphics, "#2b4842", 222, 151, 186, 183);
            Fill(graphics, "#5b8270", 234, 159, 162, 174);
            Fill(graphics, "#132924", 246, 170, 138, 164);
            Fill(graphics, "#a9d9ac", 246, 170, 138, 6);
            Fill(graphics, "#527c68", 246, 176, 6, 151);
            Fill(graphics, "#527c68", 378, 176, 6, 151);
            Fill(graphics, "#253f34", 255, 179, 120, 150);
            Fill(graphics, "#355444", 263, 187, 104, 140);
            Fill(graphics, "#6a8970", 311, 187, 7, 140);
            Fill(graphics, "#172a29", 311, 238, 7, 55);

            for (int panelX = 16; panelX < 630; panelX += 100)
            {
                Fill(graphics, "#263d38", panelX, 146, 66, 122);
                Fill(graphics, "#1b302c", panelX + 4, 151, 58, 114);
                Fill(graphics, "#3c5b4b", panelX + 10, 158, 15, 3);
                Fill(graphics, "#0c1918", panelX, 275, 66, 8);
            }

            Stamp(graphics, atlas, new Rectangle(192, 168, 96, 48), -25, 222, 2);
            Stamp(graphics, atlas, new Rectangle(512, 96, 64, 64), 513, 193, 2);
            Fill(graphics, "#101c1b", 0, 130, 630, 194, 100);
            Fill(graphics, "#79c9b9", 24, 184, 95, 4);
            Fill(graphics, "#ed8078", 516, 184, 90, 4);
            Fill(graphics, "#283e38", 0, 315, 630, 185);

            Polygon(graphics, "#3f5a45", 255, new Point(264, 321),
                new Point(367, 321), new Point(468, 500), new Point(153, 500));
            Polygon(graphics, "#698162", 45, new Point(274, 321),
                new Point(357, 321), new Point(427, 500), new Point(194, 500));

            int[] rows = { 326, 346, 373, 410, 458, 499 };
            foreach (int floorY in rows)
            {
                Line(graphics, "#101e1b", 0, floorY, 630, floorY, 2);
                Line(graphics, "#415747", 0, floorY + 3, 630, floorY + 3, 1, 130);
            }
            for (int floorX = -380; floorX < 1100; floorX += 155)
                Line(graphics, "#172b25", 315 + (floorX - 315) / 6,
                    319, floorX, 500, 2);

            Polygon(graphics, "#6fe2cf", 26, new Point(164, 247),
                new Point(386, 408), new Point(254, 437));
            Polygon(graphics, "#ff5967", 28, new Point(482, 227),
                new Point(238, 402), new Point(361, 438));
            Line(graphics, "#6fe2cf", 164, 247, 254, 437, 1, 65);
            Line(graphics, "#ff5967", 482, 227, 361, 438, 1, 65);

            Fill(graphics, "#101c1b", 0, 0, 630, 129);
            Fill(graphics, "#1c302b", 0, 129, 630, 3);
            Fill(graphics, "#81bca0", 0, 0, 185, 4);
            Fill(graphics, "#eaf18a", 185, 0, 86, 4);
            Fill(graphics, "#e7787d", 444, 0, 186, 4);
            Fill(graphics, "#6fe2cf", 18, 110, 27, 2, 100);
            Fill(graphics, "#ff5967", 585, 110, 27, 2, 100);
        }

        private static void Characters(Graphics graphics, Bitmap scout, Bitmap trooper,
            Bitmap player)
        {
            Fill(graphics, "#0a1512", 73, 414, 155, 12, 150);
            Fill(graphics, "#0a1512", 424, 418, 126, 12, 150);
            Fill(graphics, "#0a1512", 263, 430, 106, 10, 210);

            Sprite(graphics, scout, 134, 418, 6, "#e46fa2", 45);
            Sprite(graphics, scout, 150, 422, 6, "#83e5cc", 140);
            Sprite(graphics, scout, 153, 422, 6);
            Sprite(graphics, trooper, 503, 418, 6, "#ff5967", 55);
            Sprite(graphics, trooper, 488, 424, 6, "#ff777c", 150);
            Sprite(graphics, trooper, 485, 424, 6);

            GraphicsState state = graphics.Save();
            graphics.SetClip(new Rectangle(60, 252, 185, 5));
            Sprite(graphics, scout, 145, 422, 6, "#94ffe0", 185);
            graphics.Restore(state);
            state = graphics.Save();
            graphics.SetClip(new Rectangle(420, 288, 145, 4));
            Sprite(graphics, trooper, 495, 424, 6, "#ff5967", 180);
            graphics.Restore(state);

            Sprite(graphics, player, 315, 440, 5, "#081512");
            Sprite(graphics, player, 315, 436, 5, "#ecf1b2", 170);
            Sprite(graphics, player, 315, 433, 5);

            Fill(graphics, "#78dec6", 56, 270, 18, 3, 170);
            Fill(graphics, "#ff7b8f", 224, 204, 9, 3, 170);
            Fill(graphics, "#ff7b8f", 539, 288, 21, 3, 180);
            Fill(graphics, "#78dec6", 400, 342, 9, 3, 130);
            Fill(graphics, "#cce5ad", 260, 392, 5, 5, 100);
        }

        private static void Lettering(Graphics graphics, FontFamily family)
        {
            using (var titleFont = new Font(family, 51, FontStyle.Regular, GraphicsUnit.Pixel))
            using (var taglineFont = new Font(family, 15, FontStyle.Regular, GraphicsUnit.Pixel))
            using (var format = (StringFormat)StringFormat.GenericTypographic.Clone())
            {
                const string title = "h-AI-lucinations";
                const string tagline = "TRUST IS A GLITCH.";
                float titleWidth = graphics.MeasureString(title, titleFont, 2000, format).Width;
                if (titleWidth > 574)
                    throw new InvalidDataException("The title exceeds the cover's safe area.");
                float titleLeft = (630 - titleWidth) / 2;

                using (var shadow = new SolidBrush(Ink("#080f10")))
                using (var ivory = new SolidBrush(Ink("#eff2dc")))
                using (var lime = new SolidBrush(Ink("#dff47b")))
                {
                    graphics.DrawString(title, titleFont, shadow, titleLeft + 3, 43, format);
                    graphics.DrawString(title, titleFont, ivory, titleLeft, 40, format);
                    float prefixWidth = graphics.MeasureString("h-", titleFont, 2000, format).Width;
                    graphics.DrawString("AI", titleFont, lime, titleLeft + prefixWidth, 40, format);
                }

                Fill(graphics, "#6fe2cf", (int)titleLeft + 3, 104, 47, 2, 180);
                Fill(graphics, "#ff7b8f", (int)(titleLeft + titleWidth) - 49, 104, 47, 2, 180);
                Fill(graphics, "#101c1b", 0, 454, 630, 46, 235);
                Line(graphics, "#5b7960", 28, 454, 602, 454, 1, 90);
                float taglineWidth = graphics.MeasureString(tagline, taglineFont, 1000, format).Width;
                float taglineLeft = (630 - taglineWidth) / 2;
                using (var brush = new SolidBrush(Ink("#e0e8c5")))
                    graphics.DrawString(tagline, taglineFont, brush, taglineLeft, 469, format);
                Fill(graphics, "#75d6ba", (int)taglineLeft - 23, 478, 10, 3);
                Fill(graphics, "#ee7d82", (int)(taglineLeft + taglineWidth) + 13, 478, 10, 3);
            }
        }

        private static void SaveScaled(Bitmap source, string path, int scale)
        {
            using (var result = new Bitmap(source.Width * scale, source.Height * scale,
                PixelFormat.Format24bppRgb))
            using (var graphics = Graphics.FromImage(result))
            {
                Configure(graphics);
                graphics.DrawImage(source, new Rectangle(0, 0, result.Width, result.Height),
                    new Rectangle(0, 0, source.Width, source.Height), GraphicsUnit.Pixel);
                result.Save(path, ImageFormat.Png);
            }
        }

        private static void Banner(Graphics graphics, Bitmap atlas, Bitmap scout,
            Bitmap trooper, Bitmap player, FontFamily family)
        {
            graphics.Clear(Ink("#101c1b"));
            Fill(graphics, "#192e2a", 0, 96, 960, 154);
            for (int panelX = 16; panelX < 960; panelX += 104)
            {
                Fill(graphics, "#263d38", panelX, 113, 76, 110);
                Fill(graphics, "#1b302c", panelX + 4, 118, 68, 100);
                Fill(graphics, "#3c5b4b", panelX + 9, 125, 17, 3);
            }
            Stamp(graphics, atlas, new Rectangle(192, 168, 96, 48), -31, 166, 2);
            Stamp(graphics, atlas, new Rectangle(512, 96, 64, 64), 858, 114, 2);
            Fill(graphics, "#101c1b", 0, 96, 960, 154, 105);
            Fill(graphics, "#2b4842", 399, 151, 162, 112);
            Fill(graphics, "#6b9675", 410, 159, 140, 104);
            Fill(graphics, "#c7e6ac", 420, 165, 120, 5);
            Fill(graphics, "#355444", 420, 170, 120, 93);
            Fill(graphics, "#1a3028", 427, 178, 106, 85);
            Fill(graphics, "#6a8970", 477, 178, 6, 85);
            Fill(graphics, "#283e38", 0, 250, 960, 70);
            Polygon(graphics, "#5b7450", 160, new Point(420, 251),
                new Point(540, 251), new Point(600, 320), new Point(360, 320));
            foreach (int floorY in new[] { 255, 273, 299, 319 })
                Line(graphics, "#15271f", 0, floorY, 960, floorY, 2);
            for (int floorX = -120; floorX <= 1080; floorX += 120)
                Line(graphics, "#172b25", 480 + (floorX - 480) / 4,
                    250, floorX, 320, 2);

            Polygon(graphics, "#6fe2cf", 22, new Point(111, 188),
                new Point(425, 285), new Point(346, 320));
            Polygon(graphics, "#ff5967", 24, new Point(849, 174),
                new Point(534, 285), new Point(615, 320));
            Fill(graphics, "#0a1512", 54, 281, 123, 10, 190);
            Fill(graphics, "#0a1512", 798, 282, 98, 10, 190);
            Fill(graphics, "#0a1512", 445, 294, 70, 8, 210);
            Sprite(graphics, scout, 104, 284, 4, "#e46fa2", 50);
            Sprite(graphics, scout, 110, 288, 4, "#83e5cc", 150);
            Sprite(graphics, scout, 113, 288, 4);
            Sprite(graphics, trooper, 857, 286, 4, "#ff5967", 60);
            Sprite(graphics, trooper, 849, 290, 4, "#ff777c", 150);
            Sprite(graphics, trooper, 846, 290, 4);
            Sprite(graphics, player, 480, 300, 3, "#081512");
            Sprite(graphics, player, 480, 297, 3, "#ecf1b2", 170);
            Sprite(graphics, player, 480, 295, 3);

            GraphicsState state = graphics.Save();
            graphics.SetClip(new Rectangle(42, 194, 138, 3));
            Sprite(graphics, scout, 107, 288, 4, "#94ffe0", 185);
            graphics.Restore(state);
            state = graphics.Save();
            graphics.SetClip(new Rectangle(795, 213, 113, 3));
            Sprite(graphics, trooper, 853, 290, 4, "#ff5967", 180);
            graphics.Restore(state);

            using (var titleFont = new Font(family, 48, FontStyle.Regular, GraphicsUnit.Pixel))
            using (var taglineFont = new Font(family, 15, FontStyle.Regular, GraphicsUnit.Pixel))
            using (var format = (StringFormat)StringFormat.GenericTypographic.Clone())
            {
                string[] parts = { "h-", "AI", "-lucinations" };
                float titleWidth = 0;
                foreach (string part in parts)
                    titleWidth += graphics.MeasureString(part, titleFont, 2000, format).Width;
                if (titleWidth > 552)
                    throw new InvalidDataException("The banner title exceeds its safe area.");
                float titleLeft = (960 - titleWidth) / 2;
                foreach (string part in parts)
                {
                    using (var shadow = new SolidBrush(Ink("#080f10")))
                    using (var brush = new SolidBrush(Ink(part == "AI" ? "#dff47b" : "#eff2dc")))
                    {
                        graphics.DrawString(part, titleFont, shadow, titleLeft + 3, 53, format);
                        graphics.DrawString(part, titleFont, brush, titleLeft, 50, format);
                    }
                    titleLeft += graphics.MeasureString(part, titleFont, 2000, format).Width;
                }
                const string tagline = "TRUST IS A GLITCH.";
                float taglineWidth = graphics.MeasureString(tagline, taglineFont, 1000, format).Width;
                using (var brush = new SolidBrush(Ink("#d3dfbd")))
                    graphics.DrawString(tagline, taglineFont, brush, (960 - taglineWidth) / 2,
                        119, format);
                Fill(graphics, "#75d6ba", (int)((960 - taglineWidth) / 2) - 22, 128, 10, 3);
                Fill(graphics, "#ee7d82", (int)((960 + taglineWidth) / 2) + 12, 128, 10, 3);
            }
            Fill(graphics, "#81bca0", 0, 0, 278, 3);
            Fill(graphics, "#eaf18a", 432, 0, 96, 3);
            Fill(graphics, "#e7787d", 682, 0, 278, 3);
            Fill(graphics, "#79c9b9", 22, 100, 69, 3, 170);
            Fill(graphics, "#ed8078", 869, 100, 69, 3, 170);
            Fill(graphics, "#101c1b", 0, 312, 960, 8);
            Fill(graphics, "#46664e", 0, 312, 960, 1, 150);
        }

        private static void PageBackground(Graphics graphics, Bitmap atlas,
            Bitmap scout, Bitmap trooper)
        {
            graphics.Clear(Ink("#101c1b"));
            for (int side = 0; side < 2; side++)
            {
                int left = side == 0 ? 0 : 1030;
                string accent = side == 0 ? "#79c9b9" : "#e7787d";
                Fill(graphics, "#192d29", left, 0, 250, 720);
                Fill(graphics, "#293d35", left + 13, 0, 8, 720);
                Fill(graphics, "#293d35", left + 221, 0, 6, 720);
                for (int panelY = -64; panelY < 720; panelY += 160)
                {
                    Fill(graphics, "#233b32", left + 32, panelY, 170, 133);
                    Fill(graphics, "#162a25", left + 39, panelY + 7, 156, 119);
                    Fill(graphics, "#365346", left + 49, panelY + 17, 28, 3);
                    Fill(graphics, accent, left + 54, panelY + 26, 4, 4, 105);
                    Fill(graphics, "#0b1714", left + 28, panelY + 139, 179, 9);
                    for (int ventY = 0; ventY < 4; ventY++)
                        Fill(graphics, "#294237", left + 154, panelY + 63 + ventY * 8,
                            24, 2);
                }
                Stamp(graphics, atlas, new Rectangle(512, 96, 64, 64), left + 52, 66, 2);
                Stamp(graphics, atlas, new Rectangle(192, 168, 96, 48), left + 23, 578, 2);
                Line(graphics, "#45634f", left + 8, 24, left + 8, 686, 2, 110);
                Line(graphics, "#45634f", left + 233, 14, left + 233, 706, 2, 110);
                Fill(graphics, accent, left + 5, 85, 6, 74, 120);
                Fill(graphics, accent, left + 230, 384, 6, 103, 95);
                Fill(graphics, "#dff47b", left + 27, 301, 18, 3, 105);
                Fill(graphics, "#101c1b", left, 0, 250, 720, 88);
                for (int stripe = 0; stripe < 14; stripe++)
                {
                    int stripX = side == 0 ? 230 + stripe * 6 : 1044 - stripe * 6;
                    Fill(graphics, "#101c1b", stripX, 0, 6, 720,
                        Math.Min(255, 70 + stripe * 15));
                }
            }

            Sprite(graphics, scout, 140, 565, 5, "#79c9b9", 31);
            Sprite(graphics, trooper, 1141, 565, 5, "#e7787d", 33);
            Line(graphics, "#79c9b9", 69, 392, 196, 428, 1, 22);
            Line(graphics, "#e7787d", 1209, 382, 1080, 428, 1, 24);
            Fill(graphics, "#79c9b9", 50, 460, 22, 2, 65);
            Fill(graphics, "#e7787d", 1202, 419, 25, 2, 65);

            for (int floorY = 646; floorY < 720; floorY += 24)
            {
                Line(graphics, "#1c3027", 0, floorY, 274, floorY, 1, 120);
                Line(graphics, "#1c3027", 1006, floorY, 1280, floorY, 1, 120);
            }
            Fill(graphics, "#101c1b", 310, 0, 660, 720);
        }

        public static void RenderPageArt(string projectRoot, string outputDirectory)
        {
            using (var fonts = new PrivateFontCollection())
            using (var atlas = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/pixel-cyberpunk-interior.png")))
            using (var scoutSheet = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/Robots/ScoutBot/sheets/scout_idle.png")))
            using (var trooperSheet = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/Robots/TrooperBot/sheets/trooper_idle.png")))
            using (var playerSheet = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/Character 1.png")))
            using (var scout = Frame(scoutSheet, 7, 0))
            using (var trooper = Frame(trooperSheet, 1, 0))
            using (var player = Frame(playerSheet, 2, 2))
            using (var banner = new Bitmap(960, 320, PixelFormat.Format24bppRgb))
            using (var background = new Bitmap(1280, 720, PixelFormat.Format24bppRgb))
            {
                fonts.AddFontFile(Path.Combine(projectRoot, "assets/Fonts/NotJamToolkit15.ttf"));
                using (var graphics = Graphics.FromImage(banner))
                {
                    Configure(graphics);
                    Banner(graphics, atlas, scout, trooper, player, fonts.Families[0]);
                }
                using (var graphics = Graphics.FromImage(background))
                {
                    Configure(graphics);
                    PageBackground(graphics, atlas, scout, trooper);
                }
                banner.Save(Path.Combine(outputDirectory, "itch-banner.png"), ImageFormat.Png);
                SaveScaled(banner, Path.Combine(outputDirectory, "itch-banner-2x.png"), 2);
                SaveScaled(background, Path.Combine(outputDirectory, "itch-background.png"), 2);
            }
        }

        public static void Render(string projectRoot, string outputDirectory)
        {
            using (var fonts = new PrivateFontCollection())
            using (var atlas = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/pixel-cyberpunk-interior.png")))
            using (var scoutSheet = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/Robots/ScoutBot/sheets/scout_idle.png")))
            using (var trooperSheet = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/Robots/TrooperBot/sheets/trooper_idle.png")))
            using (var playerSheet = new Bitmap(Path.Combine(projectRoot,
                "assets/Sprites/Character 1.png")))
            using (var scout = Frame(scoutSheet, 7, 0))
            using (var trooper = Frame(trooperSheet, 1, 0))
            using (var player = Frame(playerSheet, 2, 2))
            using (var cover = new Bitmap(630, 500, PixelFormat.Format24bppRgb))
            {
                fonts.AddFontFile(Path.Combine(projectRoot, "assets/Fonts/NotJamToolkit15.ttf"));
                using (var graphics = Graphics.FromImage(cover))
                {
                    Configure(graphics);
                    Environment(graphics, atlas);
                    Characters(graphics, scout, trooper, player);
                    Lettering(graphics, fonts.Families[0]);
                }
                cover.Save(Path.Combine(outputDirectory, "itch-cover.png"), ImageFormat.Png);
                using (var large = new Bitmap(1260, 1000, PixelFormat.Format24bppRgb))
                using (var graphics = Graphics.FromImage(large))
                {
                    Configure(graphics);
                    graphics.DrawImage(cover, new Rectangle(0, 0, 1260, 1000),
                        new Rectangle(0, 0, 630, 500), GraphicsUnit.Pixel);
                    large.Save(Path.Combine(outputDirectory, "itch-cover-2x.png"), ImageFormat.Png);
                }
            }
        }
    }
}
'@

$buildNamespace = 'ItchCover' + [Guid]::NewGuid().ToString('N')
$renderer = Add-Type -TypeDefinition $rendererSource.Replace('ItchCoverBuild', $buildNamespace) `
    -ReferencedAssemblies System.Drawing -PassThru |
    Where-Object { $_.Name -eq 'CoverRenderer' }
[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
if ($PageArtOnly) {
    $renderer::RenderPageArt($ProjectRoot, $outputPath)
    $exportNames = @('itch-banner.png', 'itch-banner-2x.png', 'itch-background.png')
} else {
    $renderer::Render($ProjectRoot, $outputPath)
    $exportNames = @('itch-cover.png', 'itch-cover-2x.png')
}
$exportNames | ForEach-Object { Get-Item (Join-Path $outputPath $_) } |
    Select-Object Name, Length, FullName