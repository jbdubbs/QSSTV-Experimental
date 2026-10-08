Toolbar and gallery-navigation (gal_*) icons are from the Papirus icon theme
(https://github.com/PapirusDevelopmentTeam/papirus-icon-theme), GPL-3.0; see
LICENSE. The SVGs here are the originals, except start/stop/replay/erase/save/tone_rep/doubletone/sweep/edit where the
`.ColorScheme-Text` color was changed (green/red/orange; erase: red, save, gal_*, tone_rep, doubletone, sweep, edit: blue). `../tb_*.png` are
64px renders of them:

    magick -background none -density 1200 X.svg -resize 64x64 ../tb_X.png
