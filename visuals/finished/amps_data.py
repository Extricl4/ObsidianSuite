"""Capture wiring + descriptions for the finished amps page."""
EVH = 'APP-5153-Ivory-'
MV = lambda s: f'Mesa Boogie - Mark V ({s} [18dBu]'
def mv(ch, tag): return f'Mesa Boogie - Mark V ({ch}) {tag} [18dBu]'
ROMAN = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'VIIII', 'X']
DR = 'MESA DUAL RECTIFIER 2025'

AMPS = [
 dict(key='moirai', skins=['moirai_green', 'moirai_blue', 'moirai_red'], name='Moirai', accent='#ff3b2f',
      based='EVH 5150 III 50W Ivory', pack='EVH 5150iii Ivory FULL Pack', by='ampspedalspickups', nfiles=28, kind='Amp only',
      knobs=['Gain', 'Volume', 'Low', 'Mid', 'High', 'Presence', 'Resonance'], gain=0,
      dims=[dict(id='channel', names=['Green · clean', 'Blue · crunch', 'Red · lead'], buttons=['ch1', 'ch2', 'ch3'],
                 leds=[['ch1_led'], ['ch2_led'], ['ch3_led']], skin=True, perknobs=[0, 1, 2, 3, 4], start=2)],
      caps={'0': dict(by='gain', files=[EVH + 'Green-' + ROMAN[i] for i in range(8)]),
            '1': dict(by='gain', files=[EVH + 'Blue-' + ROMAN[i] for i in range(10)]),
            '2': dict(by='gain', files=[EVH + 'Red-' + ROMAN[i] for i in range(10)])},
      intro='The three colour amps became one head. Like the real 5150 III, it has three channels, and each channel button also changes the board colour: '
            'green is the Nemesis look, blue is Styx and red is Hades. Each channel remembers its own Gain, Volume and EQ, the same as the per-channel controls on the real amp.',
      notes=['This is the most complete pack you have: every channel was captured at rising gain levels (8 green, 10 blue, 10 red), all at the same settings.',
             'Gain steps through those real captures and blends between neighbours, so the knob behaves like the real gain pot.',
             'Low, Mid and High are a tone stack wrapped around the capture. Presence and Resonance are shared by all channels, as on the 50W head.',
             'All files are amp only, so your cab and IR section works normally.']),
 dict(key='umbra', skins=['umbra_mk5'], name='Umbra', accent='#ff3a22',
      based='Mesa/Boogie Mark V', pack='Mesa Boogie - Mark V [18dBu]', by='kenazmusic', nfiles=37, kind='Amp only', esr='0.0105',
      knobs=['Gain', 'Treble', 'Middle', 'Bass', 'Presence', 'Master', 'Output'], gain=0, mid=2,
      dims=[dict(id='channel', names=['Channel 1', 'Channel 2', 'Channel 3'], button='channel', leds=[['ch1'], ['ch2'], ['ch3']], perknobs=[0, 1, 2, 3, 4, 5], start=2),
            dict(id='mode', button='mode', leds=[['md1'], ['md2'], ['md3']], per='channel', start=1)],
      caps={'0|0': dict(name='Clean', by='one', files=[mv('Ch 1 Clean', 'CLN')]),
            '0|1': dict(name='Fat', by='one', files=[mv('Ch 1 Fat', 'CLN SC')]),
            '0|2': dict(name='Tweed', by='gain', files=[mv('Ch 1 Tweed', 'EOB'), mv('Ch 1 Tweed', 'LG')], alt=[mv('Ch 1 Tweed', 'EOB SC')]),
            '1|0': dict(name='Edge', by='gain', files=[mv('Ch 2 Edge', t) for t in ('EOB', 'LG', 'MG')]),
            '1|1': dict(name='Crunch', by='gain', files=[mv('Ch 2 Cru', t) for t in ('EOB', 'LG', 'MG')]),
            '1|2': dict(name='Mark I', by='gain', files=[mv('Ch 2 MK-I', t) for t in ('LG', 'MG', 'HG')], alt=[mv(f'Ch 2 MK-I {b}', 'HG') for b in ('808', 'TS9', 'SD1')]),
            '2|0': dict(name='Extreme', by='mid', files=[mv('Ch 3 Ext', t) for t in ('HG scoop', 'HG bal', 'HG mids')],
                        alt=[mv('Ch 3 Ext', 'Black Metal I'), mv('Ch 3 Ext', 'Black Metal II')] + [mv(f'Ch 3 Ext {b}', 'HG') for b in ('808', 'TS9', 'SD1')]),
            '2|1': dict(name='Mark IIC+', by='mid', files=[mv('Ch 3 MK-IIC+', t) for t in ('HG scoop', 'HG bal', 'HG mids')], alt=[mv(f'Ch 3 MK-IIC+ {b}', 'HG') for b in ('808', 'TS9', 'SD1')]),
            '2|2': dict(name='Mark IV', by='mid', files=[mv('Ch 3 MK-IV', t) for t in ('HG scoop', 'HG bal', 'HG mids')], alt=[mv(f'Ch 3 MK-IV {b}', 'HG') for b in ('808', 'TS9', 'SD1')])},
      intro='Umbra is now a full Mark V: three channels, each with three modes, just like the real amp. CHANNEL picks 1, 2 or 3 and MODE picks that channel\'s voicing. '
            'Each channel remembers its own mode and knob settings.',
      notes=['Channel 1 and 2: where the pack has low, mid and high gain captures (Tweed, Edge, Crunch, Mark I), Gain blends between them.',
             'Channel 3 was captured at high gain with three EQ voicings: scoop, balanced and mids. Middle crossfades between those three real captures, then the tone stack shapes the rest.',
             'Extras not on the panel: Black Metal I/II and boosted versions (808, TS9, SD-1) of the high-gain modes. These could be used automatically when one of those pedals is switched on in front.',
             'Most accurate pack after the IIC+ (median accuracy score 0.0105, lower is better).']),
 dict(key='citadel', skins=['citadel_iic'], name='Citadel', accent='#e0b25a',
      based='Mesa/Boogie Mark IIC+', pack='Mesa Boogie Mark IIC+', by='siamsiamguite', nfiles=20, kind='Amp only', esr='0.0049',
      knobs=['Volume', 'Treble', 'Bass', 'Middle', 'Presence', 'Lead drive', 'Master'], gain=0,
      dims=[dict(id='lead', names=['Rhythm', 'Lead'], button='lead', leds=[[], ['lead_led']], start=1)],
      caps={'0': dict(by='gain', files=['Mesa Boogie Mark IIC+ Compressed Clean', "Mesa Boogie Mark IIC+ 80's Clean", 'Mesa Boogie Mark IIC+ Clean', 'Mesa Boogie Mark IIC+ Marshall Coil']),
            '1': dict(by='gain', knob=5, files=['Mesa Boogie Mark IIC+ Single Coil', 'Mesa Boogie Mark IIC+ Rock', 'Mesa Boogie Mark IIC+ Fat Gain', 'Mesa Boogie Mark IIC+ Solo', 'Mesa Boogie IIC+ Deth Lead'],
                      alt=['Mesa Boogie IIC+ Architects'])},
      intro='Citadel is the Mark IIC+. Like the real amp it has a Rhythm and a Lead mode, and the LEAD button switches between them (the red light is the lead lamp).',
      notes=['Rhythm: Volume steps through the four clean-to-edge captures in order of gain (compressed clean, 80s clean, clean, Marshall coil).',
             'Lead: Lead drive steps through the five lead captures (single coil, rock, fat gain, solo, Deth lead). Volume stays the rhythm control, like on the real amp.',
             'The pack also has combo versions of all ten sounds and an Architects preset. They could go behind a head/combo switch later.',
             'The most accurate pack in your folder (median accuracy score 0.0049).']),
 dict(key='ares', skins=['ares_pb'], name='Ares', accent='#ff3d2a',
      based='ENGL Powerball II', pack='ENGL Powerball II, Grossmann Fatbox, Celestion Raven', by='kaidownguitar', nfiles=8, kind='Full rig (cab in the capture)',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Crunch', 'Lead'], button='channel', leds=[['ch1'], ['ch2']], start=1),
            dict(id='mic', names=['Mic 57', 'Mic 75', 'Mic 906', 'Mic mix'], button='mic', leds=[['mic1'], ['mic2'], ['mic3'], ['mic4']], start=3)],
      caps={f'{c}|{m}': dict(by='one', files=[f'KD-PB-{cc}-{mm}']) for c, cc in enumerate(('CR', 'LD')) for m, mm in enumerate(('57', '75', '906', 'MIX'))},
      intro='Ares is now the Powerball II. CHANNEL switches between the crunch and lead captures. These are full-rig captures (Grossmann Fatbox cab with Celestion Ravens), '
            'so the second button, MIC, picks which mic the cab was recorded with: 57, 75, 906 or the mix.',
      notes=['Full rig: the cab is inside the capture, so the plugin should bypass its own cab section while Ares is loaded.',
             'One capture per channel and mic, so Gain pushes the input harder into the capture instead of stepping through gain levels.',
             'Bass, Middle, Treble, Presence and Depth are the tone stack around the capture.']),
 dict(key='strix', skins=['strix_hw'], name='Strix', accent='#ff2a24',
      based='Schecter Hellwin 100W (Synyster Gates)', pack="Hellwin 100W 'Synyster Settings'", by='gtrjeremy', nfiles=5, kind='Amp only',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Resonance', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Clean', 'Lead'], button='channel', leds=[['ch1'], ['ch2']], perknobs=[0, 1, 2, 3, 6], start=1),
            dict(id='voice', button='voice', leds=[['vo1'], ['vo2']], per='channel')],
      caps={'0|0': dict(name='Clean USA', by='one', files=['Hellwin Clean USA']), '0|1': dict(name='Clean UK', by='one', files=['Hellwin Clean UK']),
            '1|0': dict(name='Lead 1', by='one', files=['Hellwin Lead 1']),
            '1|1': dict(name='Lead 2', by='one', files=['Hellwin Lead 2'], alt=['Hellwin Lead 2 (Capture 2)', 'Hellwin SYN100-H Lead 2 (Knobs at Noon)'])},
      intro='Strix runs Synyster Gates\' Hellwin settings. CHANNEL switches Clean and Lead, and VOICE picks between the two captures of each: USA or UK clean, Lead 1 or Lead 2. '
            'The middle window has also been rebuilt: three lancets on mullions, with a rose on top that ties into every arch, so nothing floats any more.',
      notes=['Amp only, so it works with your cab section.',
             'One capture per voice, so Gain drives the input into it. Each channel remembers its own Gain, EQ and Master.',
             'Spare captures from your folder: a second Lead 2 take, and the SYN100-H Lead 2 "knobs at noon" from the other Hellwin pack.']),
 dict(key='nihil', skins=['nihil_fm'], name='Nihil', accent='#3dff7a',
      based='Fortin Meshuggah / Sunn Model T', pack='FORTIN MESHUGGAH + SUNN MODEL T + FRIEDMAN BUTTERSLAX', by='nosatec', nfiles=3, kind='Full rig (cab in the capture)',
      knobs=['Master', 'Presence', 'Bass', 'Middle', 'Treble', 'Drive', 'Gain'], gain=6,
      dims=[dict(id='amp', names=['Fortin Meshuggah', 'Sunn Model T + Butterslax', 'Blend of both'], button='amp', leds=[['amp1'], ['amp2'], ['amp3']])],
      caps={'0': dict(by='one', files=['FORTIN MESHUGGAH']), '1': dict(by='one', files=['BUTTERSLAX + MODEL T']), '2': dict(by='one', files=['BLEND'])},
      intro='Nihil runs the Fortin Meshuggah pack. The pack has three sounds rather than three channels: the Fortin, a Sunn Model T pushed by a Friedman Butterslax, '
            'and a blend of the two. The AMP button steps through them (F, S, B), and the old OFF/STBY toggles are gone. The green lamp is the power switch.',
      notes=['Full rig, so the plugin should bypass its own cab while Nihil is loaded.',
             'One capture per sound: Gain and Drive both push the input into it, Gain harder.',
             'For the plugin\'s pedal section you also have the Fortin 33 (Thordendal boost) capture, which suits this amp.']),
 dict(key='talos', skins=['talos'], name='Talos', accent='#ff6a1a', new=True,
      based='Mesa/Boogie Dual Rectifier', pack=DR, by='deathblossomaudio', nfiles=5, kind='Amp only', esr='0.0096',
      knobs=['Gain', 'Treble', 'Middle', 'Bass', 'Presence', 'Master', 'Output'], gain=0,
      dims=[dict(id='boost', names=['Boost off', 'TS808 boost'], button='boost', leds=[[], ['boost_led']])],
      caps={'0': dict(by='gain', files=[f'1. {DR} _ CRUNCH _ RHYTHM #1', f'2. {DR} _ RHYTHM #2', f'3. {DR} _ RHYTHM #3', f'4. {DR} _ RHYTHM #4']),
            '1': dict(by='one', files=[f'5. {DR} _ TS808 BOOST _ RHYTHM #5'])},
      intro='Talos is new: a hot-rod head with an aluminium diamond-plate front, rivets, a chrome exhaust grille with the power tubes glowing behind it, and flames licking out of both ends. '
            'It runs the Dual Rectifier 2025 pack. The four rhythm captures rise in gain, and the fifth was captured with a TS808 in front, so that one sits behind the BOOST button.',
      notes=['Gain blends through captures 1 to 4 (crunch to full rhythm). With BOOST on, it uses the TS808 capture.',
             'Amp only, so it works with your cab section. Accuracy score 0.0096 (median).',
             'Alternative: the Rev F red channel + OD808 capture in your folder (full rig).']),
 dict(key='achlys', skins=['achlys'], name='Achlys', accent='#bcd4ff', new=True,
      based='Mesa/Boogie Mark IV', pack='Mesa Boogie Mark IV pack', by='petrc', nfiles=9, kind='Amp only',
      knobs=['Gain', 'Treble', 'Bass', 'Middle', 'Presence', 'Lead drive', 'Master'], gain=0,
      dims=[dict(id='channel', names=['R1 · clean', 'R2 · rhythm', 'Lead'], button='channel', leds=[['ch1'], ['ch2'], ['ch3']], perknobs=[0, 1, 2, 3, 6], start=2),
            dict(id='voice', button='voice', leds=[['vo1'], ['vo2'], ['vo3'], ['vo4'], ['vo5']], per='channel')],
      caps={'0|0': dict(name='Bright clean', by='one', files=['MB Mark IV - Bright Clean']), '0|1': dict(name='Fat clean', by='one', files=['MB Mark IV - Fat Clean']),
            '1|0': dict(name='Rhythm', by='one', files=['MB Mark IV - Ch2 Rhythm']), '1|1': dict(name='Rhythm + Fortin 808', by='one', files=['MB Mark IV - Ch2 Rhythm + Fortin 808']),
            '2|0': dict(name='Lead midgain', by='one', knob=5, files=['MB Mark IV - Ch3 Lead Midgain + no EQ']),
            '2|1': dict(name='Lead midgain + EQ', by='one', knob=5, files=['MB Mark IV - Ch3 Lead Midgain + EQ']),
            '2|2': dict(name='Lead harmonics + EQ', by='one', knob=5, files=['MB Mark IV - Ch3 Lead Harmonics + EQ']),
            '2|3': dict(name='Harmonics, presence pulled', by='one', knob=5, files=['MB Mark IV - Ch3 Lead Harmonics + EQ + Presence Pulled']),
            '2|4': dict(name='Lead + Fortin Blade', by='one', knob=5, files=['MB Mark IV - Ch3 Lead + Fortin Blade'])},
      intro='Achlys is new: a blacked-out Mark IV-style head in smooth matte black. It has black-on-black cloth, black knobs, grey print and only a faint cold-white light. '
            'CHANNEL picks R1, R2 or Lead, like the Mark IV\'s three channels, and VOICE steps through the captures in that channel (2, 2 and 5).',
      notes=['All nine captures are used, and all are amp only.',
             'On R1 and R2 Gain drives the capture. On Lead it\'s Lead drive, as on the real amp.',
             'The lead voices include the graphic-EQ-on versions and one with the presence pulled, so you get Mark IV EQ moves without modelling them.'])
]

