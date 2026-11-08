# Metal Sonic - 6th character for Sonic Mania Plus

RSDKv5 decomp mod (Android / Windows / Linux). Metal Sonic appears as a 6th card on the
Mania Mode / No Save slot screen (after Ray). He flies like Tails but never tires, never drowns,
and has the peel-out (hold UP, press JUMP). Sprites come from the "Metal Knuckles in Sonic Mania" mod.

Not supported: Encore, Time Attack, Competition. Super form uses Super Sonic's look. The in-stage
life icon is still Sonic's.

## Build from a phone (github.com in your mobile browser)
1. Create a new repository.
2. Add file > Upload files: upload every file from this folder EXCEPT the `.github` folder
   (CMakeLists.txt, MetalSonic.c, mod.ini, make_assets.py, README.md, 3 x MetalSonic*.gif, MetalSonic.bin,
   MetalSaveSelect.gif, MetalSaveSelect.bin). Commit.
3. Add file > Create new file. Type the name `.github/workflows/build.yml` (typing the slashes makes the
   folders), paste the contents of build.yml, commit.
4. Actions tab > "Build Metal Sonic mod" > Run workflow. Wait ~3 minutes.
5. Open the finished run, download the artifact for your platform (MetalSonic-Android / -Windows / -Linux),
   unzip it and put the `MetalSonic` folder into your game's `mods` folder, then enable it in the mods menu.

## Notes
* Mod files: `LogicFile=MetalSonic` in mod.ini loads MetalSonic.dll (Windows) or libMetalSonic.so (Linux/Android, arm64).
* Metal Sonic is stored as "Sonic" in save files, the mod remembers which slots are Metal Sonic in its own settings.
* `make_assets.py` shows how the sprite files were generated from the original mod's Sonic.bin.

## По-русски (коротко)
Загрузите все файлы (кроме папки .github) в новый репозиторий через сайт github.com в браузере телефона,
создайте файл `.github/workflows/build.yml` и вставьте туда содержимое build.yml. Затем Actions > Run workflow,
скачайте артефакт для своей платформы и положите папку `MetalSonic` в папку `mods` игры.
