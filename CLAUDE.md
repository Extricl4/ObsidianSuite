# Notes for Claude
- Owner: Emil. He works on the UI and looks with Claude, and does the programming himself later.
- Edit only the folder your chat is about (visuals/ amps & cabs, visuals/ pedals, ui/, plugin/), then commit and push.
- Visual tweaks: edit `visuals/<amps|cabs|pedals>/<key>.json`, make a preview, render the final front view, then add it as a new version on that item's workshop page (links in README.md).
- `visuals/legacy/` scripts are old generators. Don't re-run them; they would overwrite the hand-tweaked specs.
- The cloud workspace restarts now and then. `visuals/render/queue.sh` resumes the front renders.