INV = 'Invec.'
AMPS += [
 dict(key='lyssa', skins=['lyssa'], name='Lyssa', accent='#b25cff', new=True,
      based='Peavey Invective 120', pack='PEAVEY  INVECTIVE', by='valdir0545', nfiles=5, kind='Full rig (cab in the capture)',
      knobs=['Gain', 'Low', 'Mid', 'High', 'Presence', 'Resonance', 'Volume'], gain=0,
      dims=[dict(id='channel', names=['Rhythm', 'Lead'], button='channel', leds=[['ch1'], ['ch2']], perknobs=[0, 1, 2, 3, 6]),
            dict(id='voice', button='voice', leds=[['vo1'], ['vo2'], ['vo3']], per='channel', start=1)],
      caps={'0|0': dict(name='Rhythm', by='one', files=['Invec.  Amp+Cab']),
            '0|1': dict(name='Riff 1 + pedal', by='one', files=['Invec. Riff 1   P.d.+Amp+Cab']),
            '0|2': dict(name='Riff 2 + pedal', by='one', files=['Invec. Riff 2   P.d.+Amp+Cab']),
            '1|0': dict(name='Solo', by='one', files=['Invec.  SOLO   Amp+Cab']),
            '1|1': dict(name='Solo + pedal', by='one', files=['Invec.  SOLO   P.D.+Amp+Cab'])},
      intro='Lyssa is new, inspired by the Invective: a modern gunmetal head with slanted vents raked toward a raised centre diamond, and violet tube glow behind the vents. '
            'CHANNEL switches Rhythm and Lead, and VOICE picks the capture inside each: plain, Riff 1 or Riff 2 on rhythm, Solo with or without the pedal on lead.',
      notes=['All five Invective captures are used. They are full rig, so the plugin should bypass its own cab while Lyssa is loaded.',
             '"P.D." in the file names means the capture was made with a pedal in front. Those are the pushed versions.',
             'One capture per voice, so Gain drives the input into it. Each channel keeps its own Gain, EQ and Volume.']),
 dict(key='belial', skins=['belial'], name='Belial', accent='#ff1e14', new=True,
      based='Randall Satan', pack='REAL Olas Randall Satan 50', by='olaenglund', nfiles=2, kind='Full rig (cab in the capture)',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'], gain=0,
      dims=[dict(id='mic', names=['Mic centre', 'Mic off centre'], button='mic', leds=[['mic1'], ['mic2']])],
      caps={'0': dict(by='one', files=['Randall SATAN 50 Mic Center']), '1': dict(by='one', files=['Randall SATAN 50 Mic Off Center'])},
      intro='Belial is new, blacked out in the spirit of the Satan: thin black grille bars run across the whole front, with red-hot tubes and red light glowing between them. '
            'A black-chrome gothic "Belial" script floats in front of the bars. It runs the Satan 50 captures made by Ola Englund, who designed the real amp.',
      notes=['The pack has one sound recorded with two mic positions, so the button switches the mic (C = centre, O = off centre) rather than a channel.',
             'Full rig, so the plugin should bypass its own cab while Belial is loaded.',
             'One capture, so Gain drives the input into it. For more Satan sounds, look for an amp-only Satan pack with several gain levels on Tone3000.'])
]

