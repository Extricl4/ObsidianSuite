"""Which IR cab each preset plays through (2026-11-26: the presets moved from the built-in cabs to the IR cab pack).

Only amps that need a cab get one: an amp-pack capture of the amp alone, or one of the plugin's own amps. A full-rig
capture has its cab (and mic) in the capture already, so the cab stays out of the way and nothing is set here.

The order a preset's cab is decided in (make_presets.py, cab_for):
    1. the preset names one itself: cab_=ir('atlas_cab', ...)
    2. PRESET[preset name]
    3. ARTIST[artist][amp key], then ARTIST[artist]['*']
    4. AMP[amp key]
Each entry is (cab key, voice name or None). A voice is one of the cab's voice buttons (ALBUM, MIC, CAB, SPEAKER):
    Metallon (album IRs)      KILL 'EM ALL ... HARDWIRED, JC CLEAN
    Brontes (1960AV, V30)     EV MIX (bright, tight) | R121 (darker, rounder)
    Steropes (JCM900 1960)    SM57 + E609 | BLENDED
    Epimetheus (Mesa)         HALF BACK · V70 | RECTO · V30
    Phantasos (Bogner 4x12)   412ST · V30 | 412STH · G12M | 412STU · G12T-75
    Deimos (ENGL)             412GS · CN V30 | 412VG · UK V30
    Asteria (Vox 2x12)        ALNICO GOLD | ALNICO BLUE | GOLD + BLUE
    ZETA cabs (Hypnos, Morpheus, Phobetor, Aetna, Lemnos, Prometheus, Hesperia)
                              POS 1 (near the cap, brighter) | POS 2 (further out, darker) | SPKR 1 | SPKR 2
"""

# the amp's own family first: a Mesa into a Mesa cab, a Marshall into a Marshall cab...
AMP = {
    'talos':      ('atlas_cab', None),                      # Dual Rectifier -> Recto OS slant (V30)
    'achlys':     ('epimetheus_cab', 'HALF BACK · V70'),    # Mark IV -> Mesa half back
    'citadel':    ('prometheus_cab', 'POS 1'),              # Mark IIC+ -> Mesa 4x12
    'umbra':      ('prometheus_cab', 'POS 1'),              # Mark V
    'eris':       ('epimetheus_cab', 'RECTO · V30'),        # JP-2C -> the Recto 4x12s Petrucci plays
    'kratos':     ('atlas_cab', None),                      # 6505+ -> Mesa OS, the usual metalcore pairing
    'moirai':     ('deimos_cab', '412VG · UK V30'),         # 5150 III 50W
    'chrysaor':   ('steropes_cab', 'SM57 + E609'),          # JCM800 -> Marshall 1960 straight
    'vulcan':     ('aetna_cab', 'POS 1'),                   # VH4 -> Diezel 4x12
    'nyx':        ('phantasos_cab', '412ST · V30'),         # Überschall -> Bogner 4x12
    'strix':      ('pallas_cab', None),                     # Hellwin -> tight UK V30 4x12
    'moros':      ('daedalus_cab', None),                   # Granophyre -> the modern stealth cab
    'niflheim':   ('deimos_cab', '412GS · CN V30'),         # Revv -> ENGL-style V30 4x12
    'astraea':    ('asteria_cab', 'ALNICO BLUE'),           # AC30 -> Vox 2x12 with Alnico Blues
    'persephone': ('metallon_cab', 'JC CLEAN'),             # JC-120 -> a real Roland JC IR
    'hyperion':   ('morpheus_cab', 'POS 1'),                # Dumble -> open, round 2x12
    'eos':        ('polyphemus_cab', None),                 # Hiwatt -> tall vintage 4x12
    # the plugin's own amps
    'glass65':    ('hypnos_cab', 'POS 1'),
    'top30':      ('asteria_cab', 'ALNICO BLUE'),
    'plexi68':    ('arges_cab', None),
    'brit800':    ('steropes_cab', 'SM57 + E609'),
    'recto':      ('atlas_cab', None),
    'core':       ('daedalus_cab', None),
    'slo100':     ('brontes_cab', 'EV MIX'),
    'vh4':        ('lemnos_cab', 'POS 1'),
    'styx51':     ('atlas_cab', None),
}

