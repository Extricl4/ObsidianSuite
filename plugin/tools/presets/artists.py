"""The Artists folder: song and album tones, one sub-folder per artist.

These are our takes on the kind of sound each record has, built from the plugin's amps (mostly the
amp pack, each one based on a real amp), pedals, effects and synth. They are not copies of anyone's
rig settings. Tunings are named in the note where a song isn't in standard E. Pickups matter a lot:
most rhythm sounds here assume a bridge humbucker, cleans a neck or middle single coil."""


def make(m):
    P, A, M, fx, S, cab = m.P, m.A, m.M, m.fx, m.S, m.cab

    def artist(name):
        return lambda song, typ, note, amp, *rest, **kw: P(song, 'Artists/' + name, typ, note, amp, *rest, **kw)

    # effect shortcuts (real units: ms, %, s)
    def dly(t=420, fb=25, mix=18, mode='Digital', tone=5000, mod=0): return fx('lethe', Time=t, Feedback=fb, Mix=mix, Mode=mode, Tone=tone, Mod=mod)
    def sdly(div='1/8.', fb=30, mix=20, mode='Ping-Pong', tone=5000): return fx('lethe', Sync=1, Division=div, Feedback=fb, Mix=mix, Mode=mode, Tone=tone)
    def tape(t=380, fb=22, mix=16): return fx('lethe', Time=t, Feedback=fb, Mix=mix, Mode='Tape', Tone=4200, Mod=20)
    def verb(decay=1.8, mix=14, voice='CRYPT', size=55): return fx('crypt', Decay=decay, Size=size, Mix=mix, Voice=voice)
    def hall(decay=3.2, mix=22): return fx('crypt', Decay=decay, Size=80, Mix=mix, Voice='HALL')
    def room(mix=12): return fx('crypt', Decay=0.9, Size=35, Mix=mix, Voice='ROOM')
    def spring(mix=18): return fx('crypt', Decay=2.2, Mix=mix, Voice='SPRING')
    def plate(decay=2.2, mix=16): return fx('crypt', Decay=decay, Mix=mix, Voice='PLATE')
    def chorus(rate=0.8, depth=40, mix=35, voices='2'): return fx('tide', Rate=rate, Depth=depth, Mix=mix, Voices=voices)
    def comp(th=-26, ratio=4, makeup=8, blend=75, att=10): return fx('compressor', Threshold=th, Ratio=ratio, Attack=att, Release=160, Makeup=makeup, Blend=blend)
    def ts(cap='II', drive=0, level=4): return fx('graven', Capture=cap, Drive=drive, Level=level, Mix=100)
    def sd1(voice='D II', drive=-4, level=4): return fx('helios', Voice=voice, Drive=drive, Level=level, Mix=100)
    def klon(drive=0, blend=70, out=4, cap='MID'): return fx('chiron', Drive=drive, Blend=blend, Output=out, Capture=cap)
    def fuzz(f=7, tone=5.5, level=-2, gate=1): return fx('ember', Fuzz=f, Tone=tone, Level=level, Gate=gate)
    def boost(g=8, tight=120, bass=0, treble=0): return fx('boost', Gain=g, Tight=tight, Bass=bass, Treble=treble)
    def wah(mode='Envelope', sens=6.5, rng=7, q=7, pos=50): return fx('lament', Mode=mode, Position=pos, Sensitivity=sens, Range=rng, Q=q, Mix=100)
    def vibe(rate=2.2, depth=70, mode='Uni-Vibe'): return fx('spectre', Mode=mode, Rate=rate, Depth=depth, Mix=100)
    def phaser(rate=0.6, depth=60, fb=30, stages='4', mix=55): return fx('prism', Rate=rate, Depth=depth, Feedback=fb, Stages=stages, Mix=mix)
    def flanger(rate=0.25, depth=60, fb=50, manual=1.6, mix=50): return fx('vortex', Rate=rate, Depth=depth, Feedback=fb, Manual=manual, Mix=mix)
    def whammy(shift=12, mix=100, pos=100): return fx('ascend', Shift=shift, Position=pos, Mix=mix)
    def trem(rate=5, depth=55, shape='Sine'): return fx('pulse', Rate=rate, Depth=depth, Shape=shape)
    def octave(up=0, down=0, down2=0, dry=100, tone=6): return fx('abyss', Dry=dry, Oct_1=down, Oct_2=down2, Oct_up=up, Tone=tone)
    def shimmer(decay=6, sh=40, mix=30): return fx('seraph', Decay=decay, Size=85, Shimmer=sh, Mix=mix)
    def swell(attack=450, rev=40): return fx('aurora', Attack=attack, Sensitivity=6, Reverb=rev, Decay=5, Mix=100)
    SCOOP_JUSTICE = [-4, -3, -1, -5, -4, 0, 2.5, 1, -1]
    FOURx12, TWOx12, ONEx12 = cab('4x12'), cab('2x12'), cab('1x12')

    # =====================================================================================
    a = artist('Metallica')
    a("Hit the Lights", 'Rhythm', "Kill 'Em All (1983) · a boosted Marshall through the Kill 'Em All IR: raw, mid-heavy, fast", A('chrysaor', Gain=8, Bass=5, Middle=7, Treble=6.5, Presence=6.5, Master=5.5),
      FOURx12, pre=[boost(10, 140, 0, 2)], eq='thrash', gate=-56, doubler=6)
    a("Fade to Black (Clean)", 'Clean', "Ride the Lightning (1984) · the clean arpeggios: a Roland JC with its own chorus, through a real JC IR", A('persephone', channel='Ch II', voice='Clean', Volume=4.5, Treble=6, Middle=5, Bass=5, Reverb=0, switches=['chorus'], Speed=2.5, Depth=4),
      pre=[comp(-28, 3, 6)], post=[dly(460, 20, 12), hall(2.8, 18)])
    a("Fade to Black (Solo)", 'Lead', "Ride the Lightning (1984) · the long, mournful outro lead", A('chrysaor', Gain=8, Bass=4.5, Middle=7, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[sd1('D I', -6, 5)], eq='lead', gate=-60, post=[dly(480, 30, 20), plate(2.4, 16)])
    a("Ride the Lightning", 'Rhythm', "Ride the Lightning (1984) · boosted JCM800, tighter and darker than Kill 'Em All", A('chrysaor', Gain=8.5, Bass=6, Middle=5, Treble=6.5, Presence=6.5, Master=5),
      FOURx12, pre=[sd1('D II', -4, 5)], eq='thrash', gate=-54, doubler=6)
    a("Master of Puppets", 'Rhythm', "Master of Puppets (1986) · Mark IIC+ rhythm: scooped, tight, galloping", A('citadel', lead='Lead', Volume=7, Treble=7, Bass=3, Middle=4, Presence=6, Lead_drive=6.5, Master=5),
      FOURx12, pre=[ts('II', -3, 3)], eq='scoop', gate=-54, doubler=6, width=120)
    a("Battery", 'Rhythm', "Master of Puppets (1986) · the fast downpicked thrash riff", A('citadel', lead='Lead', Volume=7.5, Treble=7, Bass=3.5, Middle=4.5, Presence=6.5, Lead_drive=7, Master=5),
      FOURx12, eq='thrash', gate=-52, doubler=6, width=120)
    a("Blackened", 'Rhythm', "...And Justice for All (1988) · the famously dry, scooped, bass-less wall", A('citadel', lead='Lead', Volume=8, Treble=7.5, Bass=2.5, Middle=3, Presence=7, Lead_drive=7.5, Master=5),
      FOURx12, pre=[ts('II', -2, 3)], eq=SCOOP_JUSTICE, gate=-50, doubler=5, width=125)
    a("One (Clean Intro)", 'Clean', "...And Justice for All (1988) · the clean intro: a Roland JC's chorus through a real JC IR, a soft hall", A('persephone', channel='Ch II', voice='Clean', Volume=4.5, Treble=6, Middle=5, Bass=4.5, Reverb=0, switches=['chorus'], Speed=2, Depth=5),
      pre=[comp(-28, 3, 6)], post=[hall(2.5, 20)], width=120)
    a("One (Machine Gun)", 'Rhythm', "...And Justice for All (1988) · the double-kick machine-gun section", A('citadel', lead='Lead', Volume=8, Treble=7, Bass=3, Middle=3.5, Presence=6.5, Lead_drive=7.5, Master=5),
      FOURx12, eq=SCOOP_JUSTICE, gate=-48, doubler=5)
    a("Enter Sandman", 'Rhythm', "Metallica / Black Album (1991) · Mark IV with the graphic EQ: huge, polished, heavy", A('achlys', channel='Lead', voice='Lead midgain + EQ', Gain=6.5, Treble=6.5, Bass=5, Middle=5, Presence=6, Lead_drive=6.5, Master=5),
      FOURx12, eq='scoop', gate=-52, doubler=7, width=120)
    a("Sad but True", 'Rhythm', "Black Album (1991) · tuned to D, slow and crushing", A('achlys', channel='Lead', voice='Lead harmonics + EQ', Gain=7, Treble=6, Bass=6, Middle=5, Presence=6, Lead_drive=6.5, Master=5),
      FOURx12, eq=[1, 2, 0, -3, -2, 0, 1.5, 0, -1.5], gate=-50, doubler=8, width=125)
    a("Nothing Else Matters", 'Clean', "Black Album (1991) · the clean arpeggio intro", A('hyperion', channel='Clean', voice='Klean', Volume=4.5, Treble=6, Middle=5, Bass=5, Presence=5.5, Master=5),
      ONEx12, pre=[comp(-26, 3, 5)], post=[chorus(0.4, 25, 25), hall(2.6, 20)])
    a("The Unforgiven (Solo)", 'Lead', "Black Album (1991) · a warm, singing wah-less lead", A('achlys', channel='Lead', voice='Lead midgain', Gain=6.5, Treble=6, Bass=4.5, Middle=6.5, Presence=5.5, Lead_drive=7, Master=5),
      FOURx12, eq='lead', gate=-58, post=[dly(440, 28, 20), plate(2.2, 16)])
    a("Until It Sleeps", 'Rhythm', "Load (1996) · E flat tuning, looser, rawer, more mids", A('talos', Gain=5.5, Treble=6, Middle=6.5, Bass=5.5, Presence=5.5, Master=5, Output=5),
      FOURx12, eq='mid', gate=-56, doubler=7)
    a("St. Anger", 'Rhythm', "St. Anger (2003) · drop C, no solos, dry and metallic", A('talos', boost='TS808 boost', Gain=7, Treble=6.5, Middle=5.5, Bass=6, Presence=6.5, Master=5, Output=5),
      FOURx12, eq=[0, 1.5, 0, -1, 1, 2, 1.5, 0, -2], gate=-48, doubler=5)
    a("The Day That Never Comes", 'Rhythm', "Death Magnetic (2008) · Diezel-style modern Metallica rhythm", A('vulcan', channel='Ch 3 · mega', voice='III Gain FAT', Gain=6, Low=5.5, Mid=5.5, High=6, Presence=6, Depth=5.5, Master=5),
      FOURx12, eq='tight', gate=-50, doubler=7)
    a("Hardwired", 'Rhythm', "Hardwired... to Self-Destruct (2016) · Mark V, IIC+ mode", A('umbra', channel='Channel 3', mode='Mark IIC+', Gain=6.5, Treble=6.5, Middle=4.5, Bass=4.5, Presence=6, Master=5, Output=5),
      eq='scoop', gate=-50, doubler=7, width=120)
    a("Lux Aeterna", 'Rhythm', "72 Seasons (2023) · Mark V, Mark IV mode: punchy and bright", A('umbra', channel='Channel 3', mode='Mark IV', Gain=6.5, Treble=6.5, Middle=5, Bass=5, Presence=6, Master=5, Output=5),
      pre=[ts('II', -3, 3)], eq='tight', gate=-50, doubler=7)
    a("Kirk Wah Solo", 'Lead', "Kirk-style solo: Mark IIC+ lead with the wah following your picking", A('citadel', lead='Lead', Volume=7, Treble=6.5, Bass=3.5, Middle=6.5, Presence=6, Lead_drive=7.5, Master=5),
      FOURx12, pre=[wah('Envelope', 6.5, 7.5, 7.5)], gate=-58, post=[dly(420, 28, 18), plate(2.2, 14)])

    # =====================================================================================
    a = artist('Avenged Sevenfold')
    # ---- Sounding the Seventh Trumpet (2001)
    a("Sounding the Seventh Trumpet (Album Rhythm)", 'Rhythm', "Sounding the Seventh Trumpet (2001) · the raw debut: fizzy, scooped, hardcore-flavoured metalcore", A('talos', Gain=7.5, Treble=6.5, Middle=3.5, Bass=6, Presence=6.5, Master=5, Output=5),
      eq='scoop', gate=-48, doubler=6, width=115)
    a("Sounding the Seventh Trumpet (Album Lead)", 'Lead', "Sounding the Seventh Trumpet (2001) · thin, biting melodic leads with a short echo", A('talos', boost='TS808 boost', Gain=6.5, Treble=6.5, Middle=6, Bass=5, Presence=6, Master=5, Output=5),
      eq='lead', gate=-56, post=[dly(380, 20, 15), room(12)])
    # ---- Waking the Fallen (2003)
    a("Waking the Fallen (Album Rhythm)", 'Rhythm', "Waking the Fallen (2003) · boosted, aggressive metalcore chug: tight lows, cutting top", A('kratos', channel='Lead', Lead_pre=6.5, Lead_post=5, Low=5.5, Mid=5, High=6.5, Resonance=6, Presence=6.5),
      FOURx12, pre=[ts('II', -3, 3)], eq='tight', gate=-48, doubler=7, width=120)
    a("Waking the Fallen (Album Lead)", 'Lead', "Waking the Fallen (2003) · the twin-lead tone: mids forward, eighth-note ping-pong", A('talos', boost='TS808 boost', Gain=6, Treble=6, Middle=6.5, Bass=4.5, Presence=6, Master=5, Output=5),
      eq='lead', gate=-58, post=[sdly('1/8', 22, 18), plate(2.2, 14)])
    a("Unholy Confessions", 'Rhythm', "Waking the Fallen (2003) · tight Rectifier metalcore rhythm", A('talos', boost='TS808 boost', Gain=6.5, Treble=6, Middle=4.5, Bass=6, Presence=6, Master=5, Output=5),
      eq='tight', gate=-50, doubler=7, width=120)
    a("Chapter Four", 'Rhythm', "Waking the Fallen (2003) · the stomping, galloping opener", A('talos', boost='TS808 boost', Gain=7, Treble=6, Middle=5, Bass=6, Presence=6.5, Master=5, Output=5),
      eq='thrash', gate=-48, doubler=7, width=120)
    a("Second Heartbeat (Solo)", 'Lead', "Waking the Fallen (2003) · the long melodic harmony solo", A('dionysus', channel='Red · lead', voice='711 (br)', Gain=6, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Depth=5, Master=5),
      eq='lead', gate=-58, post=[dly(440, 28, 20), plate(2.4, 16)])
    # ---- City of Evil (2005)
    a("City of Evil (Album Rhythm)", 'Rhythm', "City of Evil (2005) · a huge, saturated wall with a tight bottom, double-tracked", A('nyx', channel='Lead', voice='Darkness', Gain=6.5, Bass=5.5, Middle=5, Treble=6.5, Presence=6, Depth=5.5, Master=5),
      pre=[ts('II', -3, 3)], eq='modern', gate=-50, doubler=8, width=125)
    a("City of Evil (Album Lead)", 'Lead', "City of Evil (2005) · fast, neo-classical melodic leads with a ping-pong echo", A('nyx', channel='Lead', voice='Anthem', Gain=6.5, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Depth=5, Master=5),
      eq='lead', gate=-58, post=[sdly('1/8.', 25, 18), plate(2.2, 14)])
    a("Beast and the Harlot", 'Rhythm', "City of Evil (2005) · galloping, saturated and huge", A('erebus', Gain=6.5, Bass=5.5, Middle=5.5, Treble=6, Presence=6, Depth=5.5, Master=5),
      FOURx12, eq='tight', gate=-50, doubler=7)
    a("Bat Country", 'Rhythm', "City of Evil (2005) · big Uberschall rhythm", A('nyx', channel='Lead', voice='Basher', Gain=6, Bass=5.5, Middle=5, Treble=6, Presence=6, Depth=5.5, Master=5),
      eq='modern', gate=-52, doubler=7, width=120)
    a("Blinded in Chains", 'Rhythm', "City of Evil (2005) · fast, thrashy riffing with a bright edge", A('nyx', channel='Lead', voice='Striptaser', Gain=6.5, Bass=5, Middle=5, Treble=6.5, Presence=6.5, Depth=5, Master=5),
      eq='thrash', gate=-48, doubler=7, width=120)
    a("Seize the Day (Clean)", 'Clean', "City of Evil (2005) · the clean ballad picking with chorus", A('strix', channel='Clean', voice='Clean USA', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Resonance=5, Master=5),
      pre=[comp(-26, 3, 5)], post=[chorus(0.5, 35, 30), dly(450, 25, 14), hall(2.8, 18)])
    a("Seize the Day (Solo)", 'Lead', "City of Evil (2005) · the emotional ballad solo", A('strix', channel='Lead', voice='Lead 2', Gain=5.5, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      eq='lead', gate=-60, post=[dly(480, 32, 22), hall(3, 18)])
    a("Sidewinder (Solo)", 'Lead', "City of Evil (2005) · the bluesy, wah-tinged epic solo", A('strix', channel='Lead', voice='Lead 2', Gain=6, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      pre=[wah('Envelope', 5.5, 6.5, 6.5)], eq='lead', gate=-58, post=[dly(460, 28, 20), plate(2.4, 16)])
    # ---- Avenged Sevenfold (2007)
    a("Self-Titled (Album Rhythm)", 'Rhythm', "Avenged Sevenfold (2007) · glossy, mid-forward hard-rock-meets-metal rhythm, double-tracked", A('nyx', channel='Lead', voice='Glazed', Gain=5.5, Bass=5, Middle=6, Treble=6, Presence=6, Depth=5, Master=5),
      eq='modern', gate=-52, doubler=8, width=125)
    a("Self-Titled (Album Lead)", 'Lead', "Avenged Sevenfold (2007) · smooth, singing lead with a dotted-eighth echo", A('nyx', channel='Lead', voice='Holly Rocks', Gain=6.5, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Depth=5, Master=5),
      eq='lead', gate=-58, post=[sdly('1/8.', 28, 20), plate(2.4, 16)])
    a("Critical Acclaim", 'Rhythm', "Avenged Sevenfold (2007) · the heavy, chugging verses", A('erebus', Gain=7, Bass=6, Middle=5, Treble=6, Presence=6, Depth=6, Master=5),
      FOURx12, eq='tight', gate=-50, doubler=7, width=120)
    a("Almost Easy", 'Rhythm', "Avenged Sevenfold (2007) · punchy, driving rock riffs with big mids", A('lamia', channel='HBE', Gain=6.5, Bass=5, Middle=6, Treble=6.5, Presence=6, Resonance=5.5, Master=5),
      FOURx12, eq='mid', gate=-52, doubler=7, width=120)
    a("Afterlife", 'Lead', "Avenged Sevenfold (2007) · soaring lead with dotted-eighth echoes", A('nyx', channel='Lead', voice='Blue Lights', Gain=6.5, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Depth=5, Master=5),
      eq='lead', gate=-58, post=[sdly('1/8.', 30, 20), plate(2.4, 16)])
    a("Gunslinger", 'Acoustic', "Avenged Sevenfold (2007) · the acoustic ballad strumming", A('orpheus', take='Take 1', Gain=4, Bass=4.5, Middle=5, Treble=6, Body=5.5, Reverb=1.5, Volume=5),
      pre=[comp(-24, 3, 5)], post=[room(10)])
    # ---- Nightmare (2010)
    a("Nightmare (Album Rhythm)", 'Rhythm', "Nightmare (2010) · darker and thicker: saturated, heavy low mids", A('typhon', channel='Red', Gain=6, Volume=5, Low=6, Mid=5.5, High=6, Presence=6, Resonance=6.5),
      eq='modern', gate=-50, doubler=8, width=125)
    a("Nightmare (Album Lead)", 'Lead', "Nightmare (2010) · thick, emotional lead with long echoes", A('strix', channel='Lead', voice='Lead 2', Gain=6, Bass=5, Middle=6.5, Treble=5.5, Presence=6, Resonance=5.5, Master=5),
      eq='lead', gate=-58, post=[sdly('1/4', 30, 20), hall(3, 18)])
    a("Nightmare", 'Rhythm', "Nightmare (2010) · dark, heavy and wide", A('erebus', Gain=7, Bass=6, Middle=5, Treble=6, Presence=6, Depth=6, Master=5),
      FOURx12, eq='modern', gate=-50, doubler=8, width=125)
    a("Natural Born Killer", 'Rhythm', "Nightmare (2010) · the fastest, thrashiest song on the album", A('kratos', channel='Lead', Lead_pre=7, Lead_post=5, Low=5, Mid=5.5, High=6.5, Resonance=5.5, Presence=6.5),
      FOURx12, pre=[ts('II', -3, 3)], eq='thrash', gate=-48, doubler=7, width=120)
    a("Buried Alive", 'Rhythm', "Nightmare (2010) · the slow, crushing second half", A('erebus', Gain=7, Bass=6.5, Middle=5.5, Treble=5.5, Presence=5.5, Depth=6.5, Master=5),
      FOURx12, eq='tight', gate=-50, doubler=8, width=125)
    a("Danger Line (Solo)", 'Lead', "Nightmare (2010) · the soaring, emotional solo", A('strix', channel='Lead', voice='Lead 1', Gain=6, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      eq='lead', gate=-58, post=[dly(500, 32, 22), hall(3, 20)])
    a("So Far Away (Solo)", 'Lead', "Nightmare (2010) · the tribute solo: warm, slow, singing", A('strix', channel='Lead', voice='Lead 1', Gain=5.5, Bass=5, Middle=6.5, Treble=5.5, Presence=5.5, Resonance=5, Master=5),
      eq='lead', gate=-60, post=[dly(520, 35, 22, 'Tape'), hall(3.2, 20)])
    # ---- Hail to the King (2013)
    a("Hail to the King (Album Rhythm)", 'Rhythm', "Hail to the King (2013) · classic-metal crunch: less gain, big mids, room to breathe", A('lamia', channel='BE', Gain=6, Bass=5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      FOURx12, eq='mid', gate=-52, doubler=7, width=120)
    a("Hail to the King (Album Lead)", 'Lead', "Hail to the King (2013) · bluesy classic-rock lead, a little wah in the attack", A('chrysaor', Gain=7.5, Bass=4.5, Middle=7, Treble=6, Presence=6, Master=5),
      FOURx12, pre=[sd1('D I', -6, 5), wah('Envelope', 4.5, 5, 6)], eq='lead', gate=-58, post=[dly(440, 25, 18), plate(2, 14)])
    a("Hail to the King", 'Rhythm', "Hail to the King (2013) · classic-metal mids on Syn's own amp", A('strix', channel='Lead', voice='Lead 1', Gain=6, Bass=5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      eq='mid', gate=-52, doubler=7, width=120)
    a("Shepherd of Fire", 'Rhythm', "Hail to the King (2013) · slow, heavy, stomping", A('talos', Gain=6.5, Treble=5.5, Middle=5.5, Bass=6.5, Presence=5.5, Master=5, Output=5),
      eq='tight', gate=-50, doubler=8)
    a("This Means War", 'Rhythm', "Hail to the King (2013) · the slow, crushing stomp", A('achlys', channel='Lead', voice='Lead harmonics + EQ', Gain=7, Treble=6, Bass=6, Middle=5, Presence=6, Lead_drive=6.5, Master=5),
      FOURx12, eq=[1, 2, 0, -3, -2, 0, 1.5, 0, -1.5], gate=-50, doubler=8, width=125)
    # ---- The Stage (2016)
    a("The Stage (Album Rhythm)", 'Rhythm', "The Stage (2016) · modern and articulate, for odd-time prog riffs", A('moirai', channel='Red · lead', Gain=5.5, Volume=5, Low=5, Mid=5.5, High=6, Presence=6, Resonance=5.5),
      pre=[ts('II', -3, 3)], eq='tight', gate=-48, doubler=7, width=120)
    a("The Stage (Album Lead)", 'Lead', "The Stage (2016) · fluid, compressed shred lead with a stereo echo", A('strix', channel='Lead', voice='Lead 2', Gain=6.5, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      pre=[comp(-26, 3, 5, 50)], eq='lead', gate=-58, post=[sdly('1/8.', 30, 20), hall(3, 18)])
    a("The Stage", 'Rhythm', "The Stage (2016) · modern prog rhythm, tight and punchy", A('umbra', channel='Channel 3', mode='Mark IIC+', Gain=6, Treble=6.5, Middle=5, Bass=4.5, Presence=6, Master=5, Output=5),
      eq='tight', gate=-50, doubler=7)
    a("God Damn", 'Rhythm', "The Stage (2016) · fast, aggressive and dry", A('umbra', channel='Channel 3', mode='Extreme', Gain=6.5, Treble=6.5, Middle=5, Bass=5, Presence=6, Master=5, Output=5),
      eq='thrash', gate=-48, doubler=6, width=120)
    a("Angels", 'Clean', "The Stage (2016) · the clean ballad: chorus, echo and a long reverb", A('strix', channel='Clean', voice='Clean UK', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Resonance=5, Master=5),
      pre=[comp(-26, 3, 5)], post=[chorus(0.45, 30, 28), dly(480, 28, 16), hall(3.6, 24)], width=125)
    a("Exist", 'Ambient', "The Stage (2016) · the 15-minute epic's spacey lead with a shimmer", A('strix', channel='Lead', voice='Lead 2', Gain=5.5, Bass=4.5, Middle=6, Treble=6, Presence=5.5, Resonance=5, Master=5),
      eq='lead', gate=-60, post=[dly(560, 40, 24), fx('elysium', Decay=7, Size=90, Pre_delay=50, Tone=6000, Low_Cut=170, Mod=5, Mix=28)], sfx=[shimmer(7, 30, 22)], width=130)
    # ---- Life Is But a Dream... (2023)
    a("Life Is But a Dream (Album Rhythm)", 'Rhythm', "Life Is But a Dream... (2023) · thick, modern and a little unhinged", A('vulcan', channel='Ch 3 · mega', voice='Potty Time', Gain=6, Low=6, Mid=5, High=5.5, Presence=5.5, Depth=6, Master=5),
      FOURx12, eq='tight', gate=-50, doubler=7, width=120)
    a("Life Is But a Dream (Album Lead)", 'Lead', "Life Is But a Dream... (2023) · wide, glossy modern lead with a shimmer tail", A('strix', channel='Lead', voice='Lead 2', Gain=5.5, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      eq='lead', gate=-60, post=[sdly('1/8.', 30, 20), shimmer(6, 25, 20)], width=125)
    a("Cosmic", 'Synth', "Life Is But a Dream... (2023) · clean guitar with a spacey synth arpeggio behind it", A('strix', channel='Clean', voice='Clean UK', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Resonance=5, Master=5),
      synth=S('Cosmic Arp', mix=45), post=[chorus(0.4, 30, 25), sdly('1/8.', 30, 18), hall(4, 26)], width=130)
    a("Cosmic (Solo)", 'Lead', "Life Is But a Dream... (2023) · the huge outro lead, a supersaw doubling it", A('strix', channel='Lead', voice='Lead 2', Gain=5.5, Bass=4.5, Middle=6.5, Treble=6, Presence=6, Resonance=5, Master=5),
      eq='lead', gate=-60, synth=S('Cosmic Lead', mix=30), post=[dly(520, 35, 22), shimmer(6, 25, 22)])
    a("G", 'Synth', "Life Is But a Dream... (2023) · heavy riffing with a snarling sync synth on top", A('erebus', Gain=6.5, Bass=5.5, Middle=5.5, Treble=6, Presence=6, Depth=5.5, Master=5),
      FOURx12, eq='tight', gate=-50, synth=S('G-Force', mix=35), doubler=7)
    a("(O)rdinary", 'Synth', "Life Is But a Dream... (2023) · funky clean guitar and a talking robot synth", A('persephone', channel='Ch I', voice='Bright', Volume=4, Treble=6.5, Middle=5, Bass=5, Reverb=1),
      pre=[comp(-24, 5, 8)], synth=S('(O)rdinary Robot', mix=55), post=[room(10)])
    a("Nobody", 'Rhythm', "Life Is But a Dream... (2023) · thick riff with a sub octave", A('vulcan', channel='Ch 3 · mega', voice='Physics', Gain=6, Low=6, Mid=5, High=5.5, Presence=5.5, Depth=6, Master=5),
      FOURx12, pre=[fx('richter', Dry=100, Sub_1=35, Sub_2=0, Low_Pass=250, Drive=2)], eq='tight', gate=-50, doubler=7)
    a("Game Over", 'Rhythm', "Life Is But a Dream... (2023) · twitchy prog riffs with a phaser", A('kratos', channel='Lead', Lead_pre=6, Lead_post=5, Low=5.5, Mid=5.5, High=6, Resonance=5.5, Presence=6),
      FOURx12, pre=[phaser(0.5, 45, 25, '6', 35)], eq='tight', gate=-50, doubler=7)
    a("Mattel", 'Rhythm', "Life Is But a Dream... (2023) · jarring stop-start riffs with a swirling flanger", A('vulcan', channel='Ch 3 · mega', voice='III Gain FAT', Gain=6, Low=5.5, Mid=5.5, High=6, Presence=6, Depth=5.5, Master=5),
      FOURx12, eq='tight', gate=-48, doubler=7, post=[flanger(0.2, 40, 35, 1.8, 25)])

    # =====================================================================================
    a = artist('Pink Floyd')
    for sel, title, note in (('STUDIO', 'Comfortably Numb (Studio Solo)', 'The Wall (1979) · the studio solo, Big Muff into Hiwatt'),
                             ('DSOT', 'Comfortably Numb (Delicate Sound)', 'Delicate Sound of Thunder (1988) · the live 80s solo'),
                             ('PULSE', 'Comfortably Numb (Pulse)', 'Pulse (1995) · the famous live solo'),
                             ('RTN', 'Comfortably Numb (Remember That Night)', 'Remember That Night (2006) · Royal Albert Hall'),
                             ('GDANSK', 'Comfortably Numb (Gdansk)', 'Live in Gdansk (2008) · the solo in the shipyard'),
                             ('POMPEII', 'Comfortably Numb (Pompeii)', 'Live at Pompeii (2016) · the amphitheatre solo'),
                             ('ROME', 'Comfortably Numb (Rome)', 'Rattle That Lock tour, Rome (2015)')):
        a(title, 'Lead', note, A('eos', channel='Linked', Volume=6, Bass=5, Treble=6, Middle=5.5, Presence=5.5, Master=6, Solo=sel),
          gate=-62, post=[dly(440, 25, 18, 'Tape'), hall(3, 20), fx('limiter', Input=12, Ceiling=-1, Release=120)])
    a("Comfortably Numb (Rhythm)", 'Clean', "The Wall (1979) · the clean chord stabs in the chorus", A('eos', channel='Brilliant', Volume=5, Bass=5, Treble=6.5, Middle=5, Presence=5.5, Master=6),
      pre=[comp(-26, 4, 6)], post=[chorus(0.6, 30, 25), hall(2.6, 18)])
    a("Time (Solo)", 'Lead', "The Dark Side of the Moon (1973) · a fuzzy, biting Hiwatt solo", A('eos', channel='Linked', Volume=6, Bass=5, Treble=6.5, Middle=6, Presence=6, Master=6),
      pre=[fuzz(6, 6.5, -4)], gate=-62, post=[fx('mnemosyne', Time=380, Intensity=30, Heads='3', Wow=15, Age=30, Spring=0, Echo=26, Tone=5000), hall(2.8, 16)])
    a("Money", 'Crunch', "The Dark Side of the Moon (1973) · gritty 7/4 riff and solo", A('eos', channel='Linked', boost='Klon drive', Volume=6, Bass=5, Treble=6.5, Middle=6, Presence=6, Master=6),
      post=[tape(300, 18, 14), room(10)])
    a("Shine On You Crazy Diamond", 'Lead', "Wish You Were Here (1975) · the slow, glassy four-note theme", A('eos', channel='Brilliant', boost='Klon pushed', Volume=5.5, Bass=4.5, Treble=6.5, Middle=5, Presence=5.5, Master=6),
      pre=[comp(-28, 4, 6)], post=[chorus(0.35, 30, 25), dly(540, 30, 20, 'Tape'), hall(4, 24)])
    a("Wish You Were Here", 'Acoustic', "Wish You Were Here (1975) · the intro, heard through an old radio", A('orpheus', take='Take 1', Gain=4, Bass=4, Middle=5, Treble=6, Body=5, Reverb=1, Volume=5),
      eq='lofi', post=[fx('hex', Bits=11, Downsample=5, Mix=50, Tone=5000), room(12), fx('limiter', Input=12, Ceiling=-1, Release=120)])
    a("Another Brick in the Wall", 'Clean', "The Wall (1979) · the funky, compressed clean riff with eighth-note echo", A('eos', channel='Brilliant', Volume=4.5, Bass=4.5, Treble=7, Middle=5, Presence=6, Master=6),
      pre=[comp(-30, 6, 9, 85)], post=[sdly('1/8', 20, 16, 'Digital'), room(8)])
    a("Echoes", 'Ambient', "Meddle (1971) · the ping and the long, spacey delays", A('eos', channel='Normal', Volume=5, Bass=5, Treble=6, Middle=5, Presence=5, Master=6),
      post=[fx('mnemosyne', Time=620, Intensity=50, Heads='1+2+3', Wow=20, Age=30, Spring=0, Echo=34, Tone=5000), flanger(0.12, 35, 30, 2.5, 25), hall(5, 28)], width=140)
    a("Dogs", 'Lead', "Animals (1977) · bright, chorused lead with long echoes", A('eos', channel='Linked', boost='Klon drive', Volume=6, Bass=4.5, Treble=6.5, Middle=5.5, Presence=6, Master=6),
      post=[chorus(0.5, 30, 25), dly(500, 35, 22, 'Ping-Pong'), hall(3.5, 20)])
    a("Run Like Hell", 'Clean', "The Wall (1979) · the galloping repeats on a clean guitar", A('eos', channel='Brilliant', Volume=5, Bass=4.5, Treble=6.5, Middle=5, Presence=5.5, Master=6),
      pre=[comp(-26, 4, 6)], post=[sdly('1/8.', 35, 35, 'Digital'), hall(2.8, 18)])

    # =====================================================================================
    a = artist('TesseracT')
    a("Concealing Fate", 'Rhythm', "One (2011) · clean-edged djent: tight, articulate, deep", A('umbra', channel='Channel 3', mode='Extreme', Gain=5.5, Treble=6.5, Middle=5, Bass=4.5, Presence=6, Master=5, Output=5),
      pre=[fx('xxxiii', Drive=0, Level=3, Mix=100)], eq='djent', gate=-44, doubler=6, width=125)
    a("Nocturne", 'Rhythm', "Altered State (2013) · polished modern rhythm", A('moros', boost='Plumes M2', Gain=5.5, Bass=5.5, Mid=5.5, Treble=6, Level=5, Master=5, Tight=6.5, Air=5.5),
      FOURx12, eq='djent', gate=-44, doubler=6, width=120)
    a("Of Matter", 'Clean', "Altered State (2013) · the shimmering ambient cleans", A('persephone', channel='Ch I', voice='Bright', Volume=4, Treble=6.5, Middle=4.5, Bass=5, switches=['chorus'], Speed=2.5, Depth=4),
      pre=[comp(-28, 4, 7)], post=[sdly('1/8.', 40, 26), hall(5, 30)], sfx=[shimmer(7, 30, 25)], width=140)
    a("Juno", 'Ambient', "Sonder (2018) · ambient swells over clean chords", A('dionysus', channel='Green · clean', voice='Bright', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Master=5),
      post=[swell(380, 45), sdly('1/4', 40, 22)], sfx=[shimmer(8, 35, 30)], width=140)
    a("King", 'Rhythm', "Sonder (2018) · heavy, low and modern", A('nihil', amp='Blend of both', Master=5, Presence=6, Bass=5.5, Middle=5, Treble=6, Drive=6, Gain=6),
      eq='djent', gate=-44, doubler=6)
    a("War of Being", 'Lead', "War of Being (2023) · liquid lead with long delays", A('dionysus', channel='Red · lead', voice='711 retake', Gain=5.5, Bass=4.5, Middle=6, Treble=6, Presence=5.5, Master=5),
      eq='lead', gate=-58, post=[dly(560, 40, 22, 'Ping-Pong'), hall(4, 22)])

    # =====================================================================================
    a = artist('Periphery')
    a("Icarus Lives!", 'Rhythm', "Periphery (2010) · Misha's Invective: tight, crushing riffs", A('lyssa', channel='Rhythm', voice='Riff 1 + pedal', Gain=6, Low=5.5, Mid=5.5, High=6, Presence=6, Resonance=5.5, Volume=5),
      eq='djent', gate=-44, doubler=6, width=120)
    a("Scarlet", 'Rhythm', "Periphery II (2012) · bright, modern prog riffing", A('lyssa', channel='Rhythm', voice='Riff 2 + pedal', Gain=5.5, Low=5, Mid=6, High=6.5, Presence=6, Resonance=5, Volume=5),
      eq='tight', gate=-46, doubler=6)
    a("Marigold", 'Rhythm', "Periphery III: Select Difficulty (2016) · huge, slow and heavy", A('typhon', channel='Red', Gain=6, Volume=5, Low=6, Mid=5, High=6, Presence=5.5, Resonance=6),
      pre=[ts('I', -3, 3)], eq='djent', gate=-44, doubler=7, width=125)
    a("Blood Eagle", 'Rhythm', "Periphery III (2016) · the heaviest of them all", A('moros', boost='Blower Box', Gain=6.5, Bass=6, Mid=5, Treble=6, Level=5, Master=5, Tight=6.5, Air=5),
      FOURx12, eq='djent', gate=-42, doubler=6)
    a("Ji (Solo)", 'Lead', "Periphery II (2012) · smooth fusion-metal lead", A('lyssa', channel='Lead', voice='Solo + pedal', Gain=6, Low=4.5, Mid=6.5, High=6, Presence=6, Resonance=5, Volume=5),
      eq='lead', gate=-58, post=[dly(460, 30, 20, 'Ping-Pong'), plate(2.4, 16)])
    a("Jetpacks Was Yes!", 'Clean', "Periphery (2010) · bright, chorused clean", A('persephone', channel='Ch I', voice='Bright', Volume=4.5, Treble=6.5, Middle=5, Bass=5, switches=['chorus'], Speed=3, Depth=5),
      pre=[comp(-26, 4, 6)], post=[sdly('1/8', 25, 16), hall(3, 20)])
    a("Satellites", 'Ambient', "Periphery IV: Hail Stan (2019) · wide ambient clean with an octave shimmer", A('dionysus', channel='Green · clean', voice='Normal', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Master=5),
      post=[chorus(0.4, 35, 30), sdly('1/4', 40, 22)], sfx=[shimmer(7, 40, 30)], width=140)

    # =====================================================================================
    a = artist('John Mayer')
    a("Gravity", 'Lead', "Continuum (2006) · smooth Dumble overdrive, every note sings", A('hyperion', channel='Overdrive', voice='Ford', Volume=5, Treble=5, Middle=6, Bass=5, Presence=5, Drive=5.5, Master=5),
      ONEx12, post=[dly(420, 22, 14, 'Tape'), plate(2.4, 18)])
    a("Slow Dancing in a Burning Room", 'Lead', "Continuum (2006) · warm overdrive lead with a big room", A('hyperion', channel='Overdrive', voice='Smooth', Volume=5, Treble=5.5, Middle=6, Bass=5, Presence=5, Drive=6, Master=5),
      ONEx12, post=[dly(380, 20, 12, 'Tape'), hall(2.8, 20)])
    a("Neon", 'Clean', "Room for Squares (2001) · bright, percussive, compressed clean", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=6.5, Bass=4.5, Middle=5.5, Reverb=1.5),
      pre=[comp(-28, 5, 8, 80, 6)], post=[room(10)])
    a("Daughters", 'Clean', "Heavier Things (2003) · warm, round clean picking", A('hyperion', channel='Clean', voice='Warm', Volume=4.5, Treble=5, Middle=5.5, Bass=5.5, Presence=4.5, Master=5),
      ONEx12, pre=[comp(-26, 3, 5)], post=[spring(14)])
    a("Waiting on the World to Change", 'Clean', "Continuum (2006) · clean with a gentle tremolo", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=6, Bass=5, Middle=5, Reverb=2, Speed=4.5, Intensity=3.5),
      pre=[comp(-26, 3, 5)], post=[spring(14)])
    a("Vultures", 'Crunch', "Continuum (2006) · a little hair on a clean amp, bluesy and dry", A('hyperion', channel='Overdrive', voice='Carlton', Volume=5, Treble=5.5, Middle=6, Bass=5, Presence=5.5, Drive=4.5, Master=5),
      ONEx12, post=[room(12)])
    a("Who Did You Think I Was", 'Crunch', "Try! (2005) · trio blues-rock crunch", A('tyche', Gain=5, Treble=6, Middle=6.5, Bass=5, Presence=5.5, Volume=6, Master=6),
      cab('2x12'), pre=[klon(2, 60, 4, 'HIGH')], post=[room(12)])
    a("Bold as Love", 'Crunch', "Continuum (2006) · a Hendrix cover: vibe and crunch", A('melinoe', mic='SM57', Volume=6, Treble=7.5, Bass=5, Middle=6, Reverb=2),
      pre=[fx('overdrive', Drive=4, Tone=6, Level=0.5, Mix=100), vibe(2.8, 55)], post=[spring(14)])
    a("New Light", 'Clean', "Sob Rock era (2018) · glossy 80s clean with chorus", A('persephone', channel='Ch I', voice='Bright', Volume=4.5, Treble=6.5, Middle=5, Bass=5, switches=['chorus'], Speed=3, Depth=5),
      pre=[comp(-26, 4, 7)], post=[dly(360, 20, 12), hall(2.4, 16)])
    a("Stop This Train", 'Acoustic', "Continuum (2006) · fingerpicked acoustic", A('orpheus', take='Take 2', Gain=3.5, Bass=5, Middle=5, Treble=5.5, Body=7, Reverb=2, Volume=5),
      eq='acoustic', post=[room(14)])
    a("Edge of Desire", 'Lead', "Battle Studies (2009) · clean-ish lead with long, soft echoes", A('hyperion', channel='Overdrive', voice='PAB clear drive', Volume=5, Treble=5.5, Middle=5.5, Bass=5, Presence=5, Drive=4.5, Master=5),
      ONEx12, post=[dly(520, 30, 18, 'Tape'), hall(3, 20)])

    # =====================================================================================
    a = artist('Van Halen')
    a("Eruption", 'Lead', "Van Halen (1978) · cranked Plexi, a phaser and tape echo. E flat tuning", A('tyche', Gain=8, Treble=6.5, Middle=6.5, Bass=5, Presence=6.5, Volume=8, Master=6),
      FOURx12, pre=[phaser(0.5, 55, 20, '4', 45)], gate=-60, post=[tape(360, 25, 18), plate(2.4, 18)])
    a("Ain't Talkin' 'bout Love", 'Rhythm', "Van Halen (1978) · the brown sound, a touch of flanger", A('tyche', Gain=8, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=8, Master=6),
      FOURx12, pre=[flanger(0.2, 40, 30, 1.6, 30)], gate=-60, post=[plate(2, 16)])
    a("Unchained", 'Rhythm', "Fair Warning (1981) · the big flanged riff", A('tyche', Gain=8, Treble=6, Middle=6.5, Bass=5.5, Presence=6, Volume=8, Master=6),
      FOURx12, pre=[flanger(0.15, 75, 65, 1.4, 55)], gate=-60, post=[plate(2, 14)])
    a("Panama", 'Rhythm', "1984 (1984) · hot, bright, party rock", A('tyche', Gain=8.5, Treble=7, Middle=6, Bass=5, Presence=6.5, Volume=8, Master=6),
      FOURx12, pre=[boost(6, 100, 0, 1)], gate=-60, post=[plate(2, 14)], doubler=6)
    a("Hot for Teacher", 'Rhythm', "1984 (1984) · fast boogie riffing", A('tyche', Gain=8.5, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=8, Master=6),
      FOURx12, gate=-60, post=[room(12)])
    a("Jump", 'Synth', "1984 (1984) · that synth-brass riff, played on guitar", M('glass65', gain=2.5, treble=6),
      synth=S('Rooftop Brass', mix=85), post=[hall(2.8, 22)])
    a("Judgement Day", 'Rhythm', "For Unlawful Carnal Knowledge (1991) era · the 5150 head", A('hecate', Gain=6, Volume=5, Low=5.5, Mid=6, High=6, Presence=6, Resonance=5),
      FOURx12, gate=-56, post=[plate(2, 14)], doubler=6)

    # =====================================================================================
    a = artist('Pantera')
    # Nemean plays the "Pantera Full Tone" captures (hellyeahdime), one per album. They come out very bright and deeply
    # scooped (Cowboys most of all), so every preset puts mids back, cuts the fizz above 6 kHz hard and keeps the bite.
    # Hand-tuned against the tone check (tonecheck.py): mids about -6 dB, bite +2, fizz about -16 like a bright real rig.
    CFH, VDOP, FBD, TGST = 'Cowboys From Hell', 'Vulgar Display of Power', 'Far Beyond Driven', 'The Great Southern Trendkill'
    a("Cowboys from Hell", 'Rhythm', "Cowboys from Hell (1990) · the rawest album: biting, fast, a touch of mids left in",
      A('nemean', album=CFH, Gain=6, Bass=5.5, Middle=8, Treble=4, Presence=2.5, Tight=4, Master=5),
      eq=[0, 1, 0, 1, 2, 0, -1, -8, -8], gate=-50, doubler=6, params={'eq.hc': 7500})
    a("Domination", 'Rhythm', "Cowboys from Hell (1990) · the thrash riffing and the breakdown",
      A('nemean', album=CFH, Gain=6.5, Bass=6, Middle=8, Treble=4, Presence=2.5, Tight=4.5, Master=5),
      eq=[0, 1.5, 0, 0.5, 2, 0, -1, -8, -8], gate=-50, doubler=6, params={'eq.hc': 7500})
    a("Cemetery Gates (Clean)", 'Clean', "Cowboys from Hell (1990) · the clean intro with chorus and echo", A('persephone', channel='Ch I', voice='Bright', Volume=4.5, Treble=6.5, Middle=5, Bass=5, switches=['chorus'], Speed=3, Depth=6),
      post=[dly(460, 30, 18), hall(3, 22)])
    a("Cemetery Gates (Solo)", 'Lead', "Cowboys from Hell (1990) · the screaming solo, more mids so it sings",
      A('nemean', album=CFH, Gain=6.5, Bass=4.5, Middle=8.5, Treble=4, Presence=2.5, Tight=5, Master=5),
      eq=[-1, 0, 0, 2, 2.5, 0, -1, -8, -8], gate=-58, params={'eq.hc': 7000}, post=[dly(420, 28, 18), plate(2.4, 16)])
    a("Walk", 'Rhythm', "Vulgar Display of Power (1992) · the swaggering, scooped groove",
      A('nemean', album=VDOP, Gain=6, Bass=6.5, Middle=8.5, Treble=4, Presence=3.5, Tight=3.5, Master=5),
      eq=[1, 2, 1, 2, 2, 0, 0, -6, -6], gate=-48, doubler=6, params={'eq.hc': 9000})
    a("Mouth for War", 'Rhythm', "Vulgar Display of Power (1992) · tight, punchy and mean",
      A('nemean', album=VDOP, Gain=6.5, Bass=6, Middle=8.5, Treble=4, Presence=3.5, Tight=4.5, Master=5),
      eq=[0, 1.5, 0.5, 2, 2, 0, 0, -6, -6], gate=-48, doubler=6, params={'eq.hc': 9000})
    a("I'm Broken", 'Rhythm', "Far Beyond Driven (1994) · heavier and thicker, the low end chugs",
      A('nemean', album=FBD, Gain=6.5, Bass=6, Middle=7.5, Treble=4, Presence=3.5, Tight=4, Master=5),
      eq=[1, 1.5, 0, 1, 1.5, 0, 0, -6.5, -6], gate=-48, doubler=6, params={'eq.hc': 9000})
    a("5 Minutes Alone", 'Rhythm', "Far Beyond Driven (1994) · the slow, crushing groove",
      A('nemean', album=FBD, Gain=7, Bass=6.5, Middle=7.5, Treble=4, Presence=3.5, Tight=3.5, Master=5),
      eq=[1.5, 1.5, 0, 1, 1.5, 0, 0, -6.5, -6], gate=-46, doubler=7, params={'eq.hc': 9000})
    a("Drag the Waters", 'Rhythm', "The Great Southern Trendkill (1996) · dark, saturated, swampy",
      A('nemean', album=TGST, Gain=6.5, Bass=5.5, Middle=6.5, Treble=5, Presence=4.5, Tight=4, Master=5),
      eq=[0.5, 1, 0, 0.5, 0.5, 0, 0, -3.5, -5], gate=-48, doubler=6, params={'eq.hc': 10000})
    a("The Great Southern Trendkill", 'Rhythm', "The Great Southern Trendkill (1996) · the title track's fast, angry riffing",
      A('nemean', album=TGST, Gain=7, Bass=5.5, Middle=6.5, Treble=5, Presence=4.5, Tight=4.5, Master=5),
      eq=[0, 1, 0, 0.5, 0.5, 0, 0, -3.5, -5], gate=-48, doubler=6, params={'eq.hc': 10000})
    a("Floods (Solo)", 'Lead', "The Great Southern Trendkill (1996) · the long, wide outro solo",
      A('nemean', album=TGST, Gain=6.5, Bass=4.5, Middle=7, Treble=5, Presence=4.5, Tight=5, Master=5),
      eq=[-1, 0, 1, 1.5, 1.5, 0.5, 0, -3, -5], gate=-58, params={'eq.hc': 9500}, post=[dly(500, 35, 22, 'Ping-Pong'), hall(3.2, 20)])

    # =====================================================================================
    a = artist('Dream Theater')
    a("Pull Me Under", 'Rhythm', "Images and Words (1992) · Mark IIC+ prog crunch", A('citadel', lead='Lead', Volume=6.5, Treble=6.5, Bass=4, Middle=5.5, Presence=6, Lead_drive=6, Master=5),
      FOURx12, gate=-54, doubler=6, post=[room(10)])
    a("The Glass Prison", 'Rhythm', "Six Degrees of Inner Turbulence (2002) · tight, heavy rhythm", A('eris', channel='Ch 2 · crunch', Gain=7, Treble=6, Middle=5, Bass=5, Presence=6, Master=5, Output=5),
      eq='tight', gate=-50, doubler=6)
    a("Metropolis Pt. 1", 'Lead', "Images and Words (1992) · fast, singing lead lines", A('eris', channel='Ch 3 · lead', voice='Lead', Gain=6, Treble=6, Middle=6, Bass=4.5, Presence=6, Master=5, Output=5),
      eq='lead', gate=-58, post=[dly(400, 25, 16), plate(2.2, 14)])
    a("Octavarium (Solo)", 'Lead', "Octavarium (2005) · the epic Petrucci lead", A('eris', channel='Ch 3 · lead', voice='Petrucci lead', boost='TS808', Gain=6, Treble=6, Middle=6.5, Bass=4.5, Presence=6, Master=5, Output=5),
      eq='lead', gate=-60, post=[dly(480, 32, 22, 'Ping-Pong'), hall(3, 20)])
    a("The Spirit Carries On (Solo)", 'Lead', "Metropolis Pt. 2 (1999) · warm, emotional lead", A('eris', channel='Ch 3 · lead', voice='JP lead', Gain=5.5, Treble=5.5, Middle=6.5, Bass=5, Presence=5.5, Master=5, Output=5),
      gate=-60, post=[dly(520, 35, 22, 'Tape'), hall(3.4, 22)])
    a("Petrucci Clean", 'Clean', "Glassy compressed clean with chorus and echo", A('eris', channel='Ch 1 · clean', Gain=4, Treble=6.5, Middle=5, Bass=5, Presence=6, Master=5, Output=5),
      pre=[comp(-28, 4, 7)], post=[chorus(0.6, 40, 35), dly(450, 30, 18), hall(3, 20)], width=130)

    # =====================================================================================
    a = artist('Meshuggah')
    a("Bleed", 'Rhythm', "obZen (2008) · the machine-gun polyrhythm tone", A('nihil', amp='Fortin Meshuggah', Master=5, Presence=6, Bass=5, Middle=5.5, Treble=6.5, Drive=6, Gain=6.5),
      pre=[fx('xxxiii', Drive=0, Level=3, Mix=100)], eq='djent', gate=-42, doubler=5)
    a("Rational Gaze", 'Rhythm', "Nothing (2002) · slower, thicker, lower", A('nihil', amp='Blend of both', Master=5, Presence=5.5, Bass=6, Middle=5, Treble=6, Drive=6, Gain=6.5),
      eq='djent', gate=-44, doubler=6)
    a("Future Breed Machine", 'Rhythm', "Destroy Erase Improve (1995) · rougher, older and dirtier", A('nihil', amp='Sunn Model T + Butterslax', Master=5, Presence=6, Bass=5.5, Middle=5.5, Treble=6, Drive=6, Gain=6),
      eq='tight', gate=-46, doubler=6)
    a("Thordendal Lead", 'Lead', "Smooth, legato, synth-like fusion lead over the chaos", A('dionysus', channel='Red · lead', voice='711 (br)', Gain=6, Bass=4.5, Middle=6.5, Treble=5.5, Presence=5, Master=5),
      pre=[comp(-30, 6, 8, 90, 5)], gate=-60, post=[dly(540, 38, 26, 'Ping-Pong', 3500, 30), hall(4, 24)])

    # =====================================================================================
    a = artist('Gojira')
    # Phlegethon plays the "Black Go" capture of the Gojira X plugin: already tight and mid-forward, so the EQ stays light.
    a("Flying Whales", 'Rhythm', "From Mars to Sirius (2005) · massive, organic heavy tone",
      A('phlegethon', Gain=6, Bass=6, Middle=5.5, Treble=5.5, Presence=5.5, Depth=6, Master=5), eq=[0.5, 1, 0, -0.5, 0, 0.5, 1, 0, -1], gate=-50, doubler=7)
    a("Backbone", 'Rhythm', "From Mars to Sirius (2005) · fast, galloping and dry",
      A('phlegethon', Gain=6.5, Bass=5.5, Middle=5.5, Treble=6, Presence=6, Depth=5.5, Master=5), eq=[0, 0.5, -0.5, -0.5, 0, 1, 1, 0, -1], gate=-48, doubler=6)
    a("Stranded", 'Rhythm', "Magma (2016) · the whammy squeals: octave-up whammy blended in",
      A('phlegethon', Gain=6, Bass=5.5, Middle=5.5, Treble=6, Presence=6, Depth=5.5, Master=5),
      pre=[whammy(12, 55)], eq=[0, 0.5, 0, -0.5, 0, 1, 1, 0, -1], gate=-48, doubler=6)
    a("L'Enfant Sauvage", 'Rhythm', "L'Enfant Sauvage (2012) · tight, choppy, pick-scrape heavy",
      A('phlegethon', Gain=6, Bass=5.5, Middle=6, Treble=6, Presence=6, Depth=5, Master=5),
      pre=[ts('II', -3, 3)], eq=[-0.5, 0.5, 0, 0, 0.5, 1, 1, 0, -1], gate=-48, doubler=6)
    a("Silvera", 'Rhythm', "Magma (2016) · modern, punchy, groovy",
      A('phlegethon', Gain=5.5, Bass=5.5, Middle=5.5, Treble=6, Presence=6, Depth=5.5, Master=5), eq=[0, 0.5, 0, -0.5, 0, 1, 1, 0, -1], gate=-48, doubler=6)
    a("Born for One Thing", 'Rhythm', "Fortitude (2021) · thick, bouncy, the strings slapping",
      A('phlegethon', Gain=6, Bass=6, Middle=5, Treble=5.5, Presence=5.5, Depth=6, Master=5), eq=[0.5, 1, -0.5, -1, 0, 0.5, 1, 0, -1], gate=-48, doubler=7)
    a("Amazonia (Clean)", 'Clean', "Fortitude (2021) · earthy clean with a twang", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=6, Bass=5, Middle=5.5, Reverb=2),
      post=[dly(380, 20, 12, 'Tape'), room(12)])

    # =====================================================================================
    a = artist('Slipknot')
    a("Wait and Bleed", 'Rhythm', "Slipknot (1999) · low-tuned, raw and furious", A('hecate', Gain=7, Volume=5, Low=6.5, Mid=4.5, High=6.5, Presence=6, Resonance=6.5),
      FOURx12, eq='scoop', gate=-46, doubler=6)
    a("Duality", 'Rhythm', "Vol. 3 (2004) · huge Rectifier wall, low tuning", A('talos', Gain=7, Treble=6, Middle=4.5, Bass=6.5, Presence=6, Master=5, Output=5),
      eq='modern', gate=-46, doubler=7, width=125)
    a("Psychosocial", 'Rhythm', "All Hope Is Gone (2008) · tight, heavy and driving", A('talos', boost='TS808 boost', Gain=6.5, Treble=6, Middle=5, Bass=6, Presence=6, Master=5, Output=5),
      eq='tight', gate=-46, doubler=7)
    a("Before I Forget", 'Rhythm', "Vol. 3 (2004) · the punchy chorus riff", A('talos', Gain=6.5, Treble=6.5, Middle=5, Bass=6, Presence=6.5, Master=5, Output=5),
      eq='tight', gate=-46, doubler=7)
    a("Snuff", 'Clean', "All Hope Is Gone (2008) · the clean ballad picking", A('melinoe', mic='SM57 + 414', Volume=4, Treble=5.5, Bass=5, Middle=5, Reverb=2),
      post=[chorus(0.4, 25, 20), hall(3, 22)])

    # =====================================================================================
    a = artist('Slayer')
    a("Raining Blood", 'Rhythm', "Reign in Blood (1986) · boosted JCM800, scooped and vicious", A('chrysaor', Gain=9, Bass=6, Middle=4, Treble=7, Presence=7, Master=5),
      FOURx12, pre=[sd1('D IV', -3, 5)], eq='thrash', gate=-52, doubler=5)
    a("Angel of Death", 'Rhythm', "Reign in Blood (1986) · fast and dry", A('chrysaor', Gain=8.5, Bass=6, Middle=4.5, Treble=7, Presence=6.5, Master=5),
      FOURx12, pre=[sd1('D III', -4, 5)], eq='thrash', gate=-52, doubler=5)
    a("South of Heaven", 'Rhythm', "South of Heaven (1988) · slower, darker, heavier", A('chrysaor', Gain=8.5, Bass=6.5, Middle=5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[sd1('D II', -4, 5)], eq='thrash', gate=-54, doubler=6, post=[room(10)])
    a("Seasons in the Abyss", 'Rhythm', "Seasons in the Abyss (1990) · thicker 90s Slayer", A('chrysaor', Gain=8.5, Bass=6, Middle=5, Treble=6.5, Presence=6.5, Master=5),
      FOURx12, pre=[ts('II', -2, 4)], eq='scoop', gate=-52, doubler=6)

    # =====================================================================================
    a = artist('Megadeth')
    a("Holy Wars... The Punishment Due", 'Rhythm', "Rust in Peace (1990) · tight, mid-scooped, razor sharp", A('chrysaor', Gain=8.5, Bass=5.5, Middle=4.5, Treble=7, Presence=6.5, Master=5),
      FOURx12, pre=[sd1('D II', -4, 5)], eq='thrash', gate=-52, doubler=5, width=120)
    a("Hangar 18 (Solo)", 'Lead', "Rust in Peace (1990) · the trading solos", A('chrysaor', Gain=8.5, Bass=4.5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[sd1('D I', -5, 5)], eq='lead', gate=-58, post=[dly(380, 22, 16), plate(2, 14)])
    a("Symphony of Destruction", 'Rhythm', "Countdown to Extinction (1992) · slower, chunkier", A('chrysaor', Gain=8, Bass=6, Middle=5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[ts('II', -3, 4)], eq='tight', gate=-54, doubler=6)
    a("Tornado of Souls (Solo)", 'Lead', "Rust in Peace (1990) · the solo every guitarist learns", A('chrysaor', Gain=9, Bass=4.5, Middle=7, Treble=6.5, Presence=6.5, Master=5),
      FOURx12, pre=[sd1('D I', -4, 6)], eq='lead', gate=-58, post=[dly(400, 25, 18), plate(2.2, 16)])

    # =====================================================================================
    a = artist("Guns N' Roses")
    a("Sweet Child O' Mine", 'Lead', "Appetite for Destruction (1987) · the intro riff, warm and singing. E flat tuning", A('chrysaor', Gain=7, Bass=5, Middle=7, Treble=6, Presence=5.5, Master=5),
      FOURx12, eq='mid', gate=-62, post=[dly(380, 20, 14), plate(2.2, 16)])
    a("Welcome to the Jungle", 'Crunch', "Appetite for Destruction (1987) · the riff with its echo", A('chrysaor', Gain=7.5, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, gate=-60, post=[dly(320, 25, 18), room(12)])
    a("Paradise City", 'Crunch', "Appetite for Destruction (1987) · big rock crunch", A('tyche', Gain=7.5, Treble=6.5, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, gate=-60, doubler=6, post=[room(10)])
    a("November Rain (Solo)", 'Lead', "Use Your Illusion I (1991) · the soaring outro solo", A('chrysaor', Gain=8, Bass=5, Middle=7, Treble=6, Presence=5.5, Master=5),
      FOURx12, pre=[ts('I', -4, 4)], eq='lead', gate=-62, post=[dly(480, 32, 22), hall(3.4, 22)])
    a("Knockin' on Heaven's Door (Solo)", 'Lead', "Use Your Illusion II (1991) · warm, slow, wide lead", A('tyche', Gain=7, Treble=6, Middle=7, Bass=5, Presence=5.5, Volume=7, Master=6),
      FOURx12, eq='lead', gate=-62, post=[dly(520, 30, 20, 'Tape'), hall(3, 20)])

    # =====================================================================================
    a = artist('Whitesnake')
    a("Fool for Your Loving", 'Crunch', "Ready an' Willing (1980) · bluesy British crunch with a bit of room",
      A('tyche', Gain=5.5, Treble=6, Middle=7, Bass=5, Presence=5.5, Volume=7, Master=6), FOURx12,
      gate=-64, doubler=6, post=[room(12)])
    a("Ain't No Love in the Heart of the City", 'Crunch', "Snakebite (1978) · slow blues, warm and just breaking up",
      A('tyche', Gain=4.5, Treble=5.5, Middle=7, Bass=5, Presence=5, Volume=6.5, Master=6), FOURx12,
      gate=-66, post=[spring(14)])
    a("Slide It In", 'Crunch', "Slide It In (1984) · swaggering mid-80s hard rock",
      A('chrysaor', Gain=6.5, Bass=5, Middle=6.5, Treble=6.5, Presence=6.5, Master=5), FOURx12,
      gate=-60, doubler=6, post=[room(10)])
    a("Still of the Night", 'Rhythm', "Whitesnake (1987) · the huge, saturated riff: a boosted Marshall, thick and tight",
      A('chrysaor', Gain=8, Bass=5.5, Middle=6.5, Treble=6.5, Presence=6.5, Master=5), FOURx12,
      pre=[ts('II', -2, 5)], gate=-56, doubler=8, width=135, post=[room(10)])
    a("Here I Go Again '87", 'Crunch', "Whitesnake (1987) · the big, polished arena chords",
      A('chrysaor', Gain=7, Bass=5, Middle=6.5, Treble=6.5, Presence=6.5, Master=5), FOURx12,
      pre=[ts('II', -4, 4)], gate=-60, doubler=8, width=135, post=[dly(380, 18, 10), hall(2.4, 14)])
    a("Is This Love", 'Clean', "Whitesnake (1987) · lush chorused clean with long echoes",
      A('melinoe', mic='SM57 + 414', Volume=4, Treble=8, Bass=4.5, Middle=5.5, Reverb=1.5),
      pre=[comp(-26, 3, 6, 70)], post=[chorus(0.7, 45, 40), dly(440, 30, 20), hall(3, 22)], width=130)
    a("Is This Love (Solo)", 'Lead', "Whitesnake (1987) · the singing, chorused 80s lead",
      A('chrysaor', Gain=7.5, Bass=5, Middle=7, Treble=6.5, Presence=6.5, Master=5), FOURx12,
      pre=[ts('I', -4, 4)], gate=-62, post=[chorus(0.6, 30, 25), dly(460, 32, 22), hall(3.2, 22)])
    a("Crying in the Rain '87", 'Lead', "Whitesnake (1987) · the fiery solo: lots of gain, wide echo",
      A('chrysaor', Gain=8.5, Bass=5, Middle=7, Treble=6.5, Presence=6.5, Master=5), FOURx12,
      pre=[ts('II', -2, 5)], gate=-60, post=[sdly('1/4', 30, 20, 'Ping-Pong'), hall(3, 20)])
    a("Slip of the Tongue", 'Rhythm', "Slip of the Tongue (1989) · the title track: slick, high-gain late-80s shred rhythm",
      A('thanatos', Preamp=7, Bass=5.5, Middle=6, Treble=6.5, Presence=6, Depth=5.5, Master=5), FOURx12,
      gate=-56, doubler=8, width=130)

    # =====================================================================================
    a = artist('AC/DC')
    a("Back in Black", 'Crunch', "Back in Black (1980) · less gain than you think: punchy, open, loud", A('tyche', Gain=5, Treble=6.5, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, gate=-66, doubler=8, width=130, post=[room(10)])
    a("Highway to Hell", 'Crunch', "Highway to Hell (1979) · crisp, ringing chords", A('tyche', Gain=4.5, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, gate=-66, doubler=8, width=130, post=[room(10)])
    a("Thunderstruck", 'Lead', "The Razors Edge (1990) · the hammer-on intro: a touch more gain", A('tyche', Gain=6.5, Treble=6.5, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, gate=-62, post=[dly(330, 18, 12), room(12)])
    a("T.N.T.", 'Crunch', "T.N.T. (1975) · bright, bouncy, dry", A('tyche', Gain=5, Treble=7, Middle=6.5, Bass=4.5, Presence=6.5, Volume=7, Master=6),
      FOURx12, gate=-66, post=[room(8)])

    # =====================================================================================
    a = artist('Led Zeppelin')
    a("Whole Lotta Love", 'Crunch', "Led Zeppelin II (1969) · thick, compressed riff with backwards-echo vibe", A('tyche', Gain=6.5, Treble=6, Middle=7, Bass=5.5, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[comp(-22, 3, 4, 60)], gate=-64, post=[tape(340, 20, 12), room(12)])
    a("Black Dog", 'Crunch', "Led Zeppelin IV (1971) · the squashed, buzzing riff", A('tyche', Gain=7, Treble=6.5, Middle=7.5, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[comp(-30, 8, 8, 90, 3)], gate=-62, doubler=5)
    a("Stairway to Heaven (Solo)", 'Lead', "Led Zeppelin IV (1971) · the solo, warm and biting", A('tyche', Gain=7.5, Treble=6, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, eq='lead', gate=-62, post=[tape(380, 20, 14), plate(2, 14)])
    a("Stairway to Heaven (Intro)", 'Acoustic', "Led Zeppelin IV (1971) · the fingerpicked intro, 12-string shimmer", A('orpheus', take='Take 2', Gain=3.5, Bass=5, Middle=5, Treble=6, Body=6.5, Reverb=2, Volume=5),
      eq='acoustic', pre=[fx('lyra', Octave=45, Detune=6, Spread=8, Chime=15, Width=55, Mix=100)], post=[hall(2.6, 18)])
    a("Kashmir", 'Crunch', "Physical Graffiti (1975) · the hypnotic riff with a slow phaser", A('tyche', Gain=6.5, Treble=6, Middle=6.5, Bass=5.5, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[phaser(0.25, 45, 25, '4', 35)], gate=-64, post=[hall(2.6, 18)])
    a("Immigrant Song", 'Crunch', "Led Zeppelin III (1970) · galloping, tight crunch", A('tyche', Gain=6.5, Treble=6.5, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, gate=-64, doubler=5, post=[room(10)])

    # =====================================================================================
    a = artist('Jimi Hendrix')
    a("Purple Haze", 'Crunch', "Are You Experienced (1967) · fuzz into a Marshall, octave-up on the solo. E flat tuning", A('tyche', Gain=6, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[fx('icarus', Fuzz=70, Octave=55, Tone=3000, Level=-4, Mix=100)], eq='fuzz', post=[room(12)])
    a("Little Wing", 'Clean', "Axis: Bold as Love (1967) · the clean, chordal intro through a rotary speaker", A('melinoe', mic='SM57 + 414', Volume=5, Treble=6, Bass=5, Middle=5.5, Reverb=2),
      pre=[vibe(1.2, 60, 'Rotary')], post=[spring(16)])
    a("Voodoo Child (Slight Return)", 'Lead', "Electric Ladyland (1968) · wah and fuzz, wide open", A('tyche', Gain=6.5, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[wah('Envelope', 6.5, 7.5, 8), fuzz(6.5, 6, -3)], eq='fuzz', post=[room(12)])
    a("Machine Gun", 'Lead', "Band of Gypsys (1970) · fuzz and a throbbing uni-vibe", A('tyche', Gain=6, Treble=6, Middle=6.5, Bass=5.5, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[fuzz(7, 5.5, -3), vibe(1.6, 75)], eq='fuzz', post=[hall(2.4, 16)])
    a("All Along the Watchtower", 'Lead', "Electric Ladyland (1968) · bright fuzz lead with echo", A('tyche', Gain=6, Treble=7, Middle=6, Bass=5, Presence=6.5, Volume=7, Master=6),
      FOURx12, pre=[fuzz(6, 6.5, -3)], post=[tape(300, 22, 16), room(12)])

    # =====================================================================================
    a = artist('Stevie Ray Vaughan')
    a("Pride and Joy", 'Crunch', "Texas Flood (1983) · a Tube Screamer into a loud Fender. E flat tuning", A('melinoe', mic='SM57', Volume=6, Treble=6.5, Bass=5, Middle=6, Reverb=2.5),
      pre=[fx('overdrive', Drive=2.5, Tone=6, Level=0, Mix=100)], post=[spring(14)])
    a("Texas Flood", 'Lead', "Texas Flood (1983) · slow blues, fat and stinging", A('melinoe', mic='SM57 + 414', Volume=5.5, Treble=7.5, Bass=5.5, Middle=6.5, Reverb=3),
      pre=[fx('overdrive', Drive=3.5, Tone=5.5, Level=-1.5, Mix=100)], post=[spring(16)])
    a("Lenny", 'Clean', "Texas Flood (1983) · soft, swirling clean through a rotary speaker", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=6, Bass=5, Middle=5, Reverb=2.5),
      pre=[vibe(1.4, 55, 'Rotary')], post=[spring(18)])
    a("Scuttle Buttin'", 'Crunch', "Couldn't Stand the Weather (1984) · fast, bright, a bit more drive", A('melinoe', mic='SM57', Volume=5.5, Treble=7, Bass=5, Middle=6, Reverb=2),
      pre=[fx('overdrive', Drive=4, Tone=6.5, Level=-3.5, Mix=100)], post=[spring(12)])

    # =====================================================================================
    a = artist('Queen')
    a("Bohemian Rhapsody (Solo)", 'Lead', "A Night at the Opera (1975) · treble booster into a cranked AC30", A('astraea', channel='Top Boost', boost='Treble booster', Volume=8, Treble=6.5, Bass=5, Tone_Cut=3.5, Reverb=0, Master=7),
      cab('2x12'), eq='lead', gate=-62, post=[tape(360, 18, 14), hall(2.6, 18)])
    a("Brighton Rock", 'Lead', "Sheer Heart Attack (1974) · two long echoes for the famous delay solo", A('astraea', channel='Top Boost', boost='Treble booster', Volume=8, Treble=6.5, Bass=5, Tone_Cut=3.5, Reverb=0, Master=7),
      cab('2x12'), gate=-62, post=[dly(800, 0, 45, 'Digital', 6000), fx('lethe', Time=1600, Feedback=0, Mix=45, Mode='Digital', Tone=6000), room(10)])
    a("Killer Queen", 'Lead', "Sheer Heart Attack (1974) · the bright, layered lead", A('astraea', channel='Top Boost', boost='Treble booster', Volume=7, Treble=7, Bass=4.5, Tone_Cut=3, Reverb=0, Master=6.5),
      cab('2x12'), gate=-62, post=[plate(2, 16)])
    a("Tie Your Mother Down", 'Crunch', "A Day at the Races (1976) · punchy rock riff", A('astraea', channel='Top Boost', boost='Treble booster', Volume=7.5, Treble=6.5, Bass=5, Tone_Cut=4, Reverb=0, Master=7),
      cab('2x12'), gate=-62, doubler=6, post=[room(10)])

    # =====================================================================================
    a = artist('U2')
    a("Where the Streets Have No Name", 'Clean', "The Joshua Tree (1987) · dotted-eighth echoes on a chiming AC30", A('astraea', channel='Top Boost', Volume=5, Treble=7, Bass=4.5, Tone_Cut=3, Reverb=0, Master=6),
      cab('2x12'), post=[fx('gemini', Time=476, Pattern='DOTTED', Feedback=32, Cross=40, Tone=6000, Mod=10, Mix=38), hall(2.4, 16)], width=130)
    a("With or Without You", 'Ambient', "The Joshua Tree (1987) · the endless, swelling 'infinite guitar'", A('astraea', channel='Top Boost', Volume=5, Treble=6.5, Bass=4.5, Tone_Cut=3.5, Reverb=0, Master=6),
      cab('2x12'), post=[swell(600, 50), sdly('1/8.', 40, 26)], sfx=[shimmer(10, 30, 30)], width=140)
    a("Sunday Bloody Sunday", 'Crunch', "War (1983) · biting, jangly crunch", A('astraea', channel='Top Boost', boost='Treble booster', Volume=6.5, Treble=7, Bass=4.5, Tone_Cut=3, Reverb=0, Master=6),
      cab('2x12'), post=[sdly('1/8', 20, 16), room(10)])
    a("Pride (In the Name of Love)", 'Clean', "The Unforgettable Fire (1984) · bright arpeggios with dotted-eighth echo", A('astraea', channel='Top Boost', Volume=5.5, Treble=7, Bass=4.5, Tone_Cut=3, Reverb=0, Master=6),
      cab('2x12'), post=[fx('gemini', Time=566, Pattern='DOTTED', Feedback=30, Cross=40, Tone=6000, Mod=10, Mix=34), hall(2.6, 16)], width=130)

    # =====================================================================================
    a = artist('Nirvana')
    a("Smells Like Teen Spirit (Verse)", 'Clean', "Nevermind (1991) · the clean verse riff with chorus", A('melinoe', mic='SM57', Volume=4.5, Treble=6, Bass=5, Middle=5, Reverb=1),
      pre=[chorus(0.8, 55, 50)], post=[room(10)])
    a("Smells Like Teen Spirit (Chorus)", 'Rhythm', "Nevermind (1991) · the distorted chorus: loud, simple, huge", A('tyche', Gain=5.5, Treble=6, Middle=6, Bass=6, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[fx('emperor', Mode='DIST', Drive=3, Volume=2, Mix=100)], gate=-60, doubler=8, width=130)
    a("Come as You Are", 'Clean', "Nevermind (1991) · watery, deep chorus on the riff", A('melinoe', mic='SM57', Volume=4.5, Treble=5.5, Bass=5, Middle=5, Reverb=1),
      pre=[chorus(1.2, 80, 60, '1')], post=[room(10)])
    a("Heart-Shaped Box", 'Rhythm', "In Utero (1993) · raw, fuzzy, abrasive", A('tyche', Gain=5.5, Treble=6.5, Middle=6, Bass=5.5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[fuzz(7, 6, -3)], eq='fuzz', doubler=7)
    a("Lithium", 'Rhythm', "Nevermind (1991) · the loud chorus distortion", A('tyche', Gain=5.5, Treble=6, Middle=6, Bass=6, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[fx('emperor', Mode='DIST', Drive=2, Volume=2, Mix=100)], gate=-60, doubler=8)

    # =====================================================================================
    a = artist('Alice in Chains')
    # Jerry Cantrell: dark, thick, mid-heavy Marshall-type crunch, lots of wah. Most songs in E flat.
    a("Man in the Box", 'Rhythm', "Facelift (1990) · the talk-box riff (an envelope wah stands in for the talk box). E flat tuning",
      A('chrysaor', Gain=7, Bass=6, Middle=6.5, Treble=6.5, Presence=6.5, Master=5), FOURx12,
      pre=[wah('Envelope', 5.5, 5, 5)], gate=-58, doubler=7, post=[room(10)])
    a("Man in the Box (Solo)", 'Lead', "Facelift (1990) · the wah solo: vocal, nasty, mid-heavy. E flat tuning",
      A('chrysaor', Gain=7.5, Bass=5, Middle=7.5, Treble=5.5, Presence=5, Master=5), FOURx12,
      pre=[wah('Manual', pos=58, q=8), ts('I', -4, 4)], gate=-62, post=[dly(380, 22, 14), plate(2, 14)])
    a("Them Bones", 'Rhythm', "Dirt (1992) · the lurching drop riff, thick and dark. Drop D flat tuning",
      A('chrysaor', Gain=7.5, Bass=6, Middle=6.5, Treble=6.5, Presence=6.5, Master=5), FOURx12,
      pre=[ts('II', -3, 4)], gate=-54, doubler=7)
    a("Would?", 'Rhythm', "Dirt (1992) · heavy, brooding and wide. E flat tuning",
      A('thanatos', Preamp=6.5, Bass=6, Middle=6.5, Treble=5.5, Presence=5, Depth=5.5, Master=5), FOURx12,
      gate=-56, doubler=8, width=130, post=[room(10)])
    a("Rooster", 'Rhythm', "Dirt (1992) · the crushing chorus riffs. E flat tuning",
      A('thanatos', Preamp=7, Bass=6, Middle=6.5, Treble=5.5, Presence=5, Depth=6, Master=5), FOURx12,
      gate=-54, doubler=8, width=130)
    a("Down in a Hole", 'Clean', "Dirt (1992) · the haunted clean picking with a little chorus and echo. E flat tuning",
      A('melinoe', mic='SM57 + 414', Volume=4, Treble=8, Bass=4.5, Middle=5.5, Reverb=1.5),
      pre=[chorus(0.6, 35, 30)], post=[dly(430, 25, 16), hall(2.6, 18)])
    a("No Excuses", 'Clean', "Jar of Flies (1994) · bright, chorused clean strumming. E flat tuning",
      A('persephone', channel='Ch I', voice='Bright', Volume=4.5, Treble=7.5, Middle=5.5, Bass=4.5, switches=['chorus'], Speed=2.5, Depth=5),
      post=[room(12)])
    a("Nutshell", 'Acoustic', "Jar of Flies (1994) · the sad, open acoustic part. E flat tuning",
      A('orpheus', take='Take 2', Gain=4, Bass=4.5, Middle=7, Treble=7.5, Body=5, Reverb=2.5, Volume=5),
      post=[hall(2.8, 18)])
    a("Grind", 'Rhythm', "Alice in Chains (1995) · slow, sludgy and grinding",
      A('erebus', Gain=6, Bass=6, Middle=6, Treble=5.5, Presence=5, Depth=6, Master=5),
      gate=-52, doubler=8, width=130)
    a("Check My Brain", 'Rhythm', "Black Gives Way to Blue (2009) · the bending, seasick main riff. Drop D flat tuning",
      A('lamia', channel='HBE', mic='SM57', Gain=6, Bass=5.5, Middle=6.5, Treble=7.5, Presence=7, Master=5),
      gate=-54, doubler=7)

    # =====================================================================================
    a = artist('Radiohead')
    a("Creep", 'Rhythm', "Pablo Honey (1993) · the clean verse, then the CRUNCH (this is the crunch)", A('tyche', Gain=6, Treble=6.5, Middle=6, Bass=5.5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[fx('emperor', Mode='DIST', Drive=6, Volume=2, Mix=100)], gate=-56, doubler=7)
    a("Paranoid Android", 'Lead', "OK Computer (1997) · the frantic fuzz section", A('astraea', channel='Top Boost', Volume=6.5, Treble=6.5, Bass=5, Tone_Cut=4, Reverb=0, Master=6.5),
      cab('2x12'), pre=[fuzz(7.5, 6.5, -3)], eq='fuzz', post=[room(10)])
    a("Weird Fishes", 'Clean', "In Rainbows (2007) · interlocking arpeggios in a cloud of echo", A('astraea', channel='Normal', Volume=5, Treble=6, Bass=5, Tone_Cut=4, Reverb=1, Master=6),
      cab('2x12'), post=[sdly('1/8.', 45, 30), hall(3.6, 22)], width=135)
    a("Just", 'Lead', "The Bends (1995) · squealing fuzz runs", A('astraea', channel='Top Boost', Volume=6.5, Treble=7, Bass=5, Tone_Cut=3.5, Reverb=0, Master=6.5),
      cab('2x12'), pre=[fuzz(8, 6.5, -3)], post=[dly(300, 15, 10)])
    a("Everything in Its Right Place", 'Synth', "Kid A (2000) · that hazy electric piano, played from your guitar", M('glass65', gain=2, treble=5.5),
      synth=S('VHS Memories', mix=85, level=-8), post=[dly(500, 30, 16, 'Tape'), hall(3, 22)])

    # =====================================================================================
    a = artist('Dire Straits')
    a("Sultans of Swing", 'Clean', "Dire Straits (1978) · fingerpicked, snappy, middle-pickup clean", M('glass65', gain=3.5, bass=4.5, mid=6, treble=6.5, presence=6),
      cab('2x12'), pre=[comp(-28, 4, 7, 80, 6)], post=[room(14)])
    a("Money for Nothing", 'Crunch', "Brothers in Arms (1985) · the riff through a half-open wah", A('chrysaor', Gain=6.5, Bass=4.5, Middle=7, Treble=5.5, Presence=5, Master=5),
      FOURx12, pre=[wah('Manual', pos=42, q=8)], gate=-60, post=[room(12)])
    a("Brothers in Arms (Solo)", 'Lead', "Brothers in Arms (1985) · slow, crying lead in a big space", A('tyche', Gain=6.5, Treble=5.5, Middle=7, Bass=5, Presence=5, Volume=6, Master=6),
      FOURx12, eq='lead', gate=-62, post=[dly(560, 35, 22, 'Tape'), hall(3.6, 24)])
    a("Romeo and Juliet", 'Acoustic', "Making Movies (1980) · a bright, steely resonator-like acoustic", A('orpheus', take='Take 1', Gain=4, Bass=4, Middle=6, Treble=6.5, Body=5, Reverb=2, Volume=5),
      eq='acoustic', post=[room(14)])

    # =====================================================================================
    a = artist('Eric Clapton')
    a("Sunshine of Your Love", 'Crunch', "Disraeli Gears (1967) · the dark, round 'woman tone'", A('tyche', Gain=7, Treble=4, Middle=7, Bass=5.5, Presence=4.5, Volume=7, Master=6),
      FOURx12, eq='warm', gate=-62, post=[room(12)])
    a("Layla", 'Lead', "Layla and Other Assorted Love Songs (1970) · the stinging riff", A('tyche', Gain=7, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      cab('2x12'), gate=-62, post=[room(12)])
    a("Wonderful Tonight", 'Clean', "Slowhand (1977) · gentle clean with a soft chorus", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=5.5, Bass=5, Middle=5.5, Reverb=2),
      post=[chorus(0.5, 30, 25), hall(2.4, 18)])
    a("Crossroads", 'Lead', "Wheels of Fire (1968) · live Cream: loud, fast, raw", A('tyche', Gain=7.5, Treble=6, Middle=7, Bass=5, Presence=6, Volume=8, Master=6),
      FOURx12, gate=-62, post=[room(12)])

    # =====================================================================================
    a = artist('Steve Vai')
    a("For the Love of God", 'Lead', "Passion and Warfare (1990) · liquid, vocal lead with long echoes", A('dionysus', channel='Red · lead', voice='711 (br)', Gain=6, Bass=4.5, Middle=6.5, Treble=6, Presence=5.5, Master=5),
      eq='lead', gate=-60, post=[dly(500, 35, 22, 'Ping-Pong'), hall(3.4, 22)])
    a("Tender Surrender", 'Lead', "Alien Love Secrets (1995) · clean to screaming with your pick and volume", A('dionysus', channel='Red · lead', voice='711 retake', Gain=5, Bass=5, Middle=6, Treble=5.5, Presence=5, Master=5),
      gate=-62, post=[dly(460, 30, 20), hall(3, 20)])
    a("The Audience Is Listening", 'Lead', "Passion and Warfare (1990) · bright, aggressive, playful", A('dionysus', channel='Red · lead', voice='711 (br)', Gain=6.5, Bass=4.5, Middle=6, Treble=6.5, Presence=6, Master=5),
      gate=-58, pre=[phaser(0.8, 40, 20, '6', 30)], post=[dly(360, 22, 16), plate(2, 14)])
    a("Bad Horsie", 'Rhythm', "Alien Love Secrets (1995) · heavy, low, horse-like wah squeals", A('dionysus', channel='Red · lead', voice='711 (br)', Gain=6.5, Bass=5.5, Middle=5, Treble=6, Presence=6, Master=5),
      pre=[wah('Envelope', 7, 8, 9)], gate=-54, doubler=6)

    # =====================================================================================
    a = artist('Joe Satriani')
    a("Surfing with the Alien", 'Lead', "Surfing with the Alien (1987) · wah-drenched, hot lead", A('chrysaor', Gain=8, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[wah('Envelope', 6.5, 8, 8), sd1('D I', -6, 4)], gate=-58, post=[dly(380, 22, 16), plate(2.2, 14)])
    a("Always with Me, Always with You", 'Lead', "Surfing with the Alien (1987) · the warm melodic lead", A('chrysaor', Gain=7, Bass=5, Middle=7, Treble=6, Presence=5.5, Master=5),
      FOURx12, eq='lead', gate=-60, post=[chorus(0.5, 25, 20), dly(440, 28, 20), hall(2.8, 18)])
    a("Satch Boogie", 'Lead', "Surfing with the Alien (1987) · fast boogie and tapping", A('chrysaor', Gain=8, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[sd1('D II', -5, 4)], gate=-58, post=[plate(2, 14)])
    a("Flying in a Blue Dream", 'Clean', "Flying in a Blue Dream (1989) · shimmering, chorused clean", A('persephone', channel='Ch I', voice='Bright', Volume=4.5, Treble=6.5, Middle=5, Bass=5, switches=['chorus'], Speed=2.5, Depth=6),
      post=[dly(480, 35, 20), hall(3.4, 22)], width=130)

    # =====================================================================================
    a = artist('Polyphia')
    a("G.O.A.T.", 'Clean', "New Levels New Devils (2018) · bright, squashed, sparkly clean", A('persephone', channel='Ch II', voice='Bright', Volume=4.5, Treble=7, Middle=5, Bass=5, Reverb=1),
      pre=[comp(-32, 8, 10, 85, 4)], post=[fx('glue', Threshold=-18, Ratio=3, Attack=10, Release=120, Makeup=3, Mix=60), plate(1.8, 14)])
    a("Playing God", 'Acoustic', "Remember That You Will Die (2022) · nylon-flavoured, percussive and dry", A('orpheus', take='Take 2', Gain=4, Bass=4.5, Middle=5.5, Treble=6, Body=6, Reverb=1, Volume=5),
      eq='acoustic', pre=[comp(-28, 5, 7, 70, 5)], post=[room(12)])
    a("Ego Death", 'Clean', "Remember That You Will Die (2022) · glossy clean with a modern sheen", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=6.5, Bass=5, Middle=5, Reverb=1),
      pre=[comp(-30, 6, 9, 85, 5)], post=[chorus(0.4, 25, 20), plate(2, 14)])
    a("Polyphia Lead", 'Lead', "Smooth, compressed lead with stereo echoes", A('dionysus', channel='Blue · crunch', voice='Fat + Protein OD', Gain=6, Bass=4.5, Middle=6, Treble=6, Presence=5.5, Master=5),
      pre=[comp(-28, 5, 6, 80, 6)], gate=-58, post=[sdly('1/8.', 25, 18), plate(2, 14)])

    # =====================================================================================
    a = artist('Animals as Leaders')
    a("CAFO", 'Rhythm', "Animals as Leaders (2009) · articulate eight-string djent", A('umbra', channel='Channel 3', mode='Extreme', Gain=5.5, Treble=6.5, Middle=5.5, Bass=4.5, Presence=6, Master=5, Output=5),
      pre=[fx('xxxiii', Drive=0, Level=2, Mix=100)], eq='djent', gate=-44, doubler=6)
    a("Physical Education", 'Rhythm', "The Joy of Motion (2014) · tight, glassy, percussive", A('moros', boost='Plumes M2', Gain=5.5, Bass=5, Mid=5.5, Treble=6.5, Level=5, Master=5, Tight=7, Air=6),
      FOURx12, eq='djent', gate=-44, doubler=6)
    a("Tempting Time", 'Rhythm', "Animals as Leaders (2009) · heavy, then clean and back", A('umbra', channel='Channel 3', mode='Extreme', Gain=6, Treble=6.5, Middle=5, Bass=4.5, Presence=6, Master=5, Output=5),
      eq='djent', gate=-44, doubler=6)
    a("Tosin Clean", 'Clean', "Sparkly, compressed clean for thumping and tapping", A('umbra', channel='Channel 1', mode='Clean', Gain=4, Treble=6.5, Middle=5, Bass=5, Presence=6, Master=5, Output=5),
      pre=[comp(-30, 6, 9, 85, 4)], post=[chorus(0.5, 25, 20), sdly('1/8', 25, 14), plate(2, 14)])

    # =====================================================================================
    a = artist('Plini')
    a("Electric Sunrise", 'Lead', "Handmade Cities (2016) · bright, singing lead in a sea of echo", A('dionysus', channel='Blue · crunch', voice='Fat + Protein OD', Gain=6.5, Bass=4.5, Middle=6, Treble=6, Presence=5.5, Master=5),
      eq='lead', gate=-58, post=[sdly('1/8.', 32, 22), hall(3.6, 24)], width=130)
    a("Handmade Cities", 'Rhythm', "Handmade Cities (2016) · clear, modern prog crunch", A('dionysus', channel='Red · lead', voice='711 retake', Gain=5.5, Bass=5, Middle=5.5, Treble=6, Presence=5.5, Master=5),
      eq='tight', gate=-52, doubler=6)
    a("Selenium Forest", 'Clean', "Handmade Cities (2016) · glassy, chorused clean", A('persephone', channel='Ch I', voice='Bright', Volume=4.5, Treble=6.5, Middle=5, Bass=5, switches=['chorus'], Speed=3, Depth=5),
      pre=[comp(-28, 4, 7)], post=[sdly('1/8.', 30, 20), hall(3, 20)])
    a("Every Piece Matters", 'Ambient', "Impulse Voices (2020) · dreamy clean with shimmer", A('dionysus', channel='Green · clean', voice='Bright', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Master=5),
      post=[chorus(0.4, 30, 25), fx('astral', Time=500, Feedback=50, Shimmer=40, Interval='+OCT', Diffuse=45, Mod=25, Tone=6000, Mix=30)], sfx=[fx('nimbus', Decay=14, Cloud=70, Drift=40, Swell=300, Tone=5500, Mix=30)], width=140)

    # =====================================================================================
    a = artist('Deftones')
    a("Change (In the House of Flies)", 'Rhythm', "White Pony (2000) · dark, washed-out heavy chords", A('ares', channel='Lead', mic='Mic mix', Gain=6, Bass=6, Middle=5, Treble=5.5, Presence=5.5, Depth=6, Master=5),
      post=[chorus(0.3, 30, 25), fx('halcyon', Mode='VERB', Mix=30, Decay=4)], gate=-54, doubler=8, width=130)
    a("My Own Summer (Shove It)", 'Rhythm', "Around the Fur (1997) · huge, low, punishing", A('talos', Gain=7, Treble=5.5, Middle=5, Bass=7, Presence=5.5, Master=5, Output=5),
      eq='modern', gate=-50, doubler=8, width=125)
    a("Be Quiet and Drive (Far Away)", 'Rhythm', "Around the Fur (1997) · shoegazey distortion, wide and blurry", A('ares', channel='Crunch', mic='Mic mix', Gain=6, Bass=5.5, Middle=5.5, Treble=6, Presence=5.5, Depth=5.5, Master=5),
      post=[chorus(0.35, 35, 30), fx('halcyon', Mode='VERB', Mix=35, Decay=5)], gate=-56, doubler=9, width=140)
    a("Diamond Eyes", 'Rhythm', "Diamond Eyes (2010) · tight, crushing modern Deftones", A('typhon', channel='Red', Gain=6, Volume=5, Low=6.5, Mid=5, High=5.5, Presence=5.5, Resonance=6),
      eq='modern', gate=-48, doubler=7)

    # =====================================================================================
    a = artist('System of a Down')
    a("Chop Suey!", 'Rhythm', "Toxicity (2001) · drop C, tight and frantic", A('talos', Gain=6.5, Treble=6.5, Middle=5, Bass=6, Presence=6.5, Master=5, Output=5),
      eq='tight', gate=-48, doubler=7, width=125)
    a("Toxicity", 'Rhythm', "Toxicity (2001) · drop C, heavy and bouncy", A('talos', Gain=6.5, Treble=6, Middle=5, Bass=6.5, Presence=6, Master=5, Output=5),
      eq='tight', gate=-48, doubler=7)
    a("B.Y.O.B.", 'Rhythm', "Mezmerize (2005) · drop C, aggressive and fast", A('talos', boost='TS808 boost', Gain=6.5, Treble=6.5, Middle=5, Bass=6, Presence=6.5, Master=5, Output=5),
      eq='tight', gate=-48, doubler=7)
    a("Aerials", 'Clean', "Toxicity (2001) · the clean, eastern-sounding intro", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=6, Bass=5, Middle=5.5, Reverb=1.5),
      post=[dly(420, 20, 12), hall(2.4, 16)])

    # =====================================================================================
    a = artist('Linkin Park')
    a("One Step Closer", 'Rhythm', "Hybrid Theory (2000) · thick nu-metal Rectifier", A('talos', Gain=6.5, Treble=6, Middle=4.5, Bass=6.5, Presence=6, Master=5, Output=5),
      eq='scoop', gate=-48, doubler=8, width=130)
    a("In the End", 'Rhythm', "Hybrid Theory (2000) · the chorus wall", A('talos', Gain=6, Treble=6, Middle=5, Bass=6, Presence=6, Master=5, Output=5),
      eq='modern', gate=-50, doubler=8, width=130)
    a("Numb", 'Synth', "Meteora (2003) · heavy chorus guitars with a string pad behind them", A('talos', Gain=6, Treble=6, Middle=5, Bass=6, Presence=6, Master=5, Output=5),
      eq='modern', gate=-50, synth=S('String Section', mix=30), doubler=8, width=130)
    a("Faint", 'Rhythm', "Meteora (2003) · tight and driving", A('talos', boost='TS808 boost', Gain=6.5, Treble=6.5, Middle=5, Bass=6, Presence=6.5, Master=5, Output=5),
      eq='tight', gate=-48, doubler=7)

    # =====================================================================================
    a = artist('Iron Maiden')
    a("The Trooper", 'Lead', "Piece of Mind (1983) · the galloping twin-lead melody", A('chrysaor', Gain=7.5, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, gate=-60, post=[dly(380, 20, 14), plate(2, 14)])
    a("Hallowed Be Thy Name", 'Rhythm', "The Number of the Beast (1982) · galloping crunch", A('chrysaor', Gain=7.5, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, gate=-60, doubler=6, post=[room(10)])
    a("Fear of the Dark", 'Lead', "Fear of the Dark (1992) · the melodic intro lead with chorus", A('chrysaor', Gain=7, Bass=5, Middle=6.5, Treble=6, Presence=5.5, Master=5),
      FOURx12, gate=-60, post=[chorus(0.5, 30, 25), dly(420, 25, 18), hall(2.8, 18)])
    a("Run to the Hills", 'Rhythm', "The Number of the Beast (1982) · bright, punchy gallop", A('tyche', Gain=7.5, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, gate=-60, doubler=6, post=[room(10)])

    # =====================================================================================
    a = artist('Black Sabbath')
    a("Iron Man", 'Rhythm', "Paranoid (1970) · a treble booster into a cranked head", A('tyche', Gain=6.5, Treble=6, Middle=6.5, Bass=5.5, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[boost(10, 200, -2, 5)], eq='fuzz', gate=-62, doubler=6)
    a("Paranoid", 'Rhythm', "Paranoid (1970) · fast, fuzzy and raw", A('tyche', Gain=7, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[boost(10, 200, -2, 5)], eq='fuzz', gate=-62)
    a("War Pigs", 'Rhythm', "Paranoid (1970) · slow and menacing, a little wah in the solo", A('tyche', Gain=6.5, Treble=6, Middle=6.5, Bass=5.5, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[boost(10, 200, -2, 5)], eq='fuzz', gate=-62, post=[room(10)])
    a("N.I.B.", 'Lead', "Black Sabbath (1970) · wah lead over the riff", A('tyche', Gain=7, Treble=6, Middle=6.5, Bass=5.5, Presence=5.5, Volume=7, Master=6),
      FOURx12, pre=[wah('Envelope', 6, 7, 7), boost(9, 200, -2, 4)], eq='fuzz', gate=-62)

    # =====================================================================================
    a = artist('Bullet for My Valentine')
    a("Tears Don't Fall", 'Rhythm', "The Poison (2005) · tight, polished metalcore", A('kratos', channel='Lead', Lead_pre=6, Lead_post=5, Low=6, Mid=5, High=6.5, Resonance=6, Presence=6),
      pre=[ts('II', 0, 4)], eq='tight', gate=-48, doubler=7, width=125)
    a("Waking the Demon", 'Rhythm', "Scream Aim Fire (2008) · fast, thrashy and tight", A('kratos', channel='Lead', switches=['crunch'], Lead_pre=6.5, Lead_post=5, Low=6, Mid=5, High=6.5, Resonance=6, Presence=6),
      pre=[ts('II', 0, 4)], eq='tight', gate=-48, doubler=6)
    a("Scream Aim Fire", 'Rhythm', "Scream Aim Fire (2008) · the title-track wall", A('kratos', channel='Lead', Lead_pre=6.5, Lead_post=5, Low=6, Mid=5, High=6.5, Resonance=6, Presence=6),
      pre=[ts('III', 0, 4)], eq='tight', gate=-48, doubler=7)
    a("Your Betrayal", 'Rhythm', "Fever (2010) · heavy, grinding intro", A('kratos', channel='Lead', Lead_pre=6.5, Lead_post=5, Low=6.5, Mid=5, High=6, Resonance=6.5, Presence=6),
      pre=[ts('II', 0, 4)], eq='modern', gate=-46, doubler=7)

    # =====================================================================================
    a = artist('Trivium')
    a("Pull Harder on the Strings of Your Martyr", 'Rhythm', "Ascendancy (2005) · fast metalcore thrash", A('kratos', channel='Lead', Lead_pre=6.5, Lead_post=5, Low=6, Mid=5, High=6.5, Resonance=6, Presence=6.5),
      pre=[ts('II', 0, 4)], eq='thrash', gate=-48, doubler=6)
    a("In Waves", 'Rhythm', "In Waves (2011) · low, heavy and huge", A('hecate', Gain=6.5, Volume=5, Low=6.5, Mid=5, High=6, Presence=6, Resonance=6.5),
      pre=[ts('II', 0, 4)], eq='modern', gate=-46, doubler=7)
    a("Shogun (Clean)", 'Clean', "Shogun (2008) · the clean, eastern-flavoured intro", A('moirai', channel='Green · clean', Gain=4, Volume=5, Low=5, Mid=5, High=6, Presence=5.5, Resonance=5),
      post=[chorus(0.4, 30, 25), dly(460, 28, 16), hall(2.8, 18)])
    a("Down from the Sky", 'Rhythm', "Shogun (2008) · tight thrash crunch", A('kratos', channel='Lead', Lead_pre=6, Lead_post=5, Low=6, Mid=5.5, High=6.5, Resonance=5.5, Presence=6),
      pre=[ts('III', 0, 4)], eq='tight', gate=-48, doubler=6)

    # =====================================================================================
    a = artist('Killswitch Engage')
    a("My Curse", 'Rhythm', "As Daylight Dies (2006) · melodic metalcore rhythm", A('kratos', channel='Lead', Lead_pre=6, Lead_post=5, Low=6, Mid=5.5, High=6.5, Resonance=6, Presence=6),
      pre=[ts('II', 0, 4)], eq='tight', gate=-48, doubler=7, width=125)
    a("The End of Heartache", 'Rhythm', "The End of Heartache (2004) · heavy and melodic", A('talos', boost='TS808 boost', Gain=6.5, Treble=6, Middle=5, Bass=6, Presence=6, Master=5, Output=5),
      eq='tight', gate=-48, doubler=7)
    a("Rose of Sharyn", 'Rhythm', "The End of Heartache (2004) · driving, open chords", A('kratos', channel='Lead', Lead_pre=6, Lead_post=5, Low=6, Mid=5.5, High=6.5, Resonance=6, Presence=6),
      pre=[ts('II', 0, 4)], eq='tight', gate=-48, doubler=7, post=[room(8)])

    # =====================================================================================
    a = artist('Opeth')
    a("Ghost of Perdition", 'Rhythm', "Ghost Reveries (2005) · thick, dark, prog death metal", A('chrysaor', Gain=8, Bass=6, Middle=5.5, Treble=6, Presence=5.5, Master=5),
      FOURx12, pre=[ts('I', -3, 4)], eq='modern', gate=-52, doubler=7)
    a("Windowpane", 'Clean', "Damnation (2003) · melancholic clean with a soft echo", A('melinoe', mic='SM57 + 414', Volume=4.5, Treble=5.5, Bass=5, Middle=5.5, Reverb=2),
      post=[chorus(0.35, 25, 20), dly(480, 28, 16, 'Tape'), hall(3, 22)])
    a("Deliverance", 'Rhythm', "Deliverance (2002) · crushing, mid-heavy riffing", A('chrysaor', Gain=8.5, Bass=6, Middle=5.5, Treble=6, Presence=6, Master=5),
      FOURx12, pre=[ts('II', -3, 4)], eq='tight', gate=-52, doubler=6)
    a("Burden (Solo)", 'Lead', "Watershed (2008) · a warm, 70s-style lead", A('tyche', Gain=7, Treble=5.5, Middle=7, Bass=5, Presence=5, Volume=7, Master=6),
      FOURx12, eq='lead', gate=-62, post=[tape(460, 28, 18), hall(3, 20)])

    # =====================================================================================
    a = artist('Mastodon')
    a("Blood and Thunder", 'Rhythm', "Leviathan (2004) · sludgy, heavy, rumbling", A('thanatos', Preamp=7, Bass=6, Middle=6, Treble=6, Presence=5.5, Depth=6, Master=5),
      FOURx12, eq='fuzz', gate=-54, doubler=7)
    a("Oblivion", 'Rhythm', "Crack the Skye (2009) · warm, proggy crunch", A('thanatos', Preamp=6, Bass=5.5, Middle=6, Treble=6, Presence=5.5, Depth=5, Master=5),
      FOURx12, gate=-56, doubler=6, post=[room(10)])
    a("The Motherload", 'Rhythm', "Once More 'Round the Sun (2014) · bright, punchy hard rock", A('moros', boost='Amp only', Gain=5.5, Bass=5.5, Mid=6, Treble=6, Level=5, Master=5, Tight=5, Air=5.5),
      FOURx12, gate=-54, doubler=6)
    a("Sleeping Giant", 'Lead', "Crack the Skye (2009) · psychedelic lead with phaser and echo", A('thanatos', Preamp=6.5, Bass=5, Middle=6.5, Treble=6, Presence=5.5, Depth=5, Master=5),
      FOURx12, pre=[phaser(0.4, 45, 25, '6', 35)], gate=-58, post=[dly(480, 32, 20, 'Tape'), hall(3, 20)])

    # =====================================================================================
    a = artist('Tool')
    a("Schism", 'Rhythm', "Lateralus (2001) · drop D, Diezel and Marshall layered", A('vulcan', channel='Ch 2 · crunch', voice='Crunch', Gain=6, Low=5.5, Mid=6, High=6, Presence=5.5, Depth=5.5, Master=5),
      FOURx12, gate=-56, doubler=8, width=125, post=[room(10)])
    a("Forty Six & 2", 'Rhythm', "Ænima (1996) · drop D, with a flanger on the build", A('vulcan', channel='Ch 3 · mega', voice='Potty Time', Gain=6, Low=5.5, Mid=5.5, High=6, Presence=5.5, Depth=5.5, Master=5),
      FOURx12, pre=[flanger(0.2, 50, 45, 2, 35)], gate=-54, doubler=7)
    a("Sober", 'Rhythm', "Undertow (1993) · drop D, dark and gritty", A('vulcan', channel='Ch 2 · crunch', voice='Crunch', Gain=6.5, Low=6, Mid=5.5, High=5.5, Presence=5, Depth=6, Master=5),
      FOURx12, gate=-56, doubler=7, post=[dly(400, 20, 10)])
    a("Lateralus", 'Rhythm', "Lateralus (2001) · drop D, huge, with echoes on the clean parts", A('vulcan', channel='Ch 3 · mega', voice='Physics', Gain=6, Low=6, Mid=5.5, High=6, Presence=5.5, Depth=6, Master=5),
      FOURx12, gate=-54, doubler=8, post=[sdly('1/8.', 25, 14), hall(2.8, 16)])

    # =====================================================================================
    a = artist('Muse')
    a("Plug In Baby", 'Lead', "Origin of Symmetry (2001) · fizzy fuzz riff with an octave", A('tyche', Gain=5.5, Treble=6.5, Middle=6, Bass=5, Presence=6, Volume=6.5, Master=6),
      FOURx12, pre=[octave(40, 0, 0, 95, 6.5), fuzz(8, 6.5, -4)], eq='fuzz', gate=-58)
    a("Hysteria", 'Rhythm', "Absolution (2003) · driving, saturated chords", A('tyche', Gain=6.5, Treble=6.5, Middle=6, Bass=5.5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[fuzz(6.5, 6, -4)], gate=-58, doubler=7)
    a("Knights of Cydonia", 'Lead', "Black Holes and Revelations (2006) · galloping, with a whammy on top", A('tyche', Gain=6.5, Treble=6.5, Middle=6, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[whammy(12, 45), fuzz(6, 6, -4)], gate=-58, post=[tape(360, 25, 18), hall(2.6, 18)])
    a("Supermassive Black Hole", 'Rhythm', "Black Holes and Revelations (2006) · tight, fuzzy, funky", A('tyche', Gain=5.5, Treble=6, Middle=6, Bass=5.5, Presence=5.5, Volume=6.5, Master=6),
      FOURx12, pre=[fuzz(7.5, 5, -4), fx('filter', Cutoff=1400, Resonance=3, Type='Low Pass', LFO_Rate=0.1, LFO_Depth=10, Mix=100)], gate=-58, doubler=6)

    # =====================================================================================
    a = artist('Rage Against the Machine')
    a("Killing in the Name", 'Rhythm', "Rage Against the Machine (1992) · drop D, a 50-watt Marshall, pure aggression", A('chrysaor', Gain=7, Bass=5.5, Middle=6.5, Treble=6, Presence=6, Master=5),
      FOURx12, gate=-56, doubler=6)
    a("Killing in the Name (Solo)", 'FX', "Rage Against the Machine (1992) · the whammy squeals, an octave up", A('chrysaor', Gain=7.5, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[whammy(12, 100)], gate=-56)
    a("Bulls on Parade", 'FX', "Evil Empire (1996) · the 'record scratch': a wah, rock it by hand", A('chrysaor', Gain=7, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[wah('Manual', pos=55, q=9)], gate=-54)
    a("Know Your Enemy", 'FX', "Rage Against the Machine (1992) · the octave-up whammy solo", A('chrysaor', Gain=7, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[whammy(12, 100)], gate=-56, post=[dly(300, 18, 10)])

    # =====================================================================================
    a = artist('Red Hot Chili Peppers')
    a("Californication", 'Clean', "Californication (1999) · gentle clean, neck pickup", M('glass65', gain=3, bass=5, mid=5, treble=6, presence=5.5),
      cab('2x12'), pre=[comp(-26, 3, 5)], post=[room(14)])
    a("Under the Bridge", 'Clean', "Blood Sugar Sex Magik (1991) · the intro chords, soft chorus", M('glass65', gain=3, bass=5, mid=5, treble=6.2, presence=5.5),
      cab('2x12'), post=[chorus(0.5, 30, 25), hall(2.4, 16)])
    a("Scar Tissue", 'Clean', "Californication (1999) · slide clean, squeezed and bright", M('glass65', gain=3.5, bass=4.5, mid=5.5, treble=6.5, presence=6),
      cab('2x12'), pre=[comp(-30, 6, 9, 80, 5)], post=[room(12)])
    a("Can't Stop", 'Crunch', "By the Way (2002) · muted funk, a little grit and a filter", A('tyche', Gain=4.5, Treble=6.5, Middle=6, Bass=5, Presence=6, Volume=6, Master=6),
      cab('2x12'), pre=[comp(-26, 5, 7), fx('bloom', Sensitivity=5, Range=5, Q=4, Type='Band Pass', Direction='Up', Mix=40)], post=[room(10)])
    a("Dani California (Solo)", 'Lead', "Stadium Arcadium (2006) · the fuzzy outro solo", A('tyche', Gain=6, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[fuzz(7, 6, -3)], post=[dly(420, 25, 16), room(12)])

    # =====================================================================================
    a = artist('Foo Fighters')
    a("Everlong", 'Rhythm', "The Colour and the Shape (1997) · drop D, big and wide", A('lamia', channel='BE', mic='Mic blend', Gain=6, Bass=5.5, Middle=6, Treble=6, Presence=5.5, Resonance=5, Master=5),
      gate=-58, doubler=9, width=130)
    a("The Pretender", 'Rhythm', "Echoes, Silence, Patience & Grace (2007) · driving, loud rock", A('lamia', channel='HBE', mic='Mic blend', Gain=6, Bass=5.5, Middle=6, Treble=6, Presence=6, Resonance=5, Master=5),
      gate=-56, doubler=8, width=125)
    a("Best of You", 'Rhythm', "In Your Honor (2005) · raw, wide chorus wall", A('lamia', channel='BE', mic='Mic blend', Gain=6.5, Bass=5.5, Middle=6, Treble=6, Presence=5.5, Resonance=5, Master=5),
      gate=-56, doubler=8, width=125)
    a("Learn to Fly", 'Crunch', "There Is Nothing Left to Lose (1999) · bright pop-rock crunch", A('lamia', channel='EL34', mic='SM57', Gain=5.5, Bass=5, Middle=6, Treble=8.5, Presence=8, Resonance=5, Master=5),
      gate=-60, doubler=7)

    # =====================================================================================
    a = artist('Sleep Token')
    a("The Summoning", 'Rhythm', "Take Me Back to Eden (2023) · djent riffs with a choir pad behind", A('umbra', channel='Channel 3', mode='Extreme', Gain=5.5, Treble=6.5, Middle=5, Bass=5, Presence=6, Master=5, Output=5),
      eq='djent', gate=-44, synth=S('Choir of the Damned', mix=25), doubler=6, width=125)
    a("Alkaline", 'Clean', "Sundowning (2019) · soft clean with a glassy pad", A('persephone', channel='Ch I', voice='Warm', Volume=4, Treble=6, Middle=5, Bass=5),
      synth=S('Glass Pad', mix=30), post=[sdly('1/8.', 30, 18), fx('elysium', Decay=6, Size=85, Pre_delay=40, Tone=6000, Low_Cut=160, Mod=5, Mix=26)], width=130)
    a("Granite", 'Rhythm', "Take Me Back to Eden (2023) · heavy, low, modern", A('nihil', amp='Blend of both', Master=5, Presence=6, Bass=5.5, Middle=5, Treble=6, Drive=6, Gain=6),
      eq='djent', gate=-44, doubler=6)
    a("Vore", 'Rhythm', "Take Me Back to Eden (2023) · the blackgaze-heavy one", A('typhon', channel='Red', Gain=6.5, Volume=5, Low=6, Mid=5, High=6, Presence=6, Resonance=6),
      eq='djent', gate=-44, doubler=7, post=[fx('halcyon', Mode='VERB', Mix=20, Decay=4)], width=130)

    # =====================================================================================
    a = artist('Spiritbox')
    a("Holy Roller", 'Rhythm', "Eternal Blue (2021) · extremely low and tight, with whammy dives", A('moros', boost='Blower Box', Gain=6, Bass=6, Mid=5, Treble=6, Level=5, Master=5, Tight=7, Air=5),
      FOURx12, pre=[whammy(-12, 30)], eq='djent', gate=-42, doubler=6)
    a("Circle with Me", 'Ambient', "Eternal Blue (2021) · airy clean with shimmer", A('dionysus', channel='Green · clean', voice='Bright', Gain=4, Bass=5, Middle=5, Treble=6, Presence=5.5, Master=5),
      post=[chorus(0.4, 30, 25), sdly('1/8.', 35, 22)], sfx=[fx('empyrean', Decay=10, Size=90, Shimmer=45, Voice_II=25, Interval='5TH', Tone=6000, Mod=6, Mix=30)], width=140)
    a("Constance", 'Clean', "Eternal Blue (2021) · gentle, sad clean", A('melinoe', mic='SM57 + 414', Volume=4, Treble=5.5, Bass=5, Middle=5, Reverb=2),
      post=[dly(520, 30, 18, 'Tape'), hall(3.6, 24)])

    # =====================================================================================
    a = artist('Architects')
    a("Doomsday", 'Rhythm', "Holy Hell (2018) · big, tight, emotional metalcore", A('typhon', channel='Red', Gain=6, Volume=5, Low=6, Mid=5, High=6, Presence=6, Resonance=6),
      pre=[ts('II', -3, 3)], eq='djent', gate=-44, doubler=7, width=125)
    a("Animals", 'Synth', "For Those That Wish to Exist (2021) · heavy groove with a synth behind it", A('typhon', channel='Red', Gain=6, Volume=5, Low=6, Mid=5, High=6, Presence=6, Resonance=6),
      eq='djent', gate=-44, synth=S('Reese Growl', mix=25), doubler=7)
    a("Hereafter", 'Rhythm', "Holy Hell (2018) · soaring, wide chorus guitars", A('moros', boost='Plumes M2', Gain=5.5, Bass=5.5, Mid=5.5, Treble=6, Level=5, Master=5, Tight=6, Air=6),
      FOURx12, eq='djent', gate=-44, doubler=8, width=130)

    # =====================================================================================
    a = artist('Bring Me the Horizon')
    a("Shadow Moses", 'Rhythm', "Sempiternal (2013) · heavy chorus wall", A('talos', Gain=6.5, Treble=6, Middle=5, Bass=6.5, Presence=6, Master=5, Output=5),
      eq='modern', gate=-46, doubler=8, width=130)
    a("Throne", 'Synth', "That's the Spirit (2015) · huge riff with a synth stab layer", A('talos', Gain=6, Treble=6, Middle=5, Bass=6, Presence=6, Master=5, Output=5),
      eq='modern', gate=-48, synth=S('G Stab', mix=30), doubler=8)
    a("Can You Feel My Heart", 'Synth', "Sempiternal (2013) · clean guitar and a lush synth pad", A('persephone', channel='Ch I', voice='Warm', Volume=4, Treble=6, Middle=5, Bass=5),
      synth=S('Glass Pad', mix=45), post=[sdly('1/8.', 30, 18), hall(3.4, 22)], width=130)

    # =====================================================================================
    a = artist('Ghost')
    a("Square Hammer", 'Synth', "Popestar (2016) · chunky riff with the organ following you", A('chrysaor', Gain=6.5, Bass=5, Middle=6.5, Treble=6, Presence=5.5, Master=5),
      FOURx12, gate=-58, synth=S('Haunted Organ', mix=35), doubler=7)
    a("Mary on a Cross", 'Crunch', "Seven Inches of Satanic Panic (2019) · 60s-style clean crunch", A('astraea', channel='Top Boost', Volume=6, Treble=6.5, Bass=5, Tone_Cut=4, Reverb=1.5, Master=6),
      cab('2x12'), post=[spring(16)])
    a("Cirice", 'Rhythm', "Meliora (2015) · slow, heavy and theatrical", A('chrysaor', Gain=7.5, Bass=6, Middle=6, Treble=6, Presence=5.5, Master=5),
      FOURx12, gate=-56, doubler=8, post=[hall(2.4, 14)])

    # =====================================================================================
    a = artist('Ozzy Osbourne')
    a("Crazy Train", 'Rhythm', "Blizzard of Ozz (1980) · Randy's Marshall pushed by a distortion pedal", A('tyche', Gain=7.5, Treble=6.5, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[fx('emperor', Mode='OD', Drive=2, Volume=3, Mix=100)], eq='mid', gate=-60, doubler=6, post=[room(10)])
    a("Mr. Crowley (Solo)", 'Lead', "Blizzard of Ozz (1980) · the neo-classical solo, chorus and echo", A('tyche', Gain=8, Treble=6.5, Middle=7, Bass=5, Presence=6, Volume=7, Master=6),
      FOURx12, pre=[fx('emperor', Mode='OD', Drive=3, Volume=3, Mix=100)], eq='lead', gate=-60, post=[chorus(0.6, 25, 20), dly(420, 28, 18), hall(2.6, 16)])
    a("No More Tears", 'Rhythm', "No More Tears (1991) · Zakk's heavy, pinch-harmonic crunch", A('chrysaor', Gain=8, Bass=5.5, Middle=6, Treble=6.5, Presence=6.5, Master=5),
      FOURx12, pre=[sd1('D III', -4, 5)], gate=-56, doubler=6)

    # =====================================================================================
    a = artist('Judas Priest')
    a("Painkiller", 'Rhythm', "Painkiller (1990) · fast, sharp, heavy metal", A('chrysaor', Gain=8.5, Bass=5.5, Middle=5.5, Treble=7, Presence=6.5, Master=5),
      FOURx12, pre=[sd1('D II', -3, 5)], eq='thrash', gate=-54, doubler=6)
    a("Breaking the Law", 'Crunch', "British Steel (1980) · classic, mid-gain British steel", A('chrysaor', Gain=6.5, Bass=5, Middle=6.5, Treble=6.5, Presence=6, Master=5),
      FOURx12, gate=-60, doubler=6)
    a("Electric Eye", 'Rhythm', "Screaming for Vengeance (1982) · tight galloping metal", A('chrysaor', Gain=7.5, Bass=5.5, Middle=6, Treble=6.5, Presence=6, Master=5),
      FOURx12, pre=[ts('I', -4, 4)], gate=-58, doubler=6)

    # =====================================================================================
    a = artist('Arctic Monkeys')
    a("Do I Wanna Know?", 'Rhythm', "AM (2013) · slow, thick, fuzzy riff", A('tyche', Gain=5, Treble=5.5, Middle=6.5, Bass=6, Presence=5, Volume=6, Master=6),
      FOURx12, pre=[fuzz(7, 4.5, -4)], eq='fuzz', doubler=8, width=125)
    a("R U Mine?", 'Rhythm', "AM (2013) · fuzz with an octave up, driving", A('tyche', Gain=5.5, Treble=6, Middle=6.5, Bass=5.5, Presence=5.5, Volume=6.5, Master=6),
      FOURx12, pre=[octave(35, 0, 0, 95, 6), fuzz(7.5, 5.5, -4)], eq='fuzz', doubler=7)
    a("505", 'Clean', "Favourite Worst Nightmare (2007) · moody clean with spring reverb", A('melinoe', mic='SM57', Volume=4.5, Treble=6, Bass=5, Middle=5, Reverb=4),
      post=[spring(20)])

    # =====================================================================================
    a = artist('The Beatles')
    a("Revolution", 'Rhythm', "Revolution (1968) · distortion straight into the desk: fizzy and brutal", A('astraea', channel='Top Boost', Volume=6, Treble=6.5, Bass=5, Tone_Cut=4, Reverb=0, Master=6),
      cab('2x12'), pre=[fuzz(9, 7, -5)], eq='lofi', doubler=5)
    a("Something (Solo)", 'Lead', "Abbey Road (1969) · warm, melodic, slightly broken-up", A('astraea', channel='Normal', Volume=6.5, Treble=5.5, Bass=5, Tone_Cut=5, Reverb=1, Master=6),
      cab('2x12'), post=[plate(2, 16)])
    a("Here Comes the Sun", 'Acoustic', "Abbey Road (1969) · bright capo'd acoustic", A('orpheus', take='Take 1', Gain=4, Bass=4.5, Middle=5, Treble=6.5, Body=5.5, Reverb=1, Volume=5),
      eq='acoustic', post=[room(14)])
    a("Day Tripper", 'Crunch', "Day Tripper (1965) · jangly, biting riff", A('astraea', channel='Top Boost', Volume=6.5, Treble=7, Bass=4.5, Tone_Cut=3.5, Reverb=0, Master=6),
      cab('2x12'), post=[room(10)])

    # =====================================================================================
    a = artist('Eagles')
    a("Hotel California (Solo)", 'Lead', "Hotel California (1976) · warm, singing dual-lead tone", A('tyche', Gain=6.5, Treble=5.5, Middle=7, Bass=5, Presence=5, Volume=6.5, Master=6),
      cab('2x12'), eq='lead', gate=-62, post=[dly(420, 25, 18, 'Tape'), plate(2.4, 16)])
    a("Life in the Fast Lane", 'Crunch', "Hotel California (1976) · tight 70s rock riff", A('tyche', Gain=6, Treble=6.5, Middle=6.5, Bass=5, Presence=6, Volume=6.5, Master=6),
      cab('2x12'), gate=-62, doubler=5, post=[room(10)])

    # =====================================================================================
    a = artist('Santana')
    a("Europa", 'Lead', "Amigos (1976) · endless, violin-like sustain", A('hyperion', channel='Overdrive', voice='Smooth', Volume=5.5, Treble=5, Middle=6.5, Bass=5, Presence=5, Drive=7.5, Master=5),
      ONEx12, gate=-62, post=[dly(480, 28, 18, 'Tape'), hall(3.2, 22)])
    a("Black Magic Woman", 'Lead', "Abraxas (1970) · smooth, vocal lead", A('hyperion', channel='Overdrive', voice='Ford 2', Volume=5.5, Treble=5, Middle=6.5, Bass=5, Presence=5, Drive=6.5, Master=5),
      ONEx12, gate=-62, post=[plate(2.4, 18)])
    a("Smooth", 'Lead', "Supernatural (1999) · Mesa-style singing lead", A('umbra', channel='Channel 2', mode='Mark I', Gain=6.5, Treble=5.5, Middle=6.5, Bass=5, Presence=5.5, Master=5, Output=5),
      gate=-60, post=[dly(380, 20, 14), plate(2.2, 16)])

    # =====================================================================================
    a = artist('Gary Moore')
    a("Still Got the Blues", 'Lead', "Still Got the Blues (1990) · thick, crying blues-rock lead", A('chrysaor', Gain=7.5, Bass=5, Middle=7, Treble=6, Presence=5.5, Master=5),
      FOURx12, pre=[ts('I', -3, 4)], eq='lead', gate=-62, post=[dly(460, 28, 18), hall(3, 20)])
    a("Parisienne Walkways", 'Lead', "Back on the Streets (1978) · the endless held note", A('thanatos', Preamp=7, Bass=5, Middle=7, Treble=5.5, Presence=5, Depth=5, Master=5),
      FOURx12, eq='lead', gate=-62, post=[dly(520, 32, 20, 'Tape'), hall(3.4, 22)])