FSR = 'Fender Super Reverb_ EQ Flat, Volume 3, '
UB = '[AMP] UBER.BLUE-Mar412-'
UBL = ['Anthem', 'Basher', 'Blue Lights', 'Clover', 'Cookie', 'Darkness', 'Glazed', 'Holly Rocks', 'Instant gratification', 'Noon', 'Rosemary', 'Striptaser']
UBC = ['Noon', 'Cookie', 'Instant gratification']
JCM = lambda mv, g: f'JCM800 2203 - P5 B5 M5 T5 MV{mv} G{g} - AZG - 700'
JP = lambda n, rest: f'JP2C Capture {n} {rest} - Emil Rohbe'
AMPS += [
 dict(key='melinoe', skins=['melinoe_sr'], name='Melinoe', accent='#a160ff',
      based="Fender Super Reverb ('77)", pack='Fender Super Reverb 1977', by='tone3000', nfiles=3, kind='Full rig (cab in the capture)', esr='0.0023',
      knobs=['Volume', 'Treble', 'Bass', 'Middle', 'Reverb', 'Speed', 'Intensity'], gain=0,
      dims=[dict(id='mic', names=['SM57', 'AKG 414', 'SM57 + 414'], button='mic', leds=[['mic1'], ['mic2'], ['mic3']], start=2)],
      caps={'0': dict(by='one', files=[FSR + 'sm57']), '1': dict(by='one', files=[FSR + 'AKG 414']), '2': dict(by='one', files=[FSR + 'sm57 and AKG 414'])},
      intro='Melinoe is now a Fender Super Reverb: the 4x10 clean combo, captured with the EQ flat at volume 3. The knobs are labelled like the real amp\'s vibrato channel '
            '(Volume, Treble, Bass, Middle, Reverb, Speed, Intensity), and the old STBY switch became a MIC button for the three versions in the pack.',
      notes=['All three captures are the same clean sound recorded through different mics: an SM57, an AKG 414, or both blended. The button picks the mic, not a channel, because the pack has only one channel.',
             'Full rig, so the plugin should bypass its own cab while Melinoe is loaded.',
             'Volume drives the input into the capture. Reverb, Speed and Intensity need the plugin\'s own spring reverb and tremolo after the capture, because the capture was made with those off.',
             'Very accurate captures (accuracy score about 0.002).']),
 dict(key='nyx', skins=['nyx_ub'], name='Nyx', accent='#36d2ff',
      based='Bogner Überschall Rev Blue', pack='Bogner Uberschall', by='2dor', nfiles=16, kind='Amp only',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Clean', 'Lead'], button='channel', leds=[['ch1'], ['ch2']], perknobs=[0, 1, 2, 3, 6], start=1),
            dict(id='voice', button='voice', leds=[], per='channel')],
      caps=dict([(f'0|{i}', dict(name=n, by='one', files=[UB + f'CLEAN {n} - DI'])) for i, n in enumerate(UBC)] +
                [(f'1|{i}', dict(name=n, by='one', files=[UB + f'LEAD {n} - DI'], **({'alt': ['[AMP] UBER.BLUE-Mes412-LEAD Striptaser - DI']} if n == 'Striptaser' else {})))
                 for i, n in enumerate(UBL)]),
      intro='Nyx is now a Bogner Überschall (Rev Blue). CHANNEL switches Clean and Lead, and VOICE steps through the named settings the capture maker saved on each '
            'channel: 3 cleans and 12 leads. The readout shows which one is loaded.',
      notes=['All 16 files are amp only (DI), so your cab section works normally.',
             'The lead voices are all high gain (around the same level) with different EQ and character, so VOICE is more like flipping through presets than a gain switch.',
             'Gain drives the input into the loaded capture. Each channel remembers its own Gain, EQ and Master.',
             'Spare: a second Striptaser lead captured on a different cab during recording (Mes412).']),
 dict(key='chrysaor', skins=['chrysaor'], name='Chrysaor', accent='#e8b04a', new=True,
      based='Marshall JCM800 2203', pack='Marshall JCM 800 2203', by='arthm', nfiles=30, kind='Amp only',
      knobs=['Presence', 'Bass', 'Middle', 'Treble', 'Master', 'Gain'], gain=5, master=4,
      dims=[],
      caps={'': dict(by='grid', files=[[JCM(mv, g) for g in range(1, 11)] for mv in (5, 6, 7)])},
      intro='Chrysaor is new, named after the golden-sword son of Medusa. It\'s gold and black like a Marshall, in the same family as Melinoe, Hyperion and the tarot amps: '
            'black lacquer with gold baroque scrollwork, and a winged sword in a laurel wreath under a crown. Below that is a brushed-gold control panel with the 2203\'s six '
            'knobs in their real order, the inputs on the right, and the power lamp on the left.',
      notes=['This pack is a full grid: Gain 1 to 10, captured at three Master settings (5, 6 and 7). That\'s 30 captures, all with the EQ at 5.',
             'Gain steps through the ten gain captures, and Master crossfades between the three master levels, so both knobs behave like the real amp. The power section pushes harder as Master goes up.',
             'The 2203 has one channel, so there are no channel buttons.',
             'Amp only, so it works with your cab section.']),
 dict(key='eris', skins=['eris'], name='Eris', accent='#3dffd0', new=True,
      based='Mesa/Boogie JP-2C (John Petrucci)', pack='Mesa Boogie JP-2C Pack', by='emilrohbe', nfiles=26, kind='Amp only',
      knobs=['Gain', 'Treble', 'Middle', 'Bass', 'Presence', 'Master', 'Output'], gain=0,
      dims=[dict(id='channel', names=['Ch 1 · clean', 'Ch 2 · crunch', 'Ch 3 · lead'], button='channel', leds=[['ch1'], ['ch2'], ['ch3']], perknobs=[0, 1, 2, 3, 5], start=2),
            dict(id='voice', button='voice', leds=[['vo1'], ['vo2'], ['vo3'], ['vo4'], ['vo5']], per='channel'),
            dict(id='boost', names=['No boost', 'TS808', 'Shred'], button='boost', leds=[[], ['bo1'], ['bo2']])],
      caps={},
      intro='Eris is new: a compact, matte-black head shaped like the JP-2C. The cabinet\'s top front is cut away at a steep slant (about 55 degrees), and that slanted face '
            'is one sheet of black mesh framed by the cabinet edges, so the frame slants with it. Below it is a short, flat control panel with a solid-state look: no tubes, tick-mark dials, '
            'a thin teal light. CHANNEL picks the three JP-2C channels, VOICE steps through the captured settings on that channel, and BOOST adds the TS808 or Shred versions.',
      notes=['Channel 1: clean at gain 6 and gain 10. Gain blends between them.',
             'Channel 3 voices: Lead (gain 6, 8 and 10, blended by Gain), Scooped lead (gain 8 and 10), JP lead, Ultimate lead, and Petrucci\'s current settings.',
             'BOOST swaps to the matching TS808 or Shred capture on the Lead, Scooped lead and JP lead voices. Voices without a boosted capture stay as they are, and your pedal section can add the boost instead.',
             'Not used on the panel: the power-amp-only captures (useful later for Eidolon), two EQ variants, and the five old TS808 captures the pack itself marks as bad.'])
]
_e = AMPS[-1]['caps']
def _jp(vname, files, by='one', **kw): return dict(name=vname, by=by, files=files, **kw)
CH3 = [('Lead', [JP(2, 'CH3 G6 T7 M4 B2 P4'), JP(1, 'CH3 G8 T7 M4 B2 P4'), JP(3, 'CH3 G10 T7 M4 B2 P4')], 'gain', JP(4, 'CH3 G8 T7 M4 B2 P4 TS808'), JP(6, 'CH3 G8 T7 M4 B2 P4 Shred')),
       ('Scooped lead', [JP(8, 'CH3 G8 T7 M2 B2 P5'), JP(9, 'CH3 G10 T7 M2 B2 P5')], 'gain', JP(11, 'CH3 G8 T7 M2 B2 P5 TS808'), JP(10, 'CH3 G8 T7 M2 B2 P5 Shred')),
       ('JP lead', [JP(12, 'JP Lead CH3 G9 T6 M3 B3 P6')], 'one', JP(15, 'JP Lead CH3 G9 T6 M3 B3 P6 TS808'), JP(14, 'JP Lead CH3 G9 T6 M3 B3 P6 Shred')),
       ('Ultimate lead', [JP(18, 'Ultimate Lead')], 'one', None, None),
       ('Petrucci lead', [JP(19, 'Petrucci Lead Current settings')], 'one', None, None)]
