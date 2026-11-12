# Auto Deco (Geode mod)

Adds an **Auto Deco** button to the editor pause menu. It scans every solid block and
scatters decoration objects along exposed tops, sides and bottoms. One Undo reverts it.

## Get the .geode file (no installs needed)
1. Make a free GitHub account and create a new repository.
2. Upload everything in this folder (keep the `.github` folder).
3. Open the **Actions** tab -> "Build Geode Mod" -> wait for it to finish (a few minutes).
4. Download the **Build Output** artifact, unzip it, and you'll find `yourname.auto-deco.geode`.

## Install
Drop the `.geode` file into your `Geometry Dash/geode/mods` folder (or open it with Geode
installed, or use Geode's in-game "Install from file"). Restart GD.

## Use
Open a level in the editor -> Pause -> **Auto Deco** (bottom right) -> pick a theme.

Themes: Hell (red + gray), Heaven, Water, Forest, Space, Neon, Candy, Sunset, Ice, Toxic,
plus **Custom**, which uses the object IDs from the mod settings.
Themes place color triggers (channels 990/991 for the deco, plus background/ground) near the
start of the level, so move or delete them if you already have your own color setup.
Density, sides, No Touch, random scale and background recoloring are all in the mod settings.

Before publishing, change `yourname` in `mod.json`.