ARTIST = {
    'Metallica':            {'*': ('metallon_cab', None)},            # album voices: PRESET below
    'TesseracT':            {'*': ('daedalus_cab', None)},            # Emil's TesseracT IR
    'Periphery':            {'*': ('daedalus_cab', None)},
    'Animals as Leaders':   {'*': ('daedalus_cab', None)},
    'Architects':           {'*': ('daedalus_cab', None)},
    'Spiritbox':            {'*': ('daedalus_cab', None)},
    'Sleep Token':          {'umbra': ('prometheus_cab', 'POS 2')},
    'Dream Theater':        {'*': ('epimetheus_cab', 'RECTO · V30')},
    'Avenged Sevenfold':    {'nyx': ('phantasos_cab', '412ST · V30'), 'strix': ('pallas_cab', None), 'kratos': ('atlas_cab', None),
                             'talos': ('atlas_cab', None), 'umbra': ('prometheus_cab', 'POS 1'), 'moirai': ('deimos_cab', '412VG · UK V30'),
                             'vulcan': ('lemnos_cab', 'POS 1'), 'achlys': ('epimetheus_cab', 'RECTO · V30')},
    # Marshall rock: greenback-ish 1960s for the 70s/80s classics, the V30 slant for heavier records
    "Guns N' Roses":        {'chrysaor': ('brontes_cab', 'EV MIX')},
    'Whitesnake':           {'chrysaor': ('brontes_cab', 'EV MIX')},
    'Iron Maiden':          {'chrysaor': ('polyphemus_cab', None)},
    'Judas Priest':         {'chrysaor': ('steropes_cab', 'SM57 + E609')},
    'Slayer':               {'chrysaor': ('steropes_cab', 'SM57 + E609')},
    'Megadeth':             {'chrysaor': ('steropes_cab', 'BLENDED')},
    'Alice in Chains':      {'chrysaor': ('brontes_cab', 'R121')},
    'Opeth':                {'chrysaor': ('brontes_cab', 'R121')},
    'Joe Satriani':         {'chrysaor': ('brontes_cab', 'EV MIX'), 'persephone': ('hypnos_cab', 'POS 1')},
    'Dire Straits':         {'chrysaor': ('arges_cab', None), 'glass65': ('hypnos_cab', 'POS 1')},
    'Gary Moore':           {'chrysaor': ('brontes_cab', 'R121')},
    'Ghost':                {'chrysaor': ('gyges_cab', None)},
    'Rage Against the Machine': {'chrysaor': ('brontes_cab', 'EV MIX')},
    'Ozzy Osbourne':        {'chrysaor': ('brontes_cab', 'EV MIX')},
    'Tool':                 {'vulcan': ('aetna_cab', 'POS 2')},
    'Mastodon':             {'moros': ('ladon_cab', None)},              # Orange 4x12
    'Pink Floyd':           {'eos': ('polyphemus_cab', None)},
    'Queen':                {'astraea': ('asteria_cab', 'ALNICO BLUE')},
    'U2':                   {'astraea': ('asteria_cab', 'GOLD + BLUE')},
    'Radiohead':            {'astraea': ('asteria_cab', 'ALNICO GOLD')},
    'The Beatles':          {'astraea': ('asteria_cab', 'ALNICO BLUE')},
    'Red Hot Chili Peppers': {'glass65': ('hypnos_cab', 'POS 1')},
    'John Mayer':           {'hyperion': ('morpheus_cab', 'POS 1'), 'persephone': ('hypnos_cab', 'POS 1')},
    'Santana':              {'hyperion': ('morpheus_cab', 'POS 2'), 'umbra': ('prometheus_cab', 'POS 2')},
    'Trivium':              {'kratos': ('atlas_cab', None), 'moirai': ('deimos_cab', '412VG · UK V30')},
    'Bullet for My Valentine': {'kratos': ('atlas_cab', None)},
    'Killswitch Engage':    {'kratos': ('deimos_cab', '412GS · CN V30'), 'talos': ('atlas_cab', None)},
    'Polyphia':             {'persephone': ('hypnos_cab', 'POS 1')},
    'Plini':                {'persephone': ('hypnos_cab', 'POS 1')},
    'Pantera':              {'persephone': ('hypnos_cab', 'POS 1')},
    'Bring Me the Horizon': {'talos': ('atlas_cab', None), 'persephone': ('hypnos_cab', 'POS 1')},
}