for b in range(3):
    _e[f'0|0|{b}'] = _jp('Clean', [JP(16, 'Clean CH1 G6 T6 M1 B4 P6'), JP(17, 'Clean CH1 G10 T6 M1 B4 P6')], 'gain', **({} if b == 0 else {'hide': True, 'note': 'no boosted capture'}))
    _e[f'1|0|{b}'] = _jp('JP rhythm', [JP(13, 'JP RHY CH2 G8 T6 M4 B3 P6')], **({} if b == 0 else {'hide': True, 'note': 'no boosted capture'}))
    for v, (nm, fs, by, ts, sh) in enumerate(CH3):
        if b == 0: _e[f'2|{v}|0'] = _jp(nm, fs, by, **({'alt': [JP(5, 'CH3 G8 T7 M4 B2 P6'), JP(7, 'CH3 G8 T7 M4 B2 P4 EQ Less Bass')]} if v == 0 else {}))
        else:
            f = ts if b == 1 else sh
            _e[f'2|{v}|{b}'] = _jp(nm, [f], 'one') if f else _jp(nm, fs, by, hide=True, note='no boosted capture')
AMPS[-1]['notes'][-1] += ' The power-amp-only files are ' + JP(20, 'Power Amp Only No EQ') + ' and ' + JP(21, 'Power Amp Only With EQ') + '.'

# Chrysaor grid: rows = Master 5/6/7 (knob 4), cols = Gain 1-10 (knob 5)
_c = [a for a in AMPS if a['key'] == 'chrysaor'][0]['caps']['']
_c.update(rowKnob=4, colKnob=5, rowNames=['Master 5', 'Master 6', 'Master 7'])

