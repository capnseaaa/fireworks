#!/bin/sh
# Package Starbloom for an SD card. Builds first, then writes release/:
#   gba/starbloom.gba, gba/downloaded_images/starbloom.png   the R36 card's gba/ folder (EmulationStation)
#   gamelist-entry.xml                                        one <game> block for gba/gamelist.xml
#   starbloom-art.zip                                         the art and description, in a folder per setup
# The card already has its own gba/gamelist.xml. Never replace it: insert the one block before </gameList>.
set -e
cd "$(dirname "$0")/.."
sh build.sh > /dev/null
rm -rf release
mkdir -p release/gba/downloaded_images
cp starbloom.gba release/gba/starbloom.gba
cp art/cover.png release/gba/downloaded_images/starbloom.png
# A tall cover (250 x 360) for setups that want one: the square cover on the night sky's color.
python3 - <<'PY'
from PIL import Image
sq = Image.open("art/cover.png").convert("RGB").resize((250, 250), Image.LANCZOS)
tall = Image.new("RGB", (250, 360), sq.getpixel((125, 2)))
tall.paste(sq, (0, 55))
tall.save("release/portrait.png")
PY
DESC="Grandma has handed you her little firework shop in the seaside town of Port Willow. By day, walk the forest, the mine, the tide pools and the quarry for powder, shells and colored stars. By night, customers tell you what they'd love to see in the sky, and you build each firework at the counter. At closing time it all goes up in one big show, and then everyone tells you what they thought."
cat > release/gamelist-entry.xml <<XML
	<game>
		<path>./starbloom.gba</path>
		<name>STARBLOOM</name>
		<desc>$DESC</desc>
		<image>./downloaded_images/starbloom.png</image>
		<developer>Caleb</developer>
		<genre>Simulation</genre>
		<players>1</players>
	</game>
XML
A=release/art/starbloom-art
mkdir -p "$A/images" "$A/R36 ArkOS EmuELEC (EmulationStation)/gba/downloaded_images" \
  "$A/RetroArch/thumbnails/Nintendo - Game Boy Advance/Named_Boxarts" \
  "$A/Onion OS (Miyoo Mini)/Roms/GBA/Imgs" "$A/NextUI/Roms/Game Boy Advance (GBA)/.media" \
  "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/box" "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/text"
cp art/cover.png "$A/images/starbloom_square_640.png"
cp release/portrait.png "$A/images/starbloom_portrait_250x360.png"
printf '%s\n' "$DESC" > "$A/description.txt"
cp art/cover.png "$A/R36 ArkOS EmuELEC (EmulationStation)/gba/downloaded_images/starbloom.png"
cp release/gamelist-entry.xml "$A/R36 ArkOS EmuELEC (EmulationStation)/gamelist-entry.xml"
cp art/cover.png "$A/RetroArch/thumbnails/Nintendo - Game Boy Advance/Named_Boxarts/starbloom.png"
cp release/portrait.png "$A/Onion OS (Miyoo Mini)/Roms/GBA/Imgs/starbloom.png"
cp art/cover.png "$A/NextUI/Roms/Game Boy Advance (GBA)/.media/starbloom.png"
cp release/portrait.png "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/box/starbloom.png"
printf '%s\n' "$DESC" > "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/text/starbloom.txt"
cp docs/INSTALL.md "$A/HOW_TO_INSTALL.md"
rm release/portrait.png
(cd release/art && zip -qr ../starbloom-art.zip starbloom-art)
rm -rf release/art
echo "packaged:"; find release -type f | sort
