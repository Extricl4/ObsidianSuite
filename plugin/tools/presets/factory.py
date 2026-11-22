"""The Factory folder: everyday sounds built from the plugin's own amps, the amp pack, pedals, effects
and the synth. Knob values are what you'd see on the amp's face (0-10); effects use their own units."""


def make(m):
    P, A, M, fx, S, cab = m.P, m.A, m.M, m.fx, m.S, m.cab
    F = 'Factory'

    # ------------------------------------------------------------------ CLEAN
    P('Glass Clean', F, 'Clean', 'Glassy compressed clean with a slow chorus and a soft room',
      A('persephone', channel='Ch I', voice='Bright', Volume=4, Treble=6.5, Middle=4.5, Bass=5, Reverb=0, switches=['chorus'], Speed=3, Depth=4),
      cab('2x12', mic='Condenser 414', pos=0.45), eq='glass',
      pre=[fx('compressor', Threshold=-28, Ratio=4, Attack=12, Release=180, Makeup=8, Blend=70)],
      post=[fx('lethe', Time=420, Feedback=22, Mix=14, Tone=5200, Mode='Tape'), fx('crypt', Decay=2.2, Size=55, Mix=20, Voice='PLATE')])
    P('Twin Sparkle', F, 'Clean', 'Big American clean: scooped, bright and wide', M('glass65', gain=3.2, bass=5.5, mid=4, treble=6.8, presence=6, master=6),
      cab('2x12', mic='Dynamic 57', pos=0.35), eq='bright', post=[fx('crypt', Decay=1.8, Size=45, Mix=18, Voice='SPRING')], width=120, doubler=10)
    P('Super Reverb Blues', F, 'Clean', 'Edge-of-breakup Super Reverb, pick harder and it growls',
      A('melinoe', mic='SM57 + 414', Volume=5.8, Treble=6, Bass=4.5, Middle=5.5, Reverb=2.5, Speed=0, Intensity=0),
      pre=[fx('chiron', Drive=-6, Blend=60, Output=3, Capture='MID')], post=[fx('crypt', Decay=1.6, Mix=12, Voice='SPRING')])
    P('Dumble Clean', F, 'Clean', 'Warm, round, expensive-sounding clean', A('hyperion', channel='Clean', voice='Balanced', Volume=5, Treble=5.5, Middle=5.5, Bass=5, Presence=5, Master=6),
      cab('1x12', mic='Ribbon 121', pos=0.3), eq='warm', post=[fx('lethe', Time=360, Feedback=18, Mix=10, Mode='Tape'), fx('crypt', Decay=1.4, Mix=14, Voice='ROOM')])
    P('Funk Quack', F, 'Clean', 'Envelope filter that quacks with your picking', M('glass65', gain=2.8, bass=4.5, mid=5, treble=6.5, presence=6),
      cab('2x12'), pre=[fx('compressor', Threshold=-24, Ratio=5, Attack=5, Release=120, Makeup=8), fx('bloom', Sensitivity=7, Range=7, Q=6, Type='Band Pass', Direction='Up', Mix=100)],
      post=[fx('crypt', Decay=0.9, Size=30, Mix=10, Voice='ROOM')])
    P('Country Snap', F, 'Clean', 'Chicken-picking twang: squashed, bright, slapback echo', M('glass65', gain=3.5, bass=4, mid=5.5, treble=7.5, presence=6.5),
      cab('2x12', mic='Dynamic 57', pos=0.25), eq='bright',
      pre=[fx('compressor', Threshold=-30, Ratio=8, Attack=3, Release=90, Makeup=10, Blend=80)],
      post=[fx('lethe', Time=110, Feedback=8, Mix=22, Mode='Tape', Tone=4500)])
    P('Jazz Box', F, 'Clean', 'Dark, round neck-pickup jazz tone', A('hyperion', channel='Clean', voice='Warm', Volume=4.5, Treble=3.8, Middle=6, Bass=6, Presence=3.5, Master=5),
      cab('1x12', mic='Ribbon 121', pos=0.55), eq='warm', post=[fx('crypt', Decay=1.2, Size=35, Mix=12, Voice='ROOM')])
    P('Surf Tremolo', F, 'Clean', 'Drippy spring reverb and a choppy tremolo', A('melinoe', mic='SM57', Volume=4.5, Treble=7, Bass=4, Middle=5, Reverb=7, Speed=5.5, Intensity=6),
      post=[fx('crypt', Decay=2.6, Mix=24, Voice='SPRING')])
    P('Vibe Clean', F, 'Clean', 'Throbbing uni-vibe over a warm clean', A('dionysus', channel='Green · clean', voice='Warm', Gain=4, Bass=5, Middle=5.5, Treble=5.5, Presence=5, Master=5),
      pre=[fx('spectre', Mode='Uni-Vibe', Rate=1.8, Depth=70, Mix=100)], post=[fx('crypt', Decay=1.8, Mix=16)])
    P('Chorus Pool', F, 'Clean', 'The JC-120 chorus with a long echo, made for arpeggios',
      A('persephone', channel='Ch II', voice='Clean', Volume=4.5, Treble=6, Middle=5, Bass=5, Reverb=2, switches=['chorus'], Speed=4.5, Depth=6),
      post=[fx('lethe', Sync=1, Division='1/8.', Feedback=35, Mix=22, Mode='Ping-Pong'), fx('crypt', Decay=3.2, Size=70, Mix=22, Voice='HALL')], width=130)

    # ------------------------------------------------------------------ CRUNCH
    P('Plexi Crunch', F, 'Crunch', 'Late-60s stack turned up: chords ring, single notes bite', A('tyche', Gain=7, Treble=6.5, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      cab('4x12', mic='Dynamic 57', pos=0.4), eq='vintage', gate=-64, post=[fx('crypt', Decay=1.4, Mix=12, Voice='ROOM')])
    P('Top Boost Chime', F, 'Crunch', 'Chiming AC30 with a treble booster in front', A('astraea', channel='Top Boost', boost='Treble booster', Volume=6.5, Treble=6.5, Bass=5, Tone_Cut=4, Reverb=0, Master=6),
      cab('2x12', mic='Dynamic 57', pos=0.35), post=[fx('lethe', Time=380, Feedback=20, Mix=12, Mode='Tape'), fx('crypt', Decay=1.5, Mix=14, Voice='ROOM')])
    P('JCM Classic', F, 'Crunch', 'Hot-rodded 80s Marshall, classic rock rhythm', A('chrysaor', Gain=7, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5.5),
      cab('4x12'), eq='mid', gate=-60, pre=[fx('helios', Voice='D II', Drive=-6, Level=4, Mix=100)], doubler=7)
    P('Klon Pushed', F, 'Crunch', 'A transparent overdrive pushing a Plexi into singing breakup', A('tyche', Gain=5, Treble=6, Middle=6.5, Bass=5, Presence=5.5, Volume=6, Master=6),
      cab('4x12', mic='Ribbon 121', pos=0.4), pre=[fx('chiron', Drive=3, Blend=70, Output=4, Capture='HIGH')], gate=-66,
      post=[fx('lethe', Time=400, Feedback=25, Mix=12, Mode='Tape')])
    P('Blue Channel', F, 'Crunch', 'Bogner blue crunch with a little extra push', A('dionysus', channel='Blue · crunch', voice='Protein OD', Gain=6, Bass=5, Middle=6, Treble=6, Presence=5.5, Master=5.5),
      gate=-62, doubler=8, post=[fx('crypt', Decay=1.2, Mix=10, Voice='ROOM')])
    P('Ford Overdrive', F, 'Crunch', 'Smooth Dumble overdrive, touch-sensitive and vocal', A('hyperion', channel='Overdrive', voice='Ford', Volume=5, Treble=5, Middle=6, Bass=5, Presence=5, Drive=6, Master=5),
      cab('1x12', mic='Dynamic 57'), post=[fx('lethe', Time=420, Feedback=25, Mix=14, Mode='Tape'), fx('crypt', Decay=1.8, Mix=14)])
    P('BE Crunch', F, 'Crunch', 'Friedman BE: thick, aggressive classic-rock crunch', A('lamia', channel='BE', mic='Mic blend', Gain=6, Bass=5, Middle=6, Treble=6, Presence=5.5, Resonance=5, Master=5),
      gate=-60, doubler=7)
    P('EL34 Grind', F, 'Crunch', 'Friedman EL34 channel with a Duke of Tone push', A('lamia', channel='EL34', mic='SM57', Gain=6.5, Bass=5, Middle=6.5, Treble=7.5, Presence=7, Resonance=5, Master=5),
      pre=[fx('emperor', Mode='OD', Drive=0, Volume=3, Mix=100)], gate=-60)

    # ------------------------------------------------------------------ RHYTHM (high gain)
    P('Tight Wall', F, 'Rhythm', 'Modern Rectifier wall, tightened with a TS-style boost', A('talos', boost='TS808 boost', Gain=6.5, Treble=6, Middle=4.5, Bass=6, Presence=6, Master=5, Output=5),
      cab('4x12', mic='Dynamic 57', pos=0.35), eq='tight', gate=-50, doubler=8, width=125)
    P('5150 Chug', F, 'Rhythm', 'The 5150 with a Fortin-modded TS9 for razor-tight chugs', A('hecate', Gain=6.5, Volume=5, Low=6, Mid=5, High=6.5, Presence=6, Resonance=6),
      cab('4x12', mic='Dynamic 57', pos=0.3), eq='tight', gate=-48, pre=[fx('graven', Capture='II', Drive=0, Level=4, Mix=100)], doubler=7, width=120)
    P('Djent Machine', F, 'Rhythm', 'Fortin Meshuggah amp, the 33 boost and a gated, scooped low end',
      A('nihil', amp='Fortin Meshuggah', Master=5, Presence=6, Bass=5.5, Middle=5, Treble=6.5, Drive=6, Gain=6),
      cab('4x12', mic='Dynamic 906', pos=0.3), eq='djent', gate=-44, pre=[fx('xxxiii', Drive=0, Level=3, Mix=100)], doubler=6, width=120)
    P('Thrash Attack', F, 'Rhythm', 'Scooped, fast and furious 80s thrash', A('chrysaor', Gain=8.5, Bass=6, Middle=4, Treble=7, Presence=6.5, Master=5),
      cab('4x12'), eq='thrash', gate=-52, pre=[fx('helios', Voice='D IV', Drive=-3, Level=5, Mix=100)], doubler=6)
    P('Red Channel', F, 'Rhythm', '5150 III red channel: modern, saturated, huge', A('moirai', channel='Red · lead', Gain=5.5, Volume=5, Low=5.5, Mid=5, High=6, Presence=6, Resonance=5.5),
      cab('4x12', mic='Dynamic 57', pos=0.35), eq='modern', gate=-50, pre=[fx('graven', Capture='I', Drive=-3, Level=3, Mix=100)], doubler=8, width=120)
    P('Stealth Djent', F, 'Rhythm', 'The 5150 III Stealth, dark, tight and heavy', A('typhon', channel='Red', Gain=6, Volume=5, Low=6, Mid=4.5, High=6, Presence=5.5, Resonance=6),
      cab('4x12', mic='Dynamic 906'), eq='djent', gate=-46, pre=[fx('maw', Capture='SLAM', Gain=-4, Level=2, Mix=100)], doubler=7, width=120)
    P('VH4 Wall', F, 'Rhythm', 'Diezel channel 3: a wall of German gain', A('vulcan', channel='Ch 3 · mega', voice='III Gain FAT', Gain=6, Low=6, Mid=5, High=6, Presence=5.5, Depth=6, Master=5),
      cab('4x12'), eq='tight', gate=-50, doubler=8)
    P('Powerball Crush', F, 'Rhythm', 'ENGL Powerball: tight, mid-forward German metal', A('ares', channel='Lead', mic='Mic mix', Gain=6.5, Bass=5.5, Middle=5.5, Treble=6, Presence=6, Depth=5.5, Master=5),
      gate=-50, doubler=7)
    P('Uberschall Grind', F, 'Rhythm', 'Bogner Uberschall: dirty, huge, ripping', A('erebus', Gain=6.5, Bass=6, Middle=5, Treble=6, Presence=5.5, Depth=6, Master=5),
      cab('4x12', mic='Dynamic 57', pos=0.4), eq='modern', gate=-50, doubler=8)
    P('Invective Riffs', F, 'Rhythm', 'Peavey Invective with its riff pedal: modern prog metal', A('lyssa', channel='Rhythm', voice='Riff 1 + pedal', Gain=6, Low=5.5, Mid=5.5, High=6, Presence=6, Resonance=5.5, Volume=5),
      cab('4x12', mic='Dynamic 57'), eq='djent', gate=-48, doubler=7, width=120)
    P("Satan's Wall", F, 'Rhythm', 'Randall Satan with a Fortin Grind: brutal and saturated', A('belial', mic='Mic centre', Gain=6, Bass=6, Middle=5, Treble=6, Presence=6, Depth=6, Master=5),
      pre=[fx('maw', Capture='KODY', Gain=-6, Level=2, Mix=100)], eq='tight', gate=-46, doubler=7)
    P('Granophyre Modern', F, 'Rhythm', 'Omega Granophyre with the Blower Box: tight, airy, modern', A('moros', boost='Blower Box', Gain=6, Bass=5.5, Mid=5, Treble=6, Level=5, Master=5, Tight=6, Air=5.5),
      cab('4x12'), gate=-48, doubler=7, width=115)
    P('Revv Red', F, 'Rhythm', 'Revv Generator red channel with a Plumes OD in front', A('niflheim', mode='Rhythm', boost='Plumes OD', Gain=6, Bass=5.5, Middle=5.5, Treble=6, Presence=6, Volume=5, Master=5),
      cab('4x12'), eq='tight', gate=-48, doubler=7)
    P('Mark IV Rhythm', F, 'Rhythm', 'Mesa Mark IV rhythm with a Fortin 808: chunky and focused', A('achlys', channel='R2 · rhythm', voice='Rhythm + Fortin 808', Gain=6.5, Treble=6.5, Bass=4.5, Middle=5.5, Presence=6, Master=5),
      cab('4x12'), eq='scoop', gate=-50, doubler=7)
    P('6505 Classic', F, 'Rhythm', 'The metalcore standard: 6505+ lead channel, crunch on', A('kratos', channel='Lead', switches=['crunch'], Lead_pre=6, Lead_post=5, Low=6, Mid=5, High=6.5, Resonance=6, Presence=6),
      cab('4x12', mic='Dynamic 57', pos=0.3), eq='tight', gate=-48, pre=[fx('graven', Capture='III', Drive=0, Level=4, Mix=100)], doubler=7, width=120)
    P('Doom Sludge', F, 'Rhythm', 'Down-tuned doom: a sub octave under a fuzz into a Rectifier', A('talos', Gain=7, Treble=5, Middle=5.5, Bass=7, Presence=5, Master=5, Output=5),
      cab('4x12', mic='Ribbon 121', pos=0.5), eq='fuzz', gate=-56,
      pre=[fx('ember', Fuzz=6, Tone=4, Level=0, Gate=3), fx('richter', Dry=100, Sub_1=45, Sub_2=0, Low_Pass=220, Drive=2)], post=[fx('crypt', Decay=2.5, Size=70, Mix=12)])
    P('Core Modern', F, 'Rhythm', 'Our own Obsidian Core amp: modern high gain, no pack needed', M('core', gain=7, bass=6, mid=5, treble=6.2, presence=6, depth=6.5, master=5),
      cab('4x12'), eq='tight', gate=-50, doubler=8, width=120)
    P('Recto Classic', F, 'Rhythm', 'The in-house Recto: 90s nu-metal wall, no pack needed', M('recto', gain=7.5, bass=6.5, mid=4.5, treble=6, presence=6, depth=7, master=5),
      cab('4x12'), eq='scoop', gate=-50, doubler=8)

    # ------------------------------------------------------------------ LEAD
    P('JP Lead', F, 'Lead', 'Petrucci-voiced JP-2C lead: smooth, fat, endless sustain', A('eris', channel='Ch 3 · lead', voice='Petrucci lead', boost='TS808', Gain=6, Treble=6, Middle=6.5, Bass=4.5, Presence=6, Master=5, Output=5),
      eq='lead', gate=-60, post=[fx('lethe', Time=430, Feedback=30, Mix=20, Mode='Tape', Tone=5000), fx('crypt', Decay=2.2, Size=60, Mix=16, Voice='HALL')])
    P('Shred Lead', F, 'Lead', 'JP-2C shred voicing with a delay that stays out of the way', A('eris', channel='Ch 3 · lead', voice='Ultimate lead', boost='Shred', Gain=6.5, Treble=6.5, Middle=6, Bass=4.5, Presence=6.5, Master=5, Output=5),
      eq='lead', gate=-58, post=[fx('lethe', Sync=1, Division='1/8.', Feedback=25, Mix=16, Mode='Ping-Pong', Tone=5000), fx('crypt', Decay=1.8, Mix=14)])
    P('SLO Sustain', F, 'Lead', 'Soldano SLO overdrive channel: liquid, singing sustain', A('thanatos', Preamp=7, Bass=5, Middle=6.5, Treble=6, Presence=5.5, Depth=5, Master=5),
      cab('4x12', mic='Dynamic 57', pos=0.45), eq='lead', gate=-60, post=[fx('lethe', Time=460, Feedback=32, Mix=20, Mode='Tape'), fx('crypt', Decay=2.4, Mix=16, Voice='PLATE')])
    P('IIC+ Lead', F, 'Lead', 'Mark IIC+ lead channel: the singing 80s lead', A('citadel', lead='Lead', Volume=7, Treble=7, Bass=3, Middle=6, Presence=6, Lead_drive=7, Master=5),
      cab('4x12'), eq='lead', gate=-60, post=[fx('lethe', Time=400, Feedback=28, Mix=18), fx('crypt', Decay=2, Mix=14, Voice='HALL')])
    P('Hellwin Lead', F, 'Lead', 'Big, polished modern hard-rock lead', A('strix', channel='Lead', voice='Lead 2', Gain=6, Bass=5, Middle=6, Treble=6, Presence=6, Resonance=5, Master=5),
      eq='lead', gate=-58, post=[fx('lethe', Sync=1, Division='1/4', Feedback=28, Mix=18, Mode='Ping-Pong'), fx('crypt', Decay=2.6, Mix=16, Voice='HALL')])
    P('Anthem Lead', F, 'Lead', 'Uberschall Rev Blue "Anthem": soaring stadium lead', A('nyx', channel='Lead', voice='Anthem', Gain=6, Bass=5, Middle=6, Treble=6, Presence=6, Depth=5, Master=5),
      eq='lead', gate=-58, post=[fx('lethe', Time=480, Feedback=35, Mix=22, Mode='Tape'), fx('seraph', Decay=4, Size=70, Shimmer=12, Mix=14)])
    P('Smooth Dumble Lead', F, 'Lead', 'Violin-like, compressed overdrive lead', A('hyperion', channel='Overdrive', voice='Smooth', Volume=5, Treble=5, Middle=6.5, Bass=5, Presence=5, Drive=7, Master=5),
      cab('1x12'), gate=-62, post=[fx('lethe', Time=420, Feedback=30, Mix=18, Mode='Tape'), fx('crypt', Decay=2.2, Mix=16)])
    P('Wah Solo', F, 'Lead', 'Auto-wah sweeping in time with the song over a hot JCM800', A('chrysaor', Gain=8, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      cab('4x12'), gate=-60, pre=[fx('lament', Mode='Envelope', Sensitivity=6.5, Range=7, Q=7, Mix=100)],
      post=[fx('lethe', Time=380, Feedback=25, Mix=16), fx('crypt', Decay=1.8, Mix=12)])
    P('Whammy Scream', F, 'Lead', 'Octave-up whammy for shrieking solos', A('kratos', channel='Lead', Lead_pre=6.5, Lead_post=5, Low=5, Mid=6, High=6.5, Resonance=5, Presence=6),
      cab('4x12'), gate=-56, pre=[fx('ascend', Shift=12, Position=100, Mix=70)], post=[fx('lethe', Time=350, Feedback=28, Mix=16), fx('crypt', Decay=2, Mix=14)])
    P('Fuzz Lead', F, 'Lead', 'Saturated fuzz lead into a Plexi, big and woolly', A('tyche', Gain=5, Treble=6, Middle=6, Bass=5, Presence=5, Volume=6, Master=6),
      cab('4x12', mic='Ribbon 121', pos=0.45), eq='fuzz', pre=[fx('ember', Fuzz=7.5, Tone=6, Level=-2, Gate=2)],
      post=[fx('lethe', Time=500, Feedback=35, Mix=20, Mode='Tape'), fx('crypt', Decay=2.5, Mix=16)])
    P('Mark V Lead', F, 'Lead', 'Mesa Mark V, IIC+ mode: tight and articulate lead', A('umbra', channel='Channel 3', mode='Mark IIC+', Gain=6, Treble=6.5, Middle=6, Bass=4, Presence=6, Master=5, Output=5),
      eq='lead', gate=-58, post=[fx('lethe', Time=420, Feedback=28, Mix=18), fx('crypt', Decay=2, Mix=14, Voice='PLATE')])

    # ------------------------------------------------------------------ AMBIENT
    P('Cathedral Swell', F, 'Ambient', 'No pick attack: notes rise into a shimmering cathedral', M('glass65', gain=2.5, bass=5, mid=4.5, treble=6, presence=5.5, master=5),
      post=[fx('aurora', Attack=500, Sensitivity=6, Reverb=45, Decay=6, Mix=100)], sfx=[fx('seraph', Decay=8, Size=90, Shimmer=55, Mix=40)])
    P('Night Drive', F, 'Ambient', 'Chorus, dotted-eighth echoes and a long hall', A('persephone', channel='Ch I', voice='Warm', Volume=4, Treble=6, Middle=5, Bass=5, Reverb=0),
      post=[fx('tide', Rate=0.35, Depth=55, Mix=40), fx('lethe', Sync=1, Division='1/8.', Feedback=45, Mix=30, Mode='Ping-Pong'), fx('crypt', Decay=5, Size=80, Mix=35, Voice='HALL')], width=140, doubler=12)
    P('Frozen Pad', F, 'Ambient', 'Hold a chord, FREEZE it, play over the top (SFX: Stasis)', M('glass65', gain=2.5, treble=6),
      post=[fx('crypt', Decay=3, Size=70, Mix=25)], sfx=[fx('stasis', Freeze=0, Fade=900, Level=80, Dry=100, Texture=40), fx('seraph', Decay=6, Shimmer=30, Mix=25)])
    P('Shoegaze Wall', F, 'Ambient', 'Fuzz into a huge washed-out reverb, very loud and very blurry', A('tyche', Gain=5, Treble=6, Middle=5, Bass=5, Presence=5, Volume=6, Master=6),
      cab('4x12'), pre=[fx('ember', Fuzz=8, Tone=5, Level=-3, Gate=1), fx('vortex', Rate=0.2, Depth=50, Feedback=40, Manual=2, Mix=40)],
      post=[fx('halcyon', Mode='VERB', Mix=55, Decay=8, Tone=6000, Low_Cut=150)], width=150)
    P('Catacombs', F, 'Ambient', 'A clean guitar played in real stone catacombs', A('melinoe', mic='AKG 414', Volume=4.5, Treble=6, Bass=5, Middle=5, Reverb=0),
      post=[fx('lethe', Time=600, Feedback=25, Mix=14, Mode='Tape'), fx('ossuary', Room='FAR', Mic='II', Mix=45, Decay=3.5, Tone=8000)])
    P('Ambient Slap', F, 'Ambient', 'Slap echo into a percussive shoegaze wash', A('dionysus', channel='Green · clean', voice='Bright', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Master=5),
      post=[fx('tide', Rate=0.6, Depth=35, Mix=30), fx('halcyon', Mode='SLAP', Mix=40, Decay=2), fx('crypt', Decay=6, Size=85, Mix=30)], width=140)
    P('Octave Heaven', F, 'Ambient', 'Shimmer an octave up with slow chorus, made for single notes', M('glass65', gain=2.5, treble=6.2),
      post=[fx('tide', Rate=0.3, Depth=50, Mix=35), fx('lethe', Time=700, Feedback=45, Mix=25, Mode='Tape')], sfx=[fx('seraph', Decay=10, Size=95, Shimmer=70, Interval=0, Mix=45)])
    P('Tremolo Dream', F, 'Ambient', 'Stereo tremolo in time with the song, drenched in reverb', A('persephone', channel='Ch I', voice='Clean', Volume=4, Treble=6, Middle=5, Bass=5),
      post=[fx('pulse', Sync=1, Division='1/8', Depth=60, Shape='Sine', Stereo=1), fx('crypt', Decay=4.5, Size=80, Mix=35, Voice='HALL')], width=140)

    # ------------------------------------------------------------------ SYNTH (guitar + SIREN)
    P('Neon Lead Synth', F, 'Synth', 'Guitar lead doubled by a soaring supersaw', A('dionysus', channel='Red · lead', voice='711 (br)', Gain=5.5, Bass=5, Middle=6, Treble=6, Presence=5.5, Master=5),
      gate=-58, synth=S('Cosmic Lead', mix=40), post=[fx('lethe', Time=380, Feedback=30, Mix=20), fx('crypt', Decay=2.5, Mix=18)])
    P('Arp Machine', F, 'Synth', 'Play a note and an 80s arpeggio follows your chords', M('glass65', gain=2.5, treble=6),
      synth=S('Eighties Pulse', mix=60), post=[fx('lethe', Sync=1, Division='1/8.', Feedback=25, Mix=15), fx('crypt', Decay=3, Mix=22)])
    P('Chip Shred', F, 'Synth', 'Shred and an 8-bit lead plays along', A('kratos', channel='Lead', Lead_pre=6, Lead_post=5, Low=5, Mid=6, High=6.5, Resonance=5, Presence=6),
      gate=-56, synth=S('Chip Hero', mix=40), post=[fx('crypt', Decay=1.5, Mix=12)])
    P('Wobble Riff', F, 'Synth', 'Djent riffs with a wobbling synth bass under them', A('nihil', amp='Fortin Meshuggah', Master=5, Presence=6, Bass=5, Middle=5, Treble=6.5, Drive=5.5, Gain=6),
      eq='djent', gate=-46, synth=S('Wobble Bass', mix=35), doubler=6)
    P('Synthwave Night', F, 'Synth', 'Clean chorus guitar with a VHS synth lead on top', A('persephone', channel='Ch I', voice='Bright', Volume=4, Treble=6.5, Middle=5, Bass=5, switches=['chorus'], Speed=3, Depth=5),
      synth=S('Synthwave Lead', mix=45), post=[fx('lethe', Sync=1, Division='1/8.', Feedback=35, Mix=22, Mode='Ping-Pong'), fx('crypt', Decay=3.5, Mix=25, Voice='HALL')], width=130)
    P('Sub Monster', F, 'Synth', 'A clean sub octave under a Rectifier for enormous riffs', A('talos', Gain=6.5, Treble=6, Middle=5, Bass=6, Presence=6, Master=5, Output=5),
      eq='tight', gate=-48, synth=S('Djent Sub', mix=30), doubler=7)
    P('Talkbox Funk', F, 'Synth', 'Funky clean rhythm and a talking synth', M('glass65', gain=3, mid=5.5, treble=6.5),
      pre=[fx('compressor', Threshold=-24, Ratio=5, Attack=5, Makeup=8)], synth=S('(O)rdinary Funk', mix=55), post=[fx('crypt', Decay=1, Mix=12)])
    P('Upside Down', F, 'Synth', 'Creepy 80s arpeggios behind a dark clean guitar', A('melinoe', mic='SM57 + 414', Volume=4, Treble=5, Bass=5, Middle=5, Reverb=3),
      synth=S('Upside Arp', mix=55), post=[fx('lethe', Time=520, Feedback=35, Mix=18, Mode='Tape'), fx('crypt', Decay=4, Mix=26)])
    P('Haunted Chapel', F, 'Synth', 'A cold church organ that follows your notes', M('glass65', gain=2, treble=5.5),
      synth=S('Haunted Organ', mix=70, level=-8), post=[fx('ossuary', Room='FAR', Mic='I', Mix=40, Decay=3.5)])

    # ------------------------------------------------------------------ FX
    P('Glitch Lab', F, 'FX', 'Stutters and bit-crush: hold SHATTER to glitch', A('kratos', channel='Lead', Lead_pre=6, Lead_post=5, Low=5.5, Mid=5.5, High=6, Resonance=5.5, Presence=6),
      gate=-50, sfx=[fx('shatter', Mode='Random', Division='1/16', Glitch=0, Auto=35, Mix=100), fx('hex', Bits=8, Downsample=4, Mix=35, Tone=9000)])
    P('Laser Room', F, 'FX', 'Every note fires a falling laser sweep', M('glass65', gain=3, treble=6),
      sfx=[fx('photon', Start=2600, End=120, Time=320, Feedback=45, Mix=55, Sensitivity=6)], post=[fx('crypt', Decay=2.5, Mix=20)])
    P('Ring Mod Robot', F, 'FX', 'Metallic ring-modulated clangs', A('chrysaor', Gain=6, Bass=5, Middle=6, Treble=6, Presence=6, Master=5),
      gate=-58, sfx=[fx('tesla', Frequency=440, Mix=55, LFO_Rate=0.4, LFO_Depth=35, Mode=0)])
    P('Octave Fuzz', F, 'FX', 'Ripping fuzz with an octave up, Hendrix-style', A('tyche', Gain=5, Treble=6, Middle=6, Bass=5, Presence=5, Volume=6, Master=6),
      cab('4x12'), pre=[fx('abyss', Dry=80, Oct_1=0, Oct_2=0, Oct_up=60, Tone=6), fx('ember', Fuzz=8, Tone=6, Level=-3, Gate=2)], eq='fuzz')
    P('Tape Stop Breakdown', F, 'FX', 'Heavy riffs with a tape stop on the SFX rack: hit STOP', A('hecate', Gain=6.5, Volume=5, Low=6, Mid=5, High=6.5, Presence=6, Resonance=6),
      eq='tight', gate=-48, sfx=[fx('gravity', Stop=0, Stop_Time=900, Start_Time=300, Mix=100)], doubler=7)
    P('Sub Octave Riff', F, 'FX', 'Bass and guitar in one: two sub octaves under a crunch', A('chrysaor', Gain=6.5, Bass=5, Middle=6, Treble=6, Presence=6, Master=5),
      gate=-56, pre=[fx('richter', Dry=100, Sub_1=60, Sub_2=25, Low_Pass=300, Drive=3)])
    P('Lo-Fi Radio', F, 'FX', 'An old radio in another room', M('top30', gain=4.5, treble=6),
      eq='lofi', post=[fx('hex', Bits=10, Downsample=6, Mix=60, Tone=4000), fx('filter', Cutoff=1800, Resonance=3, Type='Band Pass', LFO_Rate=0.2, LFO_Depth=20, Mix=100)])
    P('Jet Flanger', F, 'FX', 'Jet-plane flanger over a hot crunch', A('tyche', Gain=7, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      cab('4x12'), gate=-62, pre=[fx('vortex', Rate=0.25, Depth=70, Feedback=60, Manual=1.5, Mix=55)])
    P('Phase Swirl', F, 'FX', 'Slow 12-stage phaser over a warm crunch', A('dionysus', channel='Blue · crunch', voice='Plain', Gain=5, Bass=5, Middle=6, Treble=6, Presence=5.5, Master=5),
      pre=[fx('prism', Rate=0.4, Depth=70, Feedback=40, Stages='12', Mix=60)], post=[fx('crypt', Decay=1.6, Mix=12)])

    # ------------------------------------------------------------------ ACOUSTIC
    P('Acoustic Strummer', F, 'Acoustic', 'Your electric as a bright, roomy acoustic', A('orpheus', take='Take 1', Gain=4, Bass=5, Middle=4.5, Treble=6, Body=6, Reverb=2, Volume=5),
      eq='acoustic', pre=[fx('compressor', Threshold=-26, Ratio=3, Attack=10, Makeup=6, Blend=70)], post=[fx('crypt', Decay=1.5, Size=45, Mix=16, Voice='ROOM')])
    P('Fingerstyle Room', F, 'Acoustic', 'Warmer acoustic for fingerpicking', A('orpheus', take='Take 2', Gain=3.5, Bass=5.5, Middle=5, Treble=5.5, Body=7, Reverb=3, Volume=5),
      eq='acoustic', post=[fx('crypt', Decay=2, Size=55, Mix=20, Voice='HALL')])
    P('Twelve String', F, 'Acoustic', 'A 12-string feel: an octave up with a light chorus', A('orpheus', take='Take 1', Gain=4, Bass=4.5, Middle=4.5, Treble=6.5, Body=6, Reverb=2, Volume=5),
      eq='acoustic', pre=[fx('abyss', Dry=100, Oct_1=0, Oct_2=0, Oct_up=45, Tone=7)], post=[fx('tide', Rate=0.8, Depth=25, Mix=30), fx('crypt', Decay=1.8, Mix=16)])