P65 = lambda v, g: f'APP-6505Plus-{v}-Gain-{g:02d}'
DZ2 = lambda ch, nm, mic='BLEND #1': f'[AMP] DiZLVH4-{ch} {nm} - {mic}'
DZM = lambda ch, nm: [DZ2(ch, nm, m) for m in ('BLEND #2', 'BLEND #3', 'SM57', 'SM58')]
GR = 'Omega Granophyre'
RV = lambda t: f'REVV120 RED - {t}'
AMPS += [
 dict(key='kratos', skins=['kratos'], name='Kratos', accent='#ff3a24', new=True,
      based='Peavey 6505+', pack='6505 Gain Range Pack (High Gain)', by='ampspedalspickups', nfiles=30, kind='Amp only', esr='0.0005–0.02',
      knobs=['Rhythm pre', 'Rhythm post', 'Lead pre', 'Lead post', 'Low', 'Mid', 'High', 'Resonance', 'Presence'], gain=0,
      dims=[dict(id='channel', names=['Rhythm', 'Lead'], button='channel', leds=[['ch1'], ['ch2']], start=1)],
      switches=[dict(id='crunch', button='crunch', led='crunch_led', label='Crunch', does='more drive into the rhythm captures'),
                dict(id='bright', button='bright', led='bright_led', label='Bright', does='treble lift on the rhythm channel')],
      caps={'0': dict(by='gain', files=[P65('Clean', g) for g in range(1, 11)]),
            '1': dict(by='grid', rowKnob=5, colKnob=2, rowNames=['Scooped', 'Mid forward'],
                      files=[[P65('Scooped', g) for g in range(1, 11)], [P65('MidForward', g) for g in range(1, 11)]])},
      intro='Kratos is new, built on the 6505+: a bead-blasted matte steel front with a heavy square grate, bolted frame and a stamped centre plate, with red-hot power tubes '
            'behind the grate. The panel follows the real 6505+ layout: Rhythm pre/post, Lead pre/post, then the shared Low, Mid, High, Resonance and Presence, '
            'plus CRUNCH and BRIGHT for the rhythm channel and a CHANNEL button with green (rhythm) and red (lead) lamps.',
      notes=['The pack captured each voicing at ten gain levels: Clean 1–10, Scooped 1–10 and Mid-forward 1–10, all amp only.',
             'Rhythm: Rhythm pre steps through the ten clean captures.',
             'Lead: Lead pre steps through gain 1 to 10, and Mid crossfades between the scooped and the mid-forward set. Both knobs play real captures.',
             'Crunch and Bright weren\'t part of the capture session, so the plugin handles them itself: Crunch pushes the input harder and Bright adds a treble shelf. They light up and work, but they aren\'t captured.',
             'Very accurate at low gain (0.0005) and still good at full gain (around 0.02).']),
 dict(key='vulcan', skins=['vulcan_vh4'], name='Vulcan', accent='#ff6a1a',
      based='Diezel VH4', pack='Diezel VH4 (three packs merged)', by='deathblossomaudio · amalgamaudio · 2dor', nfiles=44, kind='Mostly amp only (two voices full rig)',
      knobs=['Gain', 'Low', 'Mid', 'High', 'Presence', 'Depth', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Ch 1 · clean', 'Ch 2 · crunch', 'Ch 3 · mega', 'Ch 4 · lead'], button='channel', leds=[['ch1'], ['ch2'], ['ch3'], ['ch4']], perknobs=[0, 1, 2, 3], start=2),
            dict(id='voice', button='voice', leds=[['vo1'], ['vo2'], ['vo3']], per='channel'),
            dict(id='boost', names=['No boost', 'TS808'], button='boost', leds=[[], ['bo1']])],
      caps={},
      intro='Yes, they can be combined. NAM files can\'t be merged into one model, but they can share one amp, each covering what it does best. Vulcan is now a 4-channel VH4: '
            'the accurate amp-only Deathblossom captures cover channels 2 and 4 (with gain steps and TS808), Amalgam\'s cab-free channel III covers 3, '
            'and 2dor\'s pack fills channel 1 and adds two extra voices.',
      notes=['Ch 1: 2dor\'s "Breakup", the only clean/breakup capture in the three packs. It\'s full rig, so the plugin\'s cab switches off on this channel.',
             'Ch 2: Deathblossom Crunch 1–3 (amp only). Gain blends between them.',
             'Ch 3: VOICE 1 is Amalgam\'s "III Gain FAT" (amp only). VOICE 2 and 3 are 2dor\'s "Physics" and "Potty Time" (full rig).',
             'Ch 4: VOICE 1 is Deathblossom Rhythm 1–3 (Gain blends), and BOOST swaps in their TS808 versions. VOICE 2 is 2dor\'s "Lead the Way" (full rig).',
             'Left out: 2dor\'s extra mic versions of each sound (BLEND #2/#3, SM57, SM58, listed as spares) and their power-amp-only "EL34 Juice" captures, which suit Eidolon better.']),
 dict(key='moros', skins=['moros_gp'], name='Moros', accent='#4d8cff',
      based='Omega Ampworks Granophyre', pack='Omega Granophyre', by='tonecast', nfiles=3, kind='Amp only',
      knobs=['Gain', 'Bass', 'Mid', 'Treble', 'Level', 'Master', 'Tight', 'Air'], gain=0,
      dims=[dict(id='boost', names=['Amp only', 'Blower Box', 'Plumes M2'], button='boost', leds=[[], ['bo1'], ['bo2']])],
      caps={'0': dict(by='one', files=[GR + ' Amp Head']), '1': dict(by='one', files=[GR + ' + Idiot Box FX Blower Box']), '2': dict(by='one', files=[GR + ' + Plumes M2'])},
      intro='Moros runs the Granophyre. The pack is one channel captured three ways: the head on its own, with an Idiot Box Blower Box in front, and with a Plumes (mode 2) in front. '
            'The old VOICE switch is now a BOOST button that steps through those (BB = Blower Box, PL = Plumes), the HIGH/LOW and ACTIVE switches are gone, '
            'and the blue lamp is the power switch.',
      notes=['All three are amp only, so your cab section works normally.',
             'One capture per setting, so Gain drives the input into it. Bass, Mid, Treble, Tight and Air shape the sound around the capture, and Level and Master set the volume.',
             'The pedals were captured inside the amp sound, so you don\'t need the plugin\'s own boost on top.']),
 dict(key='niflheim', skins=['niflheim_rv'], name='Niflheim', accent='#73b8ff',
      based='Revv Generator 120 MkIII (red channel)', pack='REVV GENERATOR 120 RED MKIII', by='deathblossomaudio', nfiles=9, kind='Amp only', esr='0.0007–0.012',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Volume', 'Master'], gain=0,
      dims=[dict(id='mode', names=['Crunch', 'Rhythm'], button='mode', leds=[['md1'], ['md2']], start=1),
            dict(id='boost', names=['No boost', 'Plumes OD'], button='boost', leds=[[], ['bo1']])],
      caps={'0|0': dict(by='gain', files=[RV(f'CRUNCH {i}') for i in (1, 2, 3)]),
            '0|1': dict(by='gain', files=[RV(f'CRUNCH {i}') for i in (1, 2, 3)], hide=True, note='no boosted crunch capture'),
            '1|0': dict(by='gain', files=[RV(f'RHYTHM {i}') for i in (1, 2, 3, 4, 5)]),
            '1|1': dict(by='one', files=[RV('RHYTHM 6 PLUMES OD')])},
      intro='Niflheim runs the Revv Generator 120 MkIII red channel. MODE switches between the crunch and rhythm sets the pack was captured in, and BOOST (OD) switches to '
            'the rhythm capture made with a Plumes overdrive in front. The old Standby switch is gone, the ice-blue lamp is the power switch, and Frost is now Volume.',
      notes=['Crunch: Gain blends through three rising captures. Rhythm: Gain blends through five.',
             'All amp only and very accurate (0.0007 at crunch, around 0.005 on the rhythm captures).',
             'Only the red channel was captured, so there\'s no green/purple/blue channel switch.'])
]
_v = [a for a in AMPS if a['key'] == 'vulcan'][0]['caps']
DB = lambda t: f'Diezel {t}'
for b in range(2):
    h = {} if b == 0 else {'hide': True, 'note': 'no boosted capture for this voice'}
    _v[f'0|0|{b}'] = dict(name='Breakup', by='one', files=[DZ2('CH1', 'Breakup')], rig=True, alt=DZM('CH1', 'Breakup'), **h)
    _v[f'1|0|{b}'] = dict(name='Crunch', by='gain', files=[DB(f'CH 2 Crunch {i}') for i in (1, 2, 3)], **h)
    _v[f'2|0|{b}'] = dict(name='III Gain FAT', by='one', files=['DZL V4 III Gain FAT CAB FREE'], **h)
    _v[f'2|1|{b}'] = dict(name='Physics', by='one', files=[DZ2('CH3', 'Physics')], rig=True, alt=DZM('CH3', 'Physics'), **h)
    _v[f'2|2|{b}'] = dict(name='Potty Time', by='one', files=[DZ2('CH3', 'Potty Time')], rig=True, alt=DZM('CH3', 'Potty Time'), **h)
    _v[f'3|1|{b}'] = dict(name='Lead the Way', by='one', files=[DZ2('CH4', 'Lead the Way')], rig=True, alt=DZM('CH4', 'Lead the Way'), **h)
_v['3|0|0'] = dict(name='Rhythm', by='gain', files=[DB(f'CH 4 Rhythm {i}') for i in (1, 2, 3)])
_v['3|0|1'] = dict(name='Rhythm', by='gain', files=[DB(f'CH 4 Rhythm TS808 {i}') for i in (1, 2)])

JC = lambda t: f'Roland JC120_{t}'
DS = lambda t: f'SLAMMIN_DUMBLE_FORD_{t}'
DCL = [('Klean', 'CLN_KLEAN_S'), ('Warm', 'CLN_WARM_S'), ('Balanced', 'CLN_BALANCED_S'), ('Scoop', 'CLN_SCOOP_S'), ('Fordish', 'CLN_FORDISH_S'), ('Carltonish', 'CLN_CARLTONISH_2_S')]
DOD = [('Ford', 'OD_FORD_S'), ('Ford 2', 'OD_FORD_2_S'), ('Carlton', 'OD_CARLTON_4_S'), ('Hermans', 'OD_HERMANS_S'), ('Smooth', 'OD_SMOOTH_S'), ('Open (Larry)', 'OD_OPEN_LARRY_S'),
       ('PAB clear drive', 'OD_PAB_CLEAR_DRIVE_XS'), ('PAB Mick', 'OD_PAB_MICK_XS'), ('PAB crunch', 'OD_PAB_CRUNCH_XS'), ('PAB thick', 'OD_PAB_THICK_XS'), ('PAB high gain', 'OD_PAB_HIGAIN_XS')]