# song by song: Metallica's album IRs, and a few that want something else than their artist's default
PRESET = {
    'Hit the Lights':                ('metallon_cab', "KILL 'EM ALL"),
    'Fade to Black (Clean)':         ('metallon_cab', 'JC CLEAN'),
    'Fade to Black (Solo)':          ('metallon_cab', 'RIDE THE LIGHTNING'),
    'Ride the Lightning':            ('metallon_cab', 'RIDE THE LIGHTNING'),
    'Master of Puppets':             ('metallon_cab', 'MASTER OF PUPPETS'),
    'Battery':                       ('metallon_cab', 'MASTER OF PUPPETS · BATTERY'),
    'Kirk Wah Solo':                 ('metallon_cab', 'MASTER OF PUPPETS'),
    'Blackened':                     ('metallon_cab', '...AND JUSTICE FOR ALL'),
    'One (Clean Intro)':             ('metallon_cab', 'JC CLEAN'),
    'One (Machine Gun)':             ('metallon_cab', '...AND JUSTICE FOR ALL'),
    'Enter Sandman':                 ('metallon_cab', 'BLACK ALBUM'),
    'Sad but True':                  ('metallon_cab', 'BLACK ALBUM · SAD BUT TRUE'),
    'The Unforgiven (Solo)':         ('metallon_cab', 'BLACK ALBUM'),
    'Nothing Else Matters':          ('morpheus_cab', 'POS 1'),
    'Until It Sleeps':               ('metallon_cab', 'LOAD / RELOAD'),
    'St. Anger':                     ('metallon_cab', 'ST. ANGER'),
    'The Day That Never Comes':      ('metallon_cab', 'HARDWIRED'),
    'Hardwired':                     ('metallon_cab', 'HARDWIRED'),
    'Lux Aeterna':                   ('metallon_cab', 'HARDWIRED'),
    # Synyster's Hellwin era and the Bogner records
    'City of Evil (Album Rhythm)':   ('phantasos_cab', '412ST · V30'),
    'City of Evil (Album Lead)':     ('phantasos_cab', '412ST · V30'),
    'Seize the Day (Clean)':         ('hypnos_cab', 'POS 1'),
    'Angels':                        ('hypnos_cab', 'POS 1'),
    'Exist':                         ('hypnos_cab', 'POS 1'),
    'Cosmic':                        ('hypnos_cab', 'POS 1'),
    'Mattel':                        ('lemnos_cab', 'POS 2'),
    # cleans through a 2x12 rather than a 4x12
    'Shogun (Clean)':                ('hypnos_cab', 'POS 1'),
    'Tosin Clean':                   ('hypnos_cab', 'POS 1'),
    'Petrucci Clean':                ('hypnos_cab', 'POS 1'),
    'Comfortably Numb (Rhythm)':     ('arges_cab', None),
    'Another Brick in the Wall':     ('arges_cab', None),
    'Run Like Hell':                 ('arges_cab', None),
    'Cemetery Gates (Clean)':        ('hypnos_cab', 'POS 1'),
}
