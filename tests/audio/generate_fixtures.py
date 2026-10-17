"""Generate original synthetic fixtures. No copyrighted media is downloaded."""
from pathlib import Path
import array, math, subprocess, sys, wave

root = Path(__file__).resolve().parents[2]
out = root / 'out/audio-fixtures'
out.mkdir(parents=True, exist_ok=True)
ffmpeg = root / 'out/audio-deps/ffmpeg/ffmpeg-n8.1-latest-win64-lgpl-shared-8.1/bin/ffmpeg.exe'
def run(*args):
    subprocess.run([str(ffmpeg), '-hide_banner', '-loglevel', 'error', '-y', *map(str,args)], check=True)
def tone(name, rate, amp=.1, channels=2, frequency=1000):
    data=array.array('h')
    for i in range(rate*3):
        for c in range(channels):
            data.append(round(32767*amp*math.sin(2*math.pi*(frequency if channels<=2 else (300+c*150))*i/rate)))
    with wave.open(str(out/name),'wb') as f:
        f.setnchannels(channels); f.setsampwidth(2);f.setframerate(rate);f.writeframes(data.tobytes())
for rate in (44100,48000,96000): tone(f'stereo-{rate}.wav',rate)
tone('quiet.wav',48000,.1);tone('loud.wav',48000,.4)
tone('surround.wav',48000,.05,6)
tone('surround-8.wav',48000,.05,8)
# Write an explicit channel-layout mask into the multichannel WAV using FFmpeg.
run('-i',out/'surround.wav','-channel_layout','5.1','-c:a','pcm_s16le',out/'surround-51.wav')
run('-i',out/'surround-8.wav','-channel_layout','7.1','-c:a','pcm_s16le',out/'surround-71.wav')
run('-i',out/'quiet.wav','-c:a','flac','-metadata','replaygain_track_gain=-3.00 dB',out/'replaygain.flac')
for suffix,codec in [('mp3','libmp3lame'),('flac','flac'),('m4a','aac')]:
    run('-i',out/'stereo-48000.wav','-c:a',codec,out/f'stereo.{suffix}')
(out/'corrupt.bin').write_bytes(b'this is not audio\x00\xff'*30)
src=(out/'stereo.flac').read_bytes();(out/'truncated.flac').write_bytes(src[:len(src)//3])
run('-i',out/'stereo-48000.wav','-c:a','libopus',
    '-stream_group','type=iamf_audio_element:id=1:st=0,layer=ch_layout=stereo',
    '-stream_group','type=iamf_mix_presentation:id=2:stg=0:annotations=en-us=Synthetic,submix=parameter_id=100:parameter_rate=48000|element=stg=0:parameter_id=100:annotations=en-us=Synthetic|layout=sound_system=stereo',
    '-streamid','0:0',out/'stereo.iamf')
tone('iamf-front.wav',48000,.05,2,440)
tone('iamf-back.wav',48000,.05,2,660)
tone('iamf-center.wav',48000,.05,1,880)
tone('iamf-lfe.wav',48000,.05,1,60)
run('-i',out/'iamf-front.wav','-i',out/'iamf-back.wav','-i',out/'iamf-center.wav','-i',out/'iamf-lfe.wav',
    '-map','0:0','-map','1:0','-map','2:0','-map','3:0','-c:a','libopus',
    '-stream_group','type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3,demixing=parameter_id=998,recon_gain=parameter_id=101,layer=ch_layout=stereo,layer=ch_layout=5.1(side)',
    '-stream_group','type=iamf_mix_presentation:id=2:stg=0:annotations=en-us=Synthetic51,submix=parameter_id=100:parameter_rate=48000|element=stg=0:parameter_id=100:annotations=en-us=Synthetic51|layout=sound_system=stereo|layout=sound_system=5.1(side)',
    '-streamid','0:0','-streamid','1:1','-streamid','2:2','-streamid','3:3',out/'surround.iamf')
print(out)