AMPS += [
 dict(key='persephone', skins=['persephone_jc'], name='Persephone', accent='#ff8aa0',
      based='Roland JC-120 Jazz Chorus', pack='Roland JC120 Jazz Chorus', by='autoguitarrig', nfiles=7, kind='Amp only',
      knobs=['Volume', 'Treble', 'Middle', 'Bass', 'Reverb', 'Speed', 'Depth'], gain=0,
      dims=[dict(id='channel', names=['Ch I', 'Ch II'], button='channel', leds=[['ch1'], ['ch2']], perknobs=[0, 1, 2, 3]),
            dict(id='voice', names=['Clean', 'Bright', 'Warm'], button='voice', leds=[['vo1'], ['vo2'], ['vo3']], start=1)],
      switches=[dict(id='chorus', button='chorus', led='chorus_led', label='Chorus', does='the JC stereo chorus, run by the plugin with Speed and Depth')],
      caps={'0|0': dict(by='one', files=[JC('CLEAN')]), '0|1': dict(by='one', files=[JC('BRIGHT')]), '0|2': dict(by='one', files=[JC('WARM')]),
            '1|0': dict(by='one', files=[JC('Stereo_CLEAN')]), '1|1': dict(by='one', files=[JC('Stereo_BRIGHT')]), '1|2': dict(by='one', files=[JC('Stereo_WARM')], alt=['JazzChorus120'])},
      intro='Persephone is the super-clean one: a Roland JC-120, the solid-state Jazz Chorus. It stays glassy and clangy at any volume, which is the piezo/neck-single-coil sparkle you '
            'were after. I used the autoguitarrig pack, the better of your two: it\'s amp only and gives you Clean, Bright and Warm versions, while the other pack is one full-rig capture.',
      notes=['CHANNEL picks Ch I or Ch II (the pack\'s normal and "Stereo" captures). VOICE picks Clean, Bright or Warm. The default is Bright, the most JC-sounding one.',
             'CHORUS switches on the JC\'s famous chorus. A capture can\'t hold a moving effect, so the plugin makes the chorus itself, set by the Speed and Depth knobs. Reverb is the plugin\'s own reverb too.',
             'The JC barely distorts at all, so Volume mostly sets level and only adds a little grit at the top.',
             'Spare: the flaviospanker JC-120 capture (full rig, accuracy 0.010).']),
 dict(key='hyperion', skins=['hyperion_ds'], name='Hyperion', accent='#ffb347',
      based='Dumble Overdrive Special 102 ("Ford")', pack='Dumble ODS 102 Ford Hyper Accuracy', by='slamminmofo', nfiles=83, kind='Amp only', esr='0.0015–0.009 (picked)',
      knobs=['Volume', 'Treble', 'Middle', 'Bass', 'Presence', 'Drive', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Clean', 'Overdrive'], button='channel', leds=[['ch1'], ['ch2']], perknobs=[0, 1, 2, 3, 6], start=1),
            dict(id='voice', button='voice', leds=[], per='channel')],
      caps=dict([(f'0|{i}', dict(name=n, by='one', files=[DS(f)])) for i, (n, f) in enumerate(DCL)] +
                [(f'1|{i}', dict(name=n, by='one', knob=5, files=[DS(f)])) for i, (n, f) in enumerate(DOD)]),
      intro='Hyperion is now a Dumble Overdrive Special, the Robben Ford 102. The pack is huge (83 captures, all amp only), so I kept the best: 6 cleans and 11 overdrive voices, '
            'picked for accuracy and so each one sounds different. CHANNEL switches Clean and Overdrive, and VOICE steps through the settings on that channel. The readout shows the name.',
      notes=['Clean voices: Klean, Warm, Balanced, Scoop, Fordish and Carltonish. Volume drives them.',
             'Overdrive voices: Ford, Ford 2, Carlton, Hermans, Smooth, Open (Larry), plus the PAB (pre-amp boost) ones: clear drive, Mick, crunch, thick and high gain. Drive pushes the input on this channel.',
             'Where the pack had two model sizes of the same sound, I took the more accurate XS version.',
             'Left out: the near-duplicate takes (Carlton 2/3, Ford 3–6, Mick 2 and so on), the "very high master" ones, which are the least accurate in the pack, and a few gritty cleans that overlap the overdrive voices. They\'re all still in the zip if you want any added back.'])
]

AMPS += [
 dict(key='hecate', skins=['hecate_evh'], name='Hecate', accent='#b45cff',
      based='EVH 5150', pack='EVH 5150', by='rusen', nfiles=1, kind='Full rig (cab in the capture)', esr='0.015',
      knobs=['Gain', 'Volume', 'Low', 'Mid', 'High', 'Presence', 'Resonance'], gain=0,
      dims=[], caps={'': dict(by='one', files=['EVH 5150'], rig=True)},
      intro='Hecate runs the EVH 5150 capture. It\'s one high-gain sound with the cab built in, so there\'s nothing to switch between: the two old toggles are gone and the '
            'purple lamp is the power switch. The panel is labelled like the 5150: Gain, Volume, Low, Mid, High, Presence, Resonance.',
      notes=['Single capture, so Gain drives the input into it and the rest is the tone stack around it.',
             'Full rig, so the plugin should bypass its own cab while Hecate is loaded.',
             'Moirai already has the full 5150 III Ivory pack with every channel at ten gain levels. If you find an amp-only 5150 pack with several gain levels or the lead channel, Hecate can get channel buttons too.'])
]

ST = lambda c: f'5150 Stealth 100w {c} Mesa OS - jp_is_out_of_tune'
AMPS += [
 dict(key='typhon', skins=['typhon_green', 'typhon_blue', 'typhon_red'], name='Typhon', accent='#e8e8ee', new=True,
      based='EVH 5150 III Stealth 100W', pack='5150 Stealth 100w Mesa OS Full Rig - Blue, Red and Green', by='jpisoutoftune', nfiles=3, kind='Full rig (cab in the capture)',
      knobs=['Gain', 'Volume', 'Low', 'Mid', 'High', 'Presence', 'Resonance'], gain=0,
      dims=[dict(id='channel', names=['Green', 'Blue', 'Red'], buttons=['ch1', 'ch2', 'ch3'], leds=[['ch1_led'], ['ch2_led'], ['ch3_led']], skin=True, perknobs=[0, 1, 2, 3, 4], start=2)],
      caps={'0': dict(by='one', files=[ST('Green SD1')], rig=True), '1': dict(by='one', files=[ST('Blue')], rig=True), '2': dict(by='one', files=[ST('Red')], rig=True)},
      intro='Typhon is new, in the spirit of the 5150 III Stealth: all matte black, with a 3D pyramid grid across the front and glossy black stripes criss-crossing it like tape. '
            'The stripes only show when the light catches them. GREEN, BLUE and RED pick the three channels, and the light strip and power lamp change to that channel\'s colour.',
      notes=['One capture per channel, all through a Mesa Oversized 4x12 (full rig), so the plugin\'s cab is bypassed on this amp.',
             'The green capture was made with an SD-1 in front, so it\'s already pushed. Blue and red are the amp on its own.',
             'Gain drives the input into each capture. Each channel remembers its own Gain, Volume and EQ, like the real amp.']),
 dict(key='tyche', skins=['tyche_pl'], name='Tyche', accent='#d6a84e',
      based='Marshall JMP-50 Lead (1969 Plexi)', pack='Marshall JMP-50 Lead 1969 Plexi A2', by='amalgamaudio', nfiles=1, kind='Full rig (cab in the capture)', esr='0.009',
      knobs=['Gain', 'Treble', 'Middle', 'Bass', 'Presence', 'Volume', 'Master'], gain=0,
      dims=[], caps={'': dict(by='one', files=['MRSH JM50LD I Crunch2 FAT CAB'], rig=True)},
      intro='Tyche is now a 1969 Marshall JMP-50 Plexi: the classic cranked rock crunch. The pack is one capture, so the old Standby switch is gone and the violet lamp is the power switch.',
      notes=['Single crunch capture with the cab built in, so the plugin\'s cab is bypassed while Tyche is loaded.',
             'Gain drives the input. Plexis clean up a lot when you turn down, so this will go from edge-of-breakup to full crunch.',
             'A pack with several Volume I settings (an amp-only one if possible) would let Gain step through real captures, like on Chrysaor.']),
 dict(key='thanatos', skins=['thanatos_slo'], name='Thanatos', accent='#2ee6ff',
      based='Soldano SLO-100 (overdrive channel)', pack='Soldano SLO-100 - Drive', by='amalgamaudio', nfiles=1, kind='Full rig (cab in the capture)',
      knobs=['Preamp', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'], gain=0,
      dims=[], caps={'': dict(by='one', files=['SLDN SL100 NR Drive WRM5 CAB'], rig=True)},
      intro='Thanatos is now a Soldano SLO-100, the overdrive channel: the smooth, singing lead sound. It\'s one capture, so the Standby switch is gone and the cyan lamp is the power switch.',
      notes=['Single capture with the cab built in, so the plugin\'s cab is bypassed while Thanatos is loaded.',
             'Preamp drives the input into the capture, and the rest is the tone stack around it.',
             'The SLO has a clean/crunch channel too. If you find a pack that has it, Thanatos can get a channel button.'])
]

AMPS += [
 dict(key='orpheus', skins=['orpheus'], name='Orpheus', accent='#e2b27a', new=True,
      based='Acoustic simulator', pack='Acoustic Sim', by='gloo', nfiles=2, kind='Capture of an acoustic-sim box (no cab)',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Body', 'Reverb', 'Volume'], gain=0,
      dims=[dict(id='take', names=['Take 1', 'Take 2'], button='take', leds=[['tk1'], ['tk2']])],
      caps={'0': dict(by='one', files=['Acoustic Sim (Take 1)']), '1': dict(by='one', files=['Acoustic Sim (Take 2)'])},
      intro='Orpheus is new, named after the musician of Greek myth. It\'s a clean, modern acoustic-style amp: matte white body and panel, a front of light matte oak slats, '
            'white knobs with bronze caps, and a soft warm-white light. The lamp is the power switch, and TAKE flips between the two captures in the pack.',
      notes=['The pack is two takes of an acoustic-simulator box. They\'re similar, so TAKE works more like an A/B for the one you prefer.',
             'Body and Reverb are the plugin\'s own: Body is a low-mid resonance boost for a woody sound, and Reverb is a room/plate after the capture.',
             'For best results, pair it with no cab, or a flat/acoustic IR in the cab section, and a clean or piezo-like guitar sound in front.'])
]

# ---------- Lamia, Astraea, Eos, Erebus, Dionysus ----------
import glob as _glob, os as _os, re as _re
FB = '[AMP] BE100DLX-'; MICS = ['SM57', 'SM58', 'BLEND']
_LCH = [('CLN', ['Tender Clean', 'Rock #01']), ('BE', ['Man in the Cogs #01', 'Eddie #01', 'TEST']), ('HBE', ['Mammoth', 'Tallica'])]
_lcaps = {f'{c}|{m}': dict(by='gain', rig=True, files=[f'{FB}{cc} {n} - {mic}' for n in ns]) for c, (cc, ns) in enumerate(_LCH) for m, mic in enumerate(MICS)}
_lcaps.update({f'3|{m}': dict(by='one', rig=True, files=[f'[POW] BE100DLX EL34 Juice #28 - {mic}']) for m, mic in enumerate(MICS)})

_HERE = _os.path.dirname(_os.path.abspath(__file__))
_AC = sorted(_os.path.basename(f)[:-4] for f in _glob.glob(_os.path.join(_HERE, '..', '..', 'plugin', 'Content', 'Captures', 'Amps', 'Astraea', '*.nam')))
_rx = _re.compile(r'_(N|TB)_V(\d+)_TC(\d+)(?:_B(\d+)_T(\d+)_[A-Z ]+?)?(?:_DALLASTREBLE_(\d+))?\s*_S$')
_acn, _act, _acb = [], [], []
for f in _AC:
    g = _rx.search(f); ch, v, tc, b, t, dt = g.groups()
    if dt: _acb.append((int(v), int(dt), f))
    elif ch == 'N': _acn.append(([int(v), int(tc)], f))
    else: _act.append(([int(v), int(tc), int(b), int(t)], f))
_acb.sort()
_tb = dict(by='near', axes=[0, 3, 2, 1], w=[1, 0.45, 0.3, 0.3], files=[f for _, f in _act], at=[a for a, _ in _act])
_accaps = {'0|0': dict(by='near', axes=[0, 3], w=[1, 0.45], files=[f for _, f in _acn], at=[a for a, _ in _acn]),
           '1|0': _tb,
           '0|1': dict(by='gain', files=[f for _, _, f in _acb]),
           '1|1': dict(_tb, hide=True, note='the pack has no Top Boost capture with the treble booster, so BOOST does nothing on this channel')}

HW = 'Hiwatt DR 103 '
_DRV = ['Hiwatt DR 103 Archer Klon Pushed', 'Hiwatt DR 103 Archer Klon drive', 'Hiwatt DR 103 TS808_Archer Klon high gain']
_eoscaps = {'0|0': dict(by='gain', files=[HW + 'VOL3', HW + 'VOL5', HW + 'VOL8', HW + 'Crancked']),
            '1|0': dict(by='gain', files=[HW + 'clean bright', HW + 'VOL3 Bright', 'Hiwatt DR103 Vol8 Bright']),
            '2|0': dict(by='gain', files=[HW + 'Edgy Bridged', HW + 'crunch Bridged', 'Hiwatt DR103 Bridged'])}
_DN = ['Klon pushed', 'Klon drive', 'TS808 + Klon']
for b in (1, 2, 3):
    for c in (0, 1, 2):
        _eoscaps[f'{c}|{b}'] = dict(by='one', files=[_DRV[b - 1]], label=f'Drive {ROMAN[b - 1]} · {_DN[b - 1]}', hide=c > 0,
                                    note='the drive captures were made at one amp setting, so CHANNEL does not change them')
_SOLO = ['OFF', 'STUDIO', 'DSOT', 'PULSE', 'RTN', 'GDANSK', 'POMPEII', 'ROME']
_SF = [None] + [f'Comfortably-Numb-{n}-Dry' for n in ('STUDIO', 'DSOT', 'PULSE', 'RTN', 'GDANSK', 'POMPEI')] + ['Comfortably-Numb-ROME-Drywav']

AMPS += [
 dict(key='lamia', skins=['lamia_be'], name='Lamia', accent='#ff2a20',
      based='Friedman BE-100 Deluxe', pack='Friedman BE100 Deluxe', by='2dor', nfiles=24, kind='Full rig (cab in the capture)',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Resonance', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Clean', 'BE', 'HBE', 'EL34'], buttons=['ch1', 'ch2', 'ch3', 'ch4'], leds=[['ch1'], ['ch2'], ['ch3'], ['ch4']],
                 perknobs=[0, 1, 2, 3, 6], start=1),
            dict(id='mic', names=['SM57', 'SM58', 'Mic blend'], button='mic', leds=[['mic1'], ['mic2'], ['mic3']], start=2)],
      caps=_lcaps,
      intro='Lamia keeps her gothic cathedral look and is now a Friedman BE-100 Deluxe. Four CHANNEL buttons pick Clean, BE and HBE like the real amp, '
            'plus EL34, the one capture the pack marks as [POW] instead of [AMP]. These are full-rig captures, so the MIC button picks how the cab was recorded: '
            'SM57, SM58 or a blend of the two.',
      notes=['Gain steps through the real captures on each channel: Clean goes Tender Clean → Rock, BE goes Man in the Cogs → Eddie → TEST, and HBE goes Mammoth → Tallica. '
             '"TEST" is the name the pack gives it.',
             'Each channel remembers its own Gain, EQ and Master, like separate channels on the real amp.',
             'Full rig: the cab is inside every capture, so the plugin should bypass its own cab section while Lamia is loaded.',
             'Every setting exists with all three mics (24 files), so MIC never runs out of options.']),
 dict(key='astraea', skins=['astraea_ac'], name='Astraea', accent='#e6c070',
      based='Vox AC30 CH', pack='VOX AC30 CH Hyper Accuracy', by='slamminmofo', nfiles=96, kind='Amp only',
      knobs=['Volume', 'Treble', 'Bass', 'Tone Cut', 'Reverb', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Normal', 'Top Boost'], buttons=['ch1', 'ch2'], leds=[['ch1_led'], ['ch2_led']], start=1),
            dict(id='boost', names=['Off', 'Treble booster'], button='boost', leds=[[], ['boost_led']])],
      caps=_accaps,
      intro='Astraea is now a Vox AC30. This is the most detailed pack on the page: the maker captured both channels at many real knob settings. '
            'So instead of blending a gain ladder, Volume, Tone Cut, Bass and Treble find the captured setting closest to where your knobs are and load it, '
            'blending with the next closest one in between. BOOST adds the treble booster captures on the Normal channel.',
      notes=['NORMAL: ' + str(len(_acn)) + ' captures across Volume 3 to 10 and Tone Cut 0 to 10. TOP BOOST: ' + str(len(_act)) + ' captures across Volume 2 to 10 with many Bass, Treble and Tone Cut settings.',
             'Tone Cut works like on a real AC30: turning it up takes treble away.',
             'Treble and Bass only exist on the Top Boost channel of a real AC30, so on Normal they are just the plugin EQ.',
             'BOOST: ' + str(len(_acb)) + ' captures with a Dallas-style treble booster in front, taken at Volume 7 and 10. Volume steps through them from mildest to hottest. The pack has none on Top Boost.',
             'All files are amp only, so your cab section works normally. Reverb and Master are plugin controls.']),
 dict(key='eos', skins=['eos_hw'], name='Eos', accent='#e8b866',
      based='Hiwatt DR103, plus the Comfortably Numb solo tones', pack='Hiwatt Custom 100 DR103 1974 + Pink Floyd Comfortably Numb Tone Collection', by='daweed, frank67',
      nfiles=20, kind='Amp only (Hiwatt) and full rig (solo tones)',
      knobs=['Volume', 'Bass', 'Treble', 'Middle', 'Presence', 'Master', 'Solo'], gain=0, steps={6: _SOLO},
      dims=[dict(id='channel', names=['Normal', 'Brilliant', 'Linked'], buttons=['ch1', 'ch2', 'ch3'], leds=[['ch1_led'], ['ch2_led'], ['ch3_led']], perknobs=[0]),
            dict(id='boost', names=['Off', 'Klon pushed', 'Klon drive', 'TS808 + Klon'], button='drive', leds=[[], ['dr1'], ['dr2'], ['dr3']])],
      caps=_eoscaps,
      sel=dict(knob=6, label='Solo', names=_SOLO, files=_SF, rig=True, note='Comfortably Numb solo tone; channel and drive are bypassed'),
      intro='Eos got a full makeover and two packs mixed into one head. Underneath is the Hiwatt DR103: CHANNEL picks Normal, Brilliant or Linked '
            '(both inputs jumped, the classic Hiwatt trick), and Volume steps through the real captures on each one. DRIVE adds the pack\'s pedal captures. '
            'The ivory SOLO selector is the second pack: turn it away from OFF and it clicks through seven Comfortably Numb solo tones '
            '(Studio, DSOT, Pulse, RTN, Gdansk, Pompeii, Rome), which take over until you turn it back to OFF.',
      notes=['New look: a thick ivory faceplate with rounded edges, gold trim rails, round black lacquer transformer cans with gold bands, gold tube collars and feet, '
             'a gold sunrise emblem and gold channel buttons.',
             'Hiwatt (13 files, amp only): Normal goes Vol 3 → 5 → 8 → cranked, Brilliant goes clean → Vol 3 → Vol 8, Linked goes edgy → crunch → full. Each channel keeps its own Volume.',
             'DRIVE (3 LEDs): I is an Archer Klon pushing the amp, II is the Klon as a drive, III is a TS808 stacked into the Klon.',
             'The solo tones (7 files) were captured from a HiTone DG-50 with the cab included, so the plugin should bypass its cab while SOLO is on.']),
 dict(key='erebus', skins=['erebus_uu'], name='Erebus', accent='#a070ff', new=True,
      based='Bogner Überschall MKII Ultra', pack='Bogner Uberschall MKII Ultra High Gain', by='amalgamaudio', nfiles=1, kind='Full rig (cab in the capture)',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'], gain=0,
      dims=[], caps={'': dict(by='one', rig=True, files=['BOG UU II Gain BAL CAB'])},
      intro='Erebus is Nyx\'s premium big brother, named after Nyx\'s partner in the myths. It keeps the black slot grille and coloured tubes, '
            'but goes upmarket: gloss black-chrome, chrome corners, pointed hex slots, a floating black-chrome EREBUS badge, a violet light line, a chrome lip '
            'and knobs with glowing violet pointers.',
      notes=['One full-rig capture of the MKII Ultra, so there is no channel button. Gain pushes the input harder into it.',
             'Full rig: the cab is inside the capture, so the plugin should bypass its own cab section.',
             'If you find more MKII Ultra captures (another channel or gain setting), send them and Erebus gets channel buttons.']),
 dict(key='dionysus', skins=['dionysus'], name='Dionysus', accent='#ff4d6a', new=True,
      based='Bogner Ecstasy 101B', pack='Bogner Ecstasy 101B Archive', by='Kenji Suzuki', nfiles=8, kind='Full rig (cab in the capture)',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Depth', 'Master'], gain=0,
      dims=[dict(id='channel', names=['Green · clean', 'Blue · crunch', 'Red · lead'], buttons=['ch1', 'ch2', 'ch3'], leds=[['ch1_led'], ['ch2_led'], ['ch3_led']],
                 perknobs=[0, 1, 2, 3, 6], start=2),
            dict(id='voice', button='voice', leds=[['vo1'], ['vo2'], ['vo3']], per='channel')],
      caps={'0|0': dict(name='Normal', by='one', rig=True, files=['SC-XTC-101B-Green-Normal']),
            '0|1': dict(name='Bright', by='one', rig=True, files=['SC-XTC-101B-Green-Bright']),
            '0|2': dict(name='Warm', by='one', rig=True, files=['SC-XTC-101B-Green-Warm']),
            '1|0': dict(name='Plain', by='one', rig=True, files=['Ecstasy 101B Blue']),
            '1|1': dict(name='Protein OD', by='one', rig=True, files=['Ecstasy Blue #U2014 Protein OD']),
            '1|2': dict(name='Fat + Protein OD', by='one', rig=True, files=['Ecstasy Blue Fat #U2014 Protein OD']),
            '2|0': dict(name='711 (br)', by='one', rig=True, files=['Bog_HG_Red_711_br']),
            '2|1': dict(name='711 retake', by='one', rig=True, files=['Bog_HG_Red_711_retake'])},
      intro='Dionysus, the god of ecstasy, is the Bogner Ecstasy 101B. Oxblood leather and a sculpted bronze frieze: a grapevine with domed grape clusters, '
            'veined leaves and curling tendrils grows out of a beaded DIONYSUS cartouche over a pierced diamond trellis, with the tubes glowing warm through it. '
            'Like the real Ecstasy it has green, blue and red channel buttons, and VOICE steps through the versions of each channel in the pack.',
      notes=['Green: Normal, Bright and Warm. Blue: plain, with a Protein overdrive in front, and Fat with the Protein. Red: two takes of the same 711 lead setting.',
             'One capture per setting, so Gain pushes the input harder into it. Each channel remembers its own Gain, EQ and Master.',
             'All 8 are full rig: the cab is inside the capture, so the plugin should bypass its own cab section.']),
]

AMPS += [
 dict(key='eidolon', skins=['eidolon_fin'], name='Eidolon', accent='#6ecdff', loader=True,
      based='Loads your own NAM captures', pack='Any .nam file you load', by='you', nfiles='Any', kind='Amp only or full rig',
      knobs=['Gain', 'Bass', 'Middle', 'Treble', 'Presence', 'Master', 'Browse'], gain=0,
      dims=[], caps={'': dict(by='loader', files=[])},
      intro='Eidolon is the NAM loader: a rack-style head with no capture of its own, for any capture you want to bring in. The screen stays simple and never moves around. '
            'The top half shows the slot number, the capture\'s name, the gear it was made from and whether it\'s amp only or full rig. '
            'The bottom row shows each knob\'s value, sitting right above that knob, and lights up the one you\'re turning.',
      notes=['LOAD opens one or more .nam files from your phone or PC. The screen reads the name and gear info straight out of each file.',
             'PREV and NEXT step back or forward one capture at a time. BROWSE is the fast way through a big list: turning it sweeps across all loaded captures, so a full turn goes from the first file to the last.',
             'Gain, Bass, Middle, Treble, Presence and Master are the same tone stack every amp uses. In the plugin, Gain drives the input into whatever capture is loaded.',
             'Files you open stay on your device. The page only reads the text inside them to show the name and gear info.'])
]
